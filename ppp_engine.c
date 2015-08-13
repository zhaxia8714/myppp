#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "ppp_interface.h"

  void receive_configure_request(struct lcp_context *context,
                                 uint8_t *buf, uint16_t len);
  void receive_configure_ack(struct lcp_context *context);
  void receive_configure_reject(struct lcp_context *context);

  void receive_terminate_request(struct lcp_context *context);
  void receive_terminate_ack(struct lcp_context *context);
  void receive_unknown_code(struct lcp_context *context,
                            uint8_t *buf, uint16_t len);
  void receive_code_reject(struct lcp_context *context);
  void receive_proto_reject(struct lcp_context *context);

  void this_layer_up(struct lcp_context *context);
  void this_layer_down(struct lcp_context *context);
  void this_layer_started(struct lcp_context *context);
  void this_layer_finished(struct lcp_context *context);

  int send_configure_request(struct lcp_context *context);
  int send_configure_ack(struct lcp_context *context,
                         uint8_t *buf, uint16_t len);
  int send_terminate_request(struct lcp_context *context);
  int send_terminate_ack(struct lcp_context *context);
  int send_code_reject(struct lcp_context *context,
                        uint8_t *buf, uint16_t len);

  int start_timeout(struct lcp_context *context);
  int stop_timeout(struct lcp_context *context);

  int ppp_engine_open(struct lcp_context *context) {
    switch (context->state) {
    case LCP_STATE_INITIAL:
      this_layer_started(context);
      context->state = LCP_STATE_STARTING;
      break;

    case LCP_STATE_CLOSED:
      context->restart = LCP_MAX_CONFIGURE;
      send_configure_request(context);
      context->state = LCP_STATE_REQ_SENT;
      break;

    case LCP_STATE_CLOSING:
      context->state = LCP_STATE_STOPPING;
      break;
    default:
      return -1;
      break;
    }

    return 0;
  }

  int ppp_engine_close(struct lcp_context *context) {
    switch (context->state) {
    case LCP_STATE_STARTING:
      this_layer_finished(context);
      context->state = LCP_STATE_INITIAL;
      break;
    case LCP_STATE_STOPPED:
      context->state = LCP_STATE_CLOSED;
      break;
    case LCP_STATE_STOPPING:
      context->state = LCP_STATE_CLOSING;
      break;
    case LCP_STATE_OPENED:
      this_layer_down(context);
      // fall through
    case LCP_STATE_REQ_SENT:
    case LCP_STATE_ACK_RCVD:
    case LCP_STATE_ACK_SENT:
      context->restart = LCP_MAX_CONFIGURE;
      send_terminate_request(context);
      context->state = LCP_STATE_CLOSING;
      break;
    default:
      return -1;
      break;
    }

    return 0;
  }

  int ppp_engine_process(struct lcp_context *context,
                         uint8_t *buf, uint16_t len) {
    struct lcphdr *lcphdr = (struct lcphdr*)buf;

    if (len < sizeof(struct lcphdr))
      return -1;

    switch (lcphdr->code) {
    case LCP_CONFREQ:
      receive_configure_request(context, buf, len);
      break;

    case LCP_CONFACK:
      receive_configure_ack(context);
      break;

    case LCP_CONFNAK:
    case LCP_CONFREJ:
      receive_configure_reject(context);
      break;

    case LCP_TERMREQ:
      receive_terminate_request(context);
      break;

    case LCP_TERMACK:
      receive_terminate_ack(context);
      break;

    case LCP_CODEREJ:
      receive_code_reject(context);
      break;

    case LCP_PROTREJ:
      receive_proto_reject(context);
      break;

    default:
      receive_unknown_code(context, buf, len);
      break;
    }

    return 0;
  }

  int ppp_engine_lower_layer_up(struct lcp_context *context) {
    switch (context->state) {
    case LCP_STATE_INITIAL:
      stop_timeout(context);
      context->state = LCP_STATE_CLOSED;
      break;
    case LCP_STATE_STARTING:
      context->restart = LCP_MAX_CONFIGURE;
      send_configure_request(context);
      context->state = LCP_STATE_REQ_SENT;
      break;
    }

    return 0;
  }

  int ppp_engine_lower_layer_down(struct lcp_context *context) {
    switch (context->state) {
    case LCP_STATE_CLOSED:
    case LCP_STATE_CLOSING:
      stop_timeout(context);
      context->state = LCP_STATE_INITIAL;
      break;
    case LCP_STATE_STOPPED:
    case LCP_STATE_STOPPING:
    case LCP_STATE_REQ_SENT:
    case LCP_STATE_ACK_RCVD:
    case LCP_STATE_ACK_SENT:
    case LCP_STATE_OPENED:
      this_layer_down(context);
      stop_timeout(context);
      context->state = LCP_STATE_STARTING;
      break;
    }

    return 0;
  }

  int ppp_engine_upper_layer_started(struct lcp_context *context) {
    switch (context->state) {
    case LCP_STATE_INITIAL:
      context->state = LCP_STATE_STARTING;
      break;
    case LCP_STATE_CLOSED:
      context->restart = LCP_MAX_CONFIGURE;
      send_configure_request(context);
      context->state = LCP_STATE_REQ_SENT;
      break;
    case LCP_STATE_CLOSING:
      context->state = LCP_STATE_STOPPING;
      break;
    }

    return 0;
  }

  int ppp_engine_upper_layer_finished(struct lcp_context *context) {
    switch (context->state) {
    case LCP_STATE_STARTING:
      context->state = LCP_STATE_INITIAL;
      break;
    case LCP_STATE_STOPPED:
      context->state = LCP_STATE_CLOSED;
      break;
    case LCP_STATE_STOPPING:
      context->state = LCP_STATE_CLOSING;
      break;
    case LCP_STATE_OPENED:
      context->restart = LCP_MAX_TERMINATE;
      send_terminate_request(context);
      context->state = LCP_STATE_CLOSING;
      break;

      // fall through
    case LCP_STATE_REQ_SENT:
    case LCP_STATE_ACK_RCVD:
    case LCP_STATE_ACK_SENT:
      context->restart = LCP_MAX_TERMINATE;//LCP_MAX_CONFIGURE;
      send_terminate_request(context);
      context->state = LCP_STATE_CLOSING;
      break;
    }

    return 0;
  }

  int ppp_engine_timeout(struct lcp_context *context) {
    if (context->restart > 0) {
      context->restart--;

      switch (context->state) {
      case LCP_STATE_CLOSING:
      case LCP_STATE_STOPPING:
        send_terminate_request(context);
        break;
      case LCP_STATE_REQ_SENT:
      case LCP_STATE_ACK_RCVD:
        send_configure_request(context);
        context->state = LCP_STATE_REQ_SENT;
        break;
      case LCP_STATE_ACK_SENT:
        send_configure_request(context);
        break;
      }
    } else {
      switch (context->state) {
      case LCP_STATE_CLOSING:
        context->state = LCP_STATE_CLOSED;
        this_layer_finished(context);
        break;
      case LCP_STATE_STOPPING:
      case LCP_STATE_REQ_SENT:
      case LCP_STATE_ACK_RCVD:
      case LCP_STATE_ACK_SENT:
        context->state = LCP_STATE_STOPPED;
        this_layer_finished(context);
        break;
      }
    }

    return 0;
  }

