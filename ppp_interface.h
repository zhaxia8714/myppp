#ifndef PROTOCOL_H
#define PROTOCOL_H

#ifdef _WIN32
#include <windows.h>
#include <WinSock2.h>
#define _NTOHL(x) ((uint32_t)ntohl((u_long)(x)))
#define _HTONL(x) ((uint32_t)htonl((u_long)(x)))
#define _NTOHS(x) ((uint16_t)ntohs((u_short)(x)))
#else
#include <netinet/in.h>
#define _NTOHL(x) ntohl(x)
#define _HTONL(x) htonl(x)
#define _NTOHS(x) ntohs(x)
#endif

#define HDLC_FLAG_SEQUENCE  0x7e
#define HDLC_CONTROL_ESCAPE 0x7d

#define HDLC_STATE_NO_SYNC  0
#define HDLC_STATE_SYNC     1
#define HDLC_STATE_ESCAPED  2

#define PPP_PROTO_IP4       0x0021
#define PPP_PROTO_ENC       0x0053
#define PPP_PROTO_IP6       0x0057
#define PPP_PROTO_LCP       0xc021
#define PPP_PROTO_ECP       0x8053
#define PPP_PROTO_IP6CP     0x8057

struct lcphdr {
  uint8_t code;
  uint8_t id;
  uint16_t len;
};

struct lcpopt {
  uint8_t type;
  uint8_t len;
};

#define LCP_MAX_TERMINATE   2
#define LCP_MAX_CONFIGURE   3
#define LCP_MAX_FAILURE     2

#define LCP_STATE_INITIAL   0
#define LCP_STATE_STARTING  1
#define LCP_STATE_CLOSED    2
#define LCP_STATE_STOPPED   3
#define LCP_STATE_CLOSING   4
#define LCP_STATE_STOPPING  5
#define LCP_STATE_REQ_SENT  6
#define LCP_STATE_ACK_RCVD  7
#define LCP_STATE_ACK_SENT  8
#define LCP_STATE_OPENED    9

#define LCP_CONFREQ         1   /* Configuration Request */
#define LCP_CONFACK         2   /* Configuration Ack */
#define LCP_CONFNAK         3   /* Configuration Nak */
#define LCP_CONFREJ         4   /* Configuration Reject */
#define LCP_TERMREQ         5   /* Termination Request */
#define LCP_TERMACK         6   /* Termination Ack */
#define LCP_CODEREJ         7   /* Code Reject */
#define LCP_PROTREJ         8   /* Protocol Reject */
#define LCP_ECHOREQ         9   /* Echo Request */
#define LCP_ECHOREP         10  /* Echo Reply */
#define LCP_DISCREQ         11  /* Discard Request */
#define LCP_IDENTIF         12  /* Identification */
#define LCP_TIMEREM         13  /* Time Remaining */
#define ECP_RESTREQ         14  /* ECP Reset-Request */
#define ECP_RESTACK         15  /* ECP Reset-Ack */

#define LCP_CI_VENDOR       0   /* Vendor Specific */
#define LCP_CI_MRU          1   /* Maximum Receive Unit */
#define LCP_CI_ASYNCMAP     2   /* Async Control gcharacter Map */
#define LCP_CI_AUTHTYPE     3   /* Authentication Type */
#define LCP_CI_QUALITY      4   /* Quality Protocol */
#define LCP_CI_MAGICNUMBER  5   /* Magic Number */
#define LCP_CI_PCOMPRESSION 7   /* Protocol Field Compression */
#define LCP_CI_ACCOMPRESSION 8  /* Address/Control Field Compression */
#define LCP_CI_FCSALTERN    9   /* FCS-Alternatives */
#define LCP_CI_SDP          10  /* Self-Describing-Pad */
#define LCP_CI_NUMBERED     11  /* Numbered-Mode */
#define LCP_CI_CALLBACK     13  /* callback */
#define LCP_CI_MRRU         17  /* max reconstructed receive unit; multilink */
#define LCP_CI_SSNHF        18  /* short sequence numbers for multilink */
#define LCP_CI_EPDISC       19  /* endpoint discriminator */
#define LCP_CI_MPPLUS       22  /* Multi-Link-Plus-Procedure */
#define LCP_CI_LDISC        23  /* Link-Discriminator */
#define LCP_CI_LCPAUTH      24  /* LCP Authentication */
#define LCP_CI_COBS         25  /* Consistent Overhead Byte Stuffing */
#define LCP_CI_PREFELIS     26  /* Prefix Elision */
#define LCP_CI_MPHDRFMT     27  /* MP Header Format */
#define LCP_CI_I18N         28  /* Internationalization */
#define LCP_CI_SDL          29  /* Simple Data Link */

