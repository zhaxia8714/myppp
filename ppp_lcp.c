#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "ppp_interface.h"
#include "ppp_engine.h"
#include "ppp_ip6cp.h"
#include "virtual_timer.h"

  void receive_echo_request(lcp_module_t *lcp, uint8_t *buf, uint16_t len);
  void receive_echo_reply(lcp_module_t *lcp, uint8_t *buf, uint16_t len);
  void send_echo_reply(lcp_module_t *lcp, uint8_t *buf, uint16_t len);

  int lcp_module_start(ppp_t *ppp) {
    return ppp_engine_upper_layer_started(&ppp->lcp.context);
  }

  int lcp_module_stop(ppp_t *ppp) {
    return ppp_engine_upper_layer_finished(&ppp->lcp.context);
  }

  int lcp_received(ppp_t *ppp, uint8_t *buf, uint16_t len) {
    struct lcphdr *lcphdr = (struct lcphdr*)buf;
    if (len < sizeof(struct lcphdr))
      return -1;

    switch (lcphdr->code) {
    case LCP_ECHOREQ:
      receive_echo_request(&ppp->lcp, buf, len);
      break;

    case LCP_ECHOREP:
      receive_echo_reply(&ppp->lcp, buf, len);
      break;

    default:
      ppp_engine_process(&ppp->lcp.context, buf, len);
      break;
    }

    return 0;
  }

  int lcp_this_layer_up(struct lcp_context *context) {
    lcp_module_t *lcp = &context->ppp->lcp;

    timer_start_one_shot(lcp->echo_timer, PPP_ECHO_PERIOD);
    lcp->echo_attempts = 0;

    if (lcp->module_up != NULL)
      lcp->module_up(context->ppp);

    return 0;
  }

  int lcp_this_layer_down(struct lcp_context *context) {
    lcp_module_t *lcp = &context->ppp->lcp;

    timer_stop(lcp->echo_timer);
    lcp->echo_attempts = 0;

    if (lcp->module_down != NULL)
      lcp->module_down(context->ppp);

    return 0;
  }

  int lcp_this_layer_started(struct lcp_context *context) {
    context = context; // to avoid warning
    return 0;
  }

  int lcp_this_layer_finished(struct lcp_context *context) {
    context = context; // to avoid warning
    return 0;
  }

  int lcp_receive_proto_reject(struct lcp_context *context) {
    context = context; // to avoid warning
    return 0;
  }

  int lcp_send_configure_request(struct lcp_context *context) {
    struct lcpopt *opt;

    if (context->req_len) {
      return -1;
    } 

    context->req_buf[0] = LCP_CONFREQ;
    context->req_buf[1] = context->req_id++;
    context->req_len = sizeof(struct lcphdr);

    opt = (struct lcpopt*)(context->req_buf + sizeof(struct lcphdr));

    // async map opt
    opt->type = LCP_CI_ASYNCMAP;
    opt->len = sizeof(struct lcpopt) + 4;
    memset((uint8_t*)opt + 2, 0, 4);
    context->req_len += opt->len;
    opt = (struct lcpopt*)((uint8_t*)opt + opt->len);

    // proto compression opt
    opt->type = LCP_CI_PCOMPRESSION;
    opt->len = sizeof(struct lcpopt);
    context->req_len += opt->len;
    opt = (struct lcpopt*)((uint8_t*)opt + opt->len);

    // addr/control field compression opt
    opt->type = LCP_CI_ACCOMPRESSION;
    opt->len = sizeof(struct lcpopt);
    context->req_len += opt->len;
    opt = (struct lcpopt*)((uint8_t*)opt + opt->len);

    context->req_buf[2] = context->req_len >> 8;
    context->req_buf[3] = context->req_len >> 0;

    return 0;
  }

  int lcp_send_configure_ack(struct lcp_context *context,
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
      case LCP_CI_ASYNCMAP:
        break;
      case LCP_CI_MAGICNUMBER:
        break;
      case LCP_CI_PCOMPRESSION:
        break;
      case LCP_CI_ACCOMPRESSION:
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

  int lcp_start_timeout(struct lcp_context *context) {
    return timer_start_one_shot(context->ppp->lcp.timeout_timer,
                                PPP_REPLY_TIMEOUT);
  }

  int lcp_stop_timeout(struct lcp_context *context) {
    return timer_stop(context->ppp->lcp.timeout_timer);
  }

  void timeout_timer_fired(void* data) {
    ppp_engine_timeout(&((lcp_module_t*)data)->context);
  }

  void receive_echo_request(lcp_module_t *lcp, uint8_t *buf, uint16_t len) {
    if (lcp->context.state != LCP_STATE_OPENED)
      return;

    send_echo_reply(lcp, buf, len);
  }

  void receive_echo_reply(lcp_module_t *lcp, uint8_t *buf, uint16_t len) {
    if (lcp->context.state != LCP_STATE_OPENED)
      return;

    timer_start_one_shot(lcp->echo_timer, PPP_ECHO_PERIOD);
    lcp->echo_attempts = 0;

    buf = buf;  // to avoid warning
    len = len;  // to avoid warning
  }

  void send_echo_request(struct lcp_context *context) {
    if (context->req_len)
      return;

    context->req_buf[0] = LCP_ECHOREQ;
    context->req_buf[1] = context->req_id++;
    context->req_buf[2] = 0;
    context->req_buf[3] = 8;
    context->req_buf[4] = 0;
    context->req_buf[5] = 0;
    context->req_buf[6] = 0;
    context->req_buf[7] = 0;
    context->req_len = 8;

    hdlc_send_msg(context);
  }

  void send_echo_reply(lcp_module_t *lcp, uint8_t *buf, uint16_t len) {
    struct lcp_context *context = &lcp->context;

    if (context->rep_len)
      return;

    memcpy(context->rep_buf, buf, len);
    context->rep_buf[0] = LCP_ECHOREP;
    context->rep_len = len;
    memset(context->rep_buf + 4, 0, 4);

    hdlc_send_msg(context);
  }

  void echo_timer_fired(void *data) {
    lcp_module_t *lcp = (lcp_module_t *)data;

    if (lcp->context.state != LCP_STATE_OPENED)
      return;

    if (lcp->echo_attempts < PPP_ECHO_ATTEMPTS) {
      send_echo_request(&lcp->context);
      lcp->echo_attempts++;
      timer_start_one_shot(lcp->echo_timer, PPP_ECHO_TIMEOUT);
    } else {
      lcp->context.state = LCP_STATE_STOPPED;
      timer_stop(lcp->timeout_timer);
      lcp_this_layer_down(&lcp->context);
    }
  }

  int lcp_init(ppp_t *ppp, void (*up)(ppp_t *ppp), void (*down)(ppp_t *ppp)) {
    lcp_module_t *lcp = &ppp->lcp;
    struct lcp_context *context = &lcp->context;

    context->proto = PPP_PROTO_LCP;
    context->state = LCP_STATE_CLOSED;
    context->ppp = ppp;
    context->req_len = 0;

    context->receive_proto_reject = lcp_receive_proto_reject;
    context->send_configure_request = lcp_send_configure_request;
    context->send_configure_ack = lcp_send_configure_ack;
    context->start_timeout = lcp_start_timeout;
    context->stop_timeout = lcp_stop_timeout;
    context->this_layer_up = lcp_this_layer_up;
    context->this_layer_down = lcp_this_layer_down;
    context->this_layer_started = lcp_this_layer_started;
    context->this_layer_finished = lcp_this_layer_finished;

    if ((lcp->echo_timer = timer_new(echo_timer_fired, (void*)lcp)) < 0)
      return -1;

    if ((lcp->timeout_timer = timer_new(timeout_timer_fired, (void*)lcp)) < 0) {
      timer_distroy(lcp->echo_timer);
      return -1;
    }

    lcp->module_up = up;
    lcp->module_down = down;

    return 0;
  }