//================ Process received pcaket ===================
  void receive_configure_request(struct lcp_context *context,
                                 uint8_t *buf, uint16_t len) {
    switch (context->state) {
    case LCP_STATE_CLOSED:
      send_terminate_ack(context);
      break;
    case LCP_STATE_STOPPED:
      context->restart = LCP_MAX_CONFIGURE;
      send_configure_request(context);
      if (send_configure_ack(context, buf, len) == 0)
        context->state = LCP_STATE_ACK_SENT;
      else
        context->state = LCP_STATE_REQ_SENT;
      break;
    case LCP_STATE_REQ_SENT:
    case LCP_STATE_ACK_SENT:
      if (send_configure_ack(context, buf, len) == 0)
        context->state = LCP_STATE_ACK_SENT;
      else
        context->state = LCP_STATE_REQ_SENT;
      break;
    case LCP_STATE_ACK_RCVD:
      if (send_configure_ack(context, buf, len) == 0) {
        stop_timeout(context);
        context->state = LCP_STATE_OPENED;
        this_layer_up(context);
      }
      break;
    case LCP_STATE_OPENED:
      this_layer_down(context);
      send_configure_request(context);
      if (send_configure_ack(context, buf, len) == 0)
        context->state = LCP_STATE_ACK_SENT;
      else
        context->state = LCP_STATE_REQ_SENT;
      break;
    }
  }

  void receive_configure_ack(struct lcp_context *context) {
    switch (context->state) {
    case LCP_STATE_CLOSED:
    case LCP_STATE_STOPPED:
      send_terminate_ack(context);
      break;
    case LCP_STATE_REQ_SENT:
      context->restart = LCP_MAX_CONFIGURE;
      context->state = LCP_STATE_ACK_RCVD;
      break;
    case LCP_STATE_ACK_RCVD:
      send_configure_request(context);
      context->state = LCP_STATE_REQ_SENT;
      break;
    case LCP_STATE_ACK_SENT:
      context->restart = LCP_MAX_CONFIGURE;
      stop_timeout(context);
      context->state = LCP_STATE_OPENED;
      this_layer_up(context);
      break;
    case LCP_STATE_OPENED:
      this_layer_down(context);
      send_configure_request(context);
      context->state = LCP_STATE_REQ_SENT;
      break;
    }
  }

  void receive_configure_reject(struct lcp_context *context) {
    switch (context->state) {
    case LCP_STATE_CLOSED:
    case LCP_STATE_STOPPED:
      send_terminate_ack(context);
      break;
    case LCP_STATE_REQ_SENT:
      context->restart = LCP_MAX_CONFIGURE;
      // fall through
    case LCP_STATE_ACK_RCVD:
      send_configure_request(context);
      context->state = LCP_STATE_REQ_SENT;
      break;
    case LCP_STATE_ACK_SENT:
      context->restart = LCP_MAX_CONFIGURE;
      send_configure_request(context);
      break;
    case LCP_STATE_OPENED:
      this_layer_down(context);
      send_configure_request(context);
      context->state = LCP_STATE_REQ_SENT;
      break;
    }
  }

  void receive_terminate_request(struct lcp_context *context) {
    switch (context->state) {
    case LCP_STATE_CLOSED:
    case LCP_STATE_STOPPED:
    case LCP_STATE_CLOSING:
    case LCP_STATE_STOPPING:
      send_terminate_ack(context);
      break;
    case LCP_STATE_REQ_SENT:
    case LCP_STATE_ACK_RCVD:
    case LCP_STATE_ACK_SENT:
      send_terminate_ack(context);
      context->state = LCP_STATE_REQ_SENT;
      break;
    case LCP_STATE_OPENED:
      this_layer_down(context);
      context->restart = 0;
      send_terminate_ack(context);
      context->state = LCP_STATE_STOPPING;
      break;
    }
  }

  void receive_terminate_ack(struct lcp_context *context) {
    switch (context->state) {
    case LCP_STATE_CLOSING:
      stop_timeout(context);
      this_layer_down(context);
      context->state = LCP_STATE_CLOSED;
      break;
    case LCP_STATE_STOPPING:
      stop_timeout(context);
      context->state = LCP_STATE_STOPPED;
      break;
    case LCP_STATE_OPENED:
      this_layer_down(context);
      send_configure_request(context);
      // fall through
    case LCP_STATE_ACK_RCVD:
      context->state = LCP_STATE_REQ_SENT;
      break;
    }
  }

  void receive_unknown_code(struct lcp_context *context,
                            uint8_t *buf, uint16_t len) {
    send_code_reject(context, buf, len);
  }

  void receive_code_reject(struct lcp_context *context) {
    switch (context->state) {
    case LCP_STATE_CLOSING:
      stop_timeout(context);
      context->state = LCP_STATE_CLOSED;
      break;
    case LCP_STATE_STOPPING:
    case LCP_STATE_REQ_SENT:
    case LCP_STATE_ACK_RCVD:
    case LCP_STATE_ACK_SENT:
      stop_timeout(context);
      context->state = LCP_STATE_STOPPED;
      break;
    case LCP_STATE_OPENED:
      this_layer_down(context);
      context->restart = LCP_MAX_TERMINATE;
      send_terminate_request(context);
      context->state = LCP_STATE_STOPPING;
      break;
    }
  }

