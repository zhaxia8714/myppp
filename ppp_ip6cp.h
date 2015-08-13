#ifndef __PPP_IP6CP_H__
#define __PPP_IP6CP_H__
#include "ppp_interface.h"

int ip6cp_init(ppp_t *ppp, void (*up)(ppp_t *ppp), void (*down)(ppp_t *ppp));

int ip6cp_module_start(ppp_t *ppp);
int ip6cp_module_stop(ppp_t *ppp);

void ip6cp_received(ppp_t *ppp, uint8_t *buf, uint16_t len);

#endif
