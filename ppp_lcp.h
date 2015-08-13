#ifndef __PPP_LCP_H__
#define __PPP_LCP_H__
#include "ppp_interface.h"

int lcp_init(ppp_t *ppp, void (*up)(ppp_t *ppp), void (*down)(ppp_t *ppp));

int lcp_module_start(ppp_t *ppp);
int lcp_module_stop(ppp_t *ppp);

int lcp_received(ppp_t *ppp, uint8_t *buf, uint16_t len);

#endif