//====================================== ===================
  void receive_proto_reject(struct lcp_context *context) {
    context->receive_proto_reject(context);
  }

  int send_configure_request(struct lcp_context *context) {
    if (context->send_configure_request(context) < 0)
      return -1;

    hdlc_send_msg(context);
    start_timeout(context);
    return 0;
  }

  int send_configure_ack(struct lcp_context *context,
                         uint8_t *buf, uint16_t len) {
    context->send_configure_ack(context, buf, len);
    hdlc_send_msg(context);
    return 0;
  }

  int send_terminate_request(struct lcp_context *context) {
    if (context->req_len)
      return -1;

    context->req_buf[0] = LCP_TERMREQ;
    context->req_buf[1] = context->req_id++;
    context->req_buf[2] = 0;
    context->req_buf[3] = sizeof(struct lcphdr);
    context->req_len = sizeof(struct lcphdr);

    hdlc_send_msg(context);
    start_timeout(context);
    return 0;
  }

  int send_terminate_ack(struct lcp_context *context) {
    if (context->rep_len)
      return -1;

    context->rep_buf[0] = LCP_TERMACK;
    context->rep_buf[1] = context->req_id++;
    context->rep_buf[2] = 0;
    context->rep_buf[3] = sizeof(struct lcphdr);
    context->rep_len = sizeof(struct lcphdr);

    hdlc_send_msg(context);
    start_timeout(context);
    return 0;
  }

  int send_code_reject(struct lcp_context *context,
                       uint8_t *buf, uint16_t len) {
    if (context->rep_len)
      return -1;

    memcpy(context->rep_buf, buf, len);
    context->rep_buf[0] = LCP_CODEREJ;
    context->rep_len = len;

    hdlc_send_msg(context);
    return 0;
  }

  int start_timeout(struct lcp_context *context) {
    return context->start_timeout(context);
  }

  int stop_timeout(struct lcp_context *context) {
    return context->stop_timeout(context);
  }

  void this_layer_up(struct lcp_context *context) {
    context->this_layer_up(context);
  }

  void this_layer_down(struct lcp_context *context) {
    context->this_layer_down(context);
  }

  void this_layer_started(struct lcp_context *context) {
    context->this_layer_started(context);
  }

  void this_layer_finished(struct lcp_context *context) {
    context->this_layer_finished(context);
  }

