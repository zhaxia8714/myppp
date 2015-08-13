#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "crc.h"
#include "ppp_interface.h"
#include "ppp_lcp.h"
#include "ppp_ip6cp.h"

void print_send(uint16_t proto, uint8_t code);
void print_recv(uint16_t proto, uint8_t code);

extern uint8_t g_debug_level;

char m_code_str[][20] = {"",
                         "CONFREQ", //1
                         "CONFACK", //2
                         "CONFNAK", //3
                         "CONFREJ", //4
                         "TERMREQ", //5
                         "TERMACK", //6
                         "CODEREJ", //7
                         "PROTREJ", //8
                         "ECHOREQ", //9
                         "ECHOREP", //10
                         "DISCREQ", //11
                         "IDENTIF", //12
                         "TIMEREM", //13
                         "RESTREQ", //14
                         "RESTACK", //15
                         "UNKNOWN"  //16
                         };

int hdlc_recv(void *handle, uint8_t *buf, uint16_t len);

void hdlc_init(struct ppp *ppp, int id, int (*raw_send)(int id, uint8_t *buf, uint16_t len)) {
  device_t *dev = &ppp->dev;
 
  dev->id = id;

  dev->tx_len = 0;
  dev->tx_crc = CRCINIT16;

  dev->rx_state = HDLC_STATE_NO_SYNC;
  dev->rx_crc = CRCINIT16;
  dev->rx_state = HDLC_STATE_NO_SYNC;

  dev->raw_recv = hdlc_recv;
  dev->raw_send = raw_send;
}

int frame_recv(ppp_t *ppp, uint8_t *buf, uint16_t len) {
  uint16_t proto;
  uint8_t code;
  uint8_t *cur;

  // strip Address and Control
  if ((buf[0] != 0xff) || (buf[1] != 0x03)) {
    return -1;
  }

  proto = buf[2];
  if ((proto & 0x01) == 0)
    proto = (proto << 8) | buf[3];

  ppp->proto = proto;
  code = buf[4];

  if (g_debug_level >= DEBUG_LEVEL_3)
    print_recv(proto, code);

  switch (proto) {
    case PPP_PROTO_LCP:
      lcp_received(ppp, buf + 4, len - 4);
      break;

    case PPP_PROTO_IP6CP:
      ip6cp_received(ppp, buf + 4, len - 4);
      break;

    case PPP_PROTO_IP6:
      ppp_ip6_received(ppp, buf + 4, len - 4);
      break;

    default:
      send_proto_reject(ppp);
      break;
  }

  return 0;
}

int append_tx_byte(device_t *dev, uint8_t byte) {
  if (dev->tx_len >= (sizeof(dev->tx_buf) - 1)) {
    return -1;
  }

  if (byte == HDLC_CONTROL_ESCAPE ||
      byte == HDLC_FLAG_SEQUENCE) {
    dev->tx_buf[dev->tx_len++] = HDLC_CONTROL_ESCAPE;
    byte ^= 0x20;
  }

  dev->tx_buf[dev->tx_len++] = byte;
  return 0;
}

int append_tx_byte_crc(device_t *dev, uint8_t byte) {
  dev->tx_crc = crc16(dev->tx_crc, &byte, 1);
  return append_tx_byte(dev, byte);
}

int append_rx_byte(device_t *dev, uint8_t byte) {

  if (dev->rx_len >= sizeof(dev->rx_buf)) {
    dev->rx_state = HDLC_STATE_NO_SYNC;
    return -1;
  }

  dev->rx_buf[dev->rx_len] = byte;
  dev->rx_crc = crc16(dev->rx_crc, &byte, 1);
  dev->rx_len++;

  return 0;
}

