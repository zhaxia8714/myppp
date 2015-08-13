#ifndef __HDLC_H__
#define __HDLC_H__

#include "ppp_interface.h"

void hdlc_init(struct ppp *ppp, int id, int (*raw_send)(int id, uint8_t *buf, uint16_t len));
int hdlc_send(struct ppp *ppp, uint16_t proto, uint8_t *buf, uint32_t len);

#endif
