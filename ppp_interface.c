#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <assert.h>

#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h>

#include "ppp_interface.h"
#include "hdlc.h"
#include "ppp_lcp.h"
#include "ppp_ip6cp.h"
#include "virtual_timer.h"

uint8_t m_timer_init_flag = 0;
uint8_t g_debug_level = DEBUG_LEVEL_2;

void lcp_module_up(ppp_t *ppp) {
  if (g_debug_level >= DEBUG_LEVEL_3) 
    fprintf(stdout, "LCP Layer up ...\n");

  ip6cp_module_start(ppp);
}

void ip6cp_module_up(ppp_t *ppp) {
  ppp->state = PPP_OPENED;

  if (g_debug_level >= DEBUG_LEVEL_3) 
    fprintf(stdout, "IP6CP Layer up ...\n");

  if (ppp->ppp_up != NULL)
    ppp->ppp_up(ppp);
}

void lcp_module_down(ppp_t *ppp) {
  if (g_debug_level >= DEBUG_LEVEL_3) 
    fprintf(stdout, "LCP Layer down ...\n");

  ip6cp_module_stop(ppp);
}

void ip6cp_module_down(ppp_t *ppp) {
  ppp->state = PPP_CLOSED;

  if (g_debug_level >= DEBUG_LEVEL_3) 
    fprintf(stdout, "IP6CP Layer down ...\n");

  if (ppp->ppp_down != NULL)
    ppp->ppp_down(ppp);
}

ppp_t *ppp_interface_create(char *name, int id,
                    void (*ppp_recv)(ppp_t *ppp, uint8_t *buf, uint16_t len),
                    int (*raw_send)(int id, uint8_t *buf, uint16_t len),
                    void (*ppp_up)(ppp_t *ppp),
                    void (*ppp_down)(ppp_t *ppp)) {

  ppp_t *ppp;
  if ((name == NULL) ||
      (raw_send == NULL) || (ppp_recv == NULL)) {
    return NULL;
  }

  if ((ppp = (ppp_t*)malloc(sizeof(ppp_t))) == NULL)
    return NULL;

  if (!m_timer_init_flag) {
    timer_init();
    m_timer_init_flag = 1;
  }

  snprintf(ppp->dev_name, sizeof(ppp->dev_name), "%s", name);

  ppp->state = PPP_CLOSED;
  ppp->ppp_recv = ppp_recv;
  ppp->ppp_up = ppp_up;
  ppp->ppp_down = ppp_down;

  hdlc_init(ppp, id, raw_send);
  lcp_init(ppp, lcp_module_up, lcp_module_down);
  ip6cp_init(ppp, ip6cp_module_up, ip6cp_module_down);

  return ppp;
}

void ppp_interface_free(ppp_t *ppp) {
  if (ppp == NULL)
    return;

  free(ppp);
}

int ppp_interface_raw_recv(ppp_t *ppp, uint8_t *buf, uint16_t len) {
  return ppp->dev.raw_recv((void*)ppp, buf, len);
}

int ppp_interface_enable(ppp_t *ppp) {
  ppp->state = PPP_OPENING;
  return lcp_module_start(ppp);
}

int ppp_interface_disable(ppp_t *ppp) {
  ppp->state = PPP_CLOSING;
  return lcp_module_stop(ppp);
}

int ppp_ip6_received(ppp_t *ppp, uint8_t *buf, uint16_t len) {
  ppp->ppp_recv(ppp, buf, len);
  return 0;
}

int ppp_interface_send(ppp_t *ppp, uint8_t *buf, uint16_t len) {
  if (ppp->state != PPP_OPENED)
    return -1;

  return hdlc_send(ppp, PPP_PROTO_IP6, buf, len);
}

void send_proto_reject(ppp_t *ppp) {
  struct lcp_context *context = &ppp->lcp.context;

  context->rep_buf[0] = LCP_PROTREJ;
  context->rep_buf[1] = context->req_id++;
  context->rep_buf[2] = 0;
  context->rep_buf[3] = 6;
  context->rep_buf[4] = ppp->proto >> 8;
  context->rep_buf[5] = ppp->proto >> 0;
  context->rep_len = 6;

  hdlc_send_msg(context);
}

int hdlc_send_msg(struct lcp_context *context) {
  if (context->rep_len) {
    hdlc_send(context->ppp, context->proto, context->rep_buf, context->rep_len);
    context->rep_len = 0;
  }

  if (context->req_len) {
     hdlc_send(context->ppp, context->proto, context->req_buf, context->req_len);
     context->req_len = 0;
  }

  return 0;
}