int hdlc_recv(void *handle, uint8_t *buf, uint16_t len) {
  ppp_t *ppp = (ppp_t*)handle;
  device_t *dev = &ppp->dev;
  uint32_t i;
  uint8_t byte;

  if (g_debug_level == DEBUG_LEVEL_4) {
    printf("raw recv:");
    for (i = 0; i < len; i++)
      printf("%2x ", buf[i]);
    printf("\n");
  }

  for (i = 0; i < len; i++) {
    byte = buf[i];
 
    switch (dev->rx_state) {
      case HDLC_STATE_NO_SYNC:
        if (byte == HDLC_FLAG_SEQUENCE) {
          dev->rx_state = HDLC_STATE_SYNC;
          dev->rx_len = 0;
          dev->rx_crc = CRCINIT16;
        }
        break;

      case HDLC_STATE_SYNC:
        switch (byte) {
          case HDLC_CONTROL_ESCAPE:
            dev->rx_state = HDLC_STATE_ESCAPED;
          break;

          case HDLC_FLAG_SEQUENCE:
            if (dev->rx_len) {
              // verify CRC and strip CRC
              if ((dev->rx_crc != CRCGOOD16) || (dev->rx_len < 2)) {
                dev->rx_len = 0;
                dev->rx_crc = CRCINIT16;
                dev->rx_state = HDLC_STATE_NO_SYNC;
                break;
              }

              dev->rx_len -= 2;
              dev->rx_state = HDLC_STATE_NO_SYNC;

              frame_recv(ppp, dev->rx_buf, dev->rx_len);
            }
          break;

          default:
            append_rx_byte(dev, byte);
          break;
        }
      break;

      case HDLC_STATE_ESCAPED:
        if (append_rx_byte(dev, byte ^ 0x20) < 0)
          continue;
        dev->rx_state = HDLC_STATE_SYNC;
      break;

      default:
        dev->rx_state = HDLC_STATE_NO_SYNC;
      break;
    }
  }

  return -1;
}

int hdlc_send(ppp_t *ppp, uint16_t proto, uint8_t *buf, uint32_t len) {
  uint32_t i;
  device_t *dev = &ppp->dev;

  if ((dev == NULL) || (dev->raw_send == NULL))
    return -1;

  dev->tx_crc = CRCINIT16;
  dev->tx_len = 0;

  // flag
  dev->tx_buf[dev->tx_len++] = HDLC_FLAG_SEQUENCE;

  // address
  append_tx_byte_crc(dev, 0xff);

  // control
  append_tx_byte_crc(dev, 0x03);

  // proto
  append_tx_byte_crc(dev, proto >> 8);
  append_tx_byte_crc(dev, proto >> 0);

  for (i = 0; i < len; i++) {
    if (append_tx_byte_crc(dev, buf[i]) < 0) {
      return -1;
    }
  }

  dev->tx_crc ^= 0xffff;

  if (append_tx_byte(dev, dev->tx_crc >> 0) < 0) {
    return -1;
  }

  if (append_tx_byte(dev, dev->tx_crc >> 8) < 0) {
    return -1;
  }

  if (dev->tx_len >= sizeof(dev->tx_buf)) {
    return -1;
  }

  dev->tx_buf[dev->tx_len++] = HDLC_FLAG_SEQUENCE;

  if (g_debug_level == DEBUG_LEVEL_4) {
    printf("\nraw send:");
    for (i = 0; i < dev->tx_len; i++)
      printf("%02x ", dev->tx_buf[i]);
    printf("\n");
  }

  if (g_debug_level >= DEBUG_LEVEL_3)
    print_send(proto, buf[0]);

  return (*dev->raw_send)(dev->id, dev->tx_buf, dev->tx_len);
}

void print_send(uint16_t proto, uint8_t code) {
  if (code >= 16) {
    code = 16;
  }

  switch (proto) {
    case PPP_PROTO_LCP:
      printf("Send LCP %s\n", m_code_str[code]);
      break;

    case PPP_PROTO_ECP:
      printf("Send ECP %s\n", m_code_str[code]);
      break;

    case PPP_PROTO_ENC:
      printf("Send ENC %s\n", m_code_str[code]);
      break;

    case PPP_PROTO_IP6CP:
      printf("Send IP6CP %s\n", m_code_str[code]);
      break;

    case PPP_PROTO_IP6:
      printf("Send IP6 Packet\n");
      break;

    default:
      printf("Send unknown packet\n");
      break;
  }
}

void print_recv(uint16_t proto, uint8_t code) {
  if ((code > 15) && (proto != PPP_PROTO_IP6)) {
    printf("Recv unknown ppp code:%d\n", code);
    return;
  }

  switch (proto) {
    case PPP_PROTO_LCP:
      printf("Recv LCP %s\n", m_code_str[code]);
      break;

    case PPP_PROTO_ECP:
      printf("Recv ECP %s\n", m_code_str[code]);
      break;

    case PPP_PROTO_ENC:
      printf("Recv ENC %s\n", m_code_str[code]);
      break;

    case PPP_PROTO_IP6CP:
      printf("Recv IP6CP %s\n", m_code_str[code]);
      break;

    case PPP_PROTO_IP6:
      printf("Recv IP6 Packet\n");
      break;

    default:
      printf("Recv unknown packet: 0x%04x\n", proto);
      break;
  }
}
