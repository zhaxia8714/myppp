/*
 * Copyright (c) 2014 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "ppp_interface.h"
#include "ppp_engine.h"
#include "virtual_timer.h"

  int ip6cp_module_start(ppp_t *ppp) {
    int rval;

    if ((rval = ppp_engine_lower_layer_up(&ppp->ip6cp.context)) < 0)
      return rval;

    return rval;
  }

  int ip6cp_module_stop(ppp_t *ppp) {
    return ppp_engine_lower_layer_down(&ppp->ip6cp.context);
  }

  void ip6cp_received(ppp_t *ppp, uint8_t *buf, uint16_t len) {
    ppp_engine_process(&ppp->ip6cp.context, buf, len);
  }

  int ip6cp_this_layer_up(struct lcp_context *context) {
    ip6cp_module_t *ip6cp = &context->ppp->ip6cp;

    if (ip6cp->module_up != NULL)
      ip6cp->module_up(context->ppp);

    return 0;
  }

  int ip6cp_this_layer_down(struct lcp_context *context) {
    ip6cp_module_t *ip6cp = &context->ppp->ip6cp;

    if (ip6cp->module_down != NULL)
      ip6cp->module_down(context->ppp);

    return 0;
  }

  int ip6cp_this_layer_started(struct lcp_context *context) {
    context = context;
    return 0;
  }

  int ip6cp_this_layer_finished(struct lcp_context *context) {
    context = context;
    return 0;
  }

  int ip6cp_receive_proto_reject(struct lcp_context *context) {
    context = context;
    return 0;
  }

  int ip6cp_send_configure_request(struct lcp_context *context) {
    struct lcpopt *opt;
    uint8_t eui64[8] = {0, 0, 0, 0, 0, 0, 0, 0x02};

    if (context->req_len)
      return -1;

    context->req_buf[0] = LCP_CONFREQ;
    context->req_buf[1] = context->req_id++;
    context->req_len = sizeof(struct lcphdr);

    opt = (struct lcpopt*)(context->req_buf + sizeof(struct lcphdr));

    // interfaceid opt
    opt->type = IPV6CP_CI_INTERFACEID;
    opt->len = 10;
    memcpy((uint8_t*)opt + 2, eui64, 8);
    context->req_len += opt->len;
    opt = (struct lcpopt*)((uint8_t*)opt + opt->len);

    context->req_buf[2] = context->req_len >> 8;
    context->req_buf[3] = context->req_len >> 0;

    return 0;
  }

  int ip6cp_send_configure_ack(struct lcp_context *context,
                               uint8_t *buf, uint16_t len) {
    struct lcphdr *lcphdr = (struct lcphdr*)buf;
    struct lcpopt *opt;
    uint16_t replen;

    if (context->rep_len)
      return -1;

    replen = sizeof(struct lcphdr);

    opt = (struct lcpopt*)(buf + sizeof(struct lcphdr));
    while ((uint8_t*)opt < buf + len) {
      if (opt->len == 0)
        break;

      switch (opt->type) {
      case IPV6CP_CI_INTERFACEID:
        memcpy((uint8_t*)&context->ppp->remote_eui64, (uint8_t*)opt + 2, 8);
        break;
      default:
        memcpy(context->rep_buf + replen, opt, opt->len);
        replen += opt->len;
        break;
      }

      opt = (struct lcpopt*)((uint8_t*)opt + opt->len);
    }

    if (replen > sizeof(struct lcphdr)) {
      context->rep_buf[0] = LCP_CONFREJ;
      context->rep_buf[1] = lcphdr->id;
      context->rep_buf[2] = replen >> 8;
      context->rep_buf[3] = replen >> 0;
      context->rep_len = replen;
    } else {
      memcpy(context->rep_buf, buf, len);
      context->rep_buf[0] = LCP_CONFACK;
      context->rep_len = len;
    }

    return (context->rep_buf[0] == LCP_CONFACK) ? 0 : -1;
  }

  int ip6cp_start_timeout(struct lcp_context *context) {
    return timer_start_one_shot(context->ppp->ip6cp.timeout_timer,
                                PPP_REPLY_TIMEOUT);
  }

  int ip6cp_stop_timeout(struct lcp_context *context) {
    return timer_stop(context->ppp->ip6cp.timeout_timer);
  }

  void ip6cp_timeout_timer_fired(void *data) {
    ppp_engine_timeout(&((ip6cp_module_t*)data)->context);
  }

  int ip6cp_init(ppp_t *ppp, void (*up)(ppp_t *ppp), void (*down)(ppp_t *ppp)) {
    ip6cp_module_t *ip6cp = &ppp->ip6cp;
    struct lcp_context *context = &ip6cp->context;

    context->proto = PPP_PROTO_IP6CP;
    context->state = LCP_STATE_STARTING;
    context->ppp = ppp;
    context->req_len = 0;

    context->receive_proto_reject = ip6cp_receive_proto_reject;
    context->send_configure_request = ip6cp_send_configure_request;
    context->send_configure_ack = ip6cp_send_configure_ack;
    context->start_timeout = ip6cp_start_timeout;
    context->stop_timeout = ip6cp_stop_timeout;
    context->this_layer_up = ip6cp_this_layer_up;
    context->this_layer_down = ip6cp_this_layer_down;
    context->this_layer_started = ip6cp_this_layer_started;
    context->this_layer_finished = ip6cp_this_layer_finished;

    if ((ip6cp->timeout_timer = timer_new(ip6cp_timeout_timer_fired, ip6cp)) < 0)
      return -1;

    ip6cp->module_up = up;
    ip6cp->module_down = down;

    return 0;
  }