#define ECP_CI_AES          16  /* AES */

#define ECP_AES_128_CCM_PSK  1
#define ECP_AES_256_CCM_PSK  2
#define ECP_AES_128_CCM_DH   3
#define ECP_AES_256_CCM_DH   4

#define ECP_AES_TYPE_0       0
#define ECP_AES_TYPE_1       1
#define ECP_AES_TYPE_2       2
#define ECP_AES_TYPE_3       3
#define ECP_AES_TYPE_4       4

#define ECP_REQUEST_MODE     0
#define ECP_RESPONSE_MODE    1

#define IPV6CP_CI_INTERFACEID  1  /* Interface Identifier */

#define PPP_ECHO_ATTEMPTS      3
#define PPP_ECHO_PERIOD        (30*1000)  /* ms */
#define PPP_ECHO_TIMEOUT       (3*1000)   /* ms */
#define PPP_REPLY_TIMEOUT      30         /* ms */

#define PPP_CLOSED             0
#define PPP_OPENING            1
#define PPP_OPENED             2
#define PPP_CLOSING            3

#define DEBUG_LEVEL_1  1
#define DEBUG_LEVEL_2  2
#define DEBUG_LEVEL_3  3
#define DEBUG_LEVEL_4  4

#define HDLC_BUF_LEN         1500

typedef struct device {
  int       id;

  uint8_t   tx_buf[HDLC_BUF_LEN];
  uint16_t  tx_len;
  uint16_t  tx_crc;

  uint8_t   buf[HDLC_BUF_LEN];  // orgin received data

  uint8_t   rx_buf[HDLC_BUF_LEN];
  uint16_t  rx_len;
  uint16_t  rx_crc;
  uint8_t   rx_state;

  int (*raw_send)(int id, uint8_t *buf, uint16_t len);
  int (*raw_recv)(void *handle, uint8_t *buf, uint16_t len);
} device_t;

//typedef struct device device_t;

struct lcp_context {
  uint16_t proto;
  uint8_t state;
  uint8_t restart;
  uint8_t req_id;
  uint16_t req_len;
  uint16_t rep_len;
  uint8_t req_buf[128];
  uint8_t rep_buf[128];

  int (*receive_proto_reject)(struct lcp_context *context);
  int (*send_configure_request)(struct lcp_context *context);
  int (*send_configure_ack)(struct lcp_context *context, uint8_t *buf, uint16_t len);

  int (*start_timeout)(struct lcp_context *context);
  int (*stop_timeout)(struct lcp_context *context);

  int (*this_layer_up)(struct lcp_context *context);
  int (*this_layer_down)(struct lcp_context *context);

  int (*this_layer_started)(struct lcp_context *context);
  int (*this_layer_finished)(struct lcp_context *context);

  struct ppp *ppp;
};

typedef struct lcp_module {
  struct lcp_context context;

  uint8_t echo_attempts;
  int    echo_timer;
  int    timeout_timer;

  void (*module_up)(struct ppp *ppp);
  void (*module_down)(struct ppp *ppp);
} lcp_module_t;

typedef struct ip6cp_module {
  struct lcp_context context;

  int timeout_timer;
  void (*module_up)(struct ppp *ppp);
  void (*module_down)(struct ppp *ppp);
} ip6cp_module_t;


struct ppp {
  char     dev_name[100];
  uint8_t  state;
  uint16_t proto;

  device_t dev;
  lcp_module_t lcp;
  ip6cp_module_t ip6cp;

  void (*ppp_recv)(struct ppp *ppp, uint8_t *buf, uint16_t len);
  void (*ppp_up)(struct ppp *ppp);
  void (*ppp_down)(struct ppp *ppp);

  uint8_t local_eui64[8];
  uint8_t remote_eui64[8];
};

typedef struct ppp ppp_t;

ppp_t *ppp_interface_create(char *name, int id,
                    void (*app_recv)(ppp_t *ppp, uint8_t *buf, uint16_t len),
                    int (*raw_send)(int id, uint8_t *buf, uint16_t len),
                    void (*ppp_up)(ppp_t *ppp),
                    void (*ppp_down)(ppp_t *ppp));

void ppp_interface_free(ppp_t *ppp);

int ppp_interface_raw_recv(ppp_t *ppp, uint8_t *buf, uint16_t len);
int ppp_interface_send(ppp_t *ppp, uint8_t *buf, uint16_t len);

int ppp_interface_enable(ppp_t *ppp);
int ppp_interface_disable(ppp_t *ppp);

int ppp_ip6_received(ppp_t *ppp, uint8_t *buf, uint16_t len);

void send_proto_reject(ppp_t *ppp);
int hdlc_send_msg(struct lcp_context *context);

#endif
