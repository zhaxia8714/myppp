
#ifndef CRC_H
#define CRC_H
#include <stdint.h>

#define CRCINIT16    0xffff  /* Initial FCS value */
#define CRCGOOD16    0xf0b8  /* Good final FCS value */

uint16_t crc16(uint16_t fcs, uint8_t *cp, uint16_t len);

#endif
