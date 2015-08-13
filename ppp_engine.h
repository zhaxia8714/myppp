#ifndef __PPP_ENGINE_H__
#define __PPP_ENGINE_H__

int ppp_engine_open(struct lcp_context *context);
int ppp_engine_close(struct lcp_context *context);
int ppp_engine_process(struct lcp_context *context, uint8_t *buf, uint16_t len);
int ppp_engine_lower_layer_up(struct lcp_context *context);
int ppp_engine_lower_layer_down(struct lcp_context *context);
int ppp_engine_upper_layer_started(struct lcp_context *context);
int ppp_engine_upper_layer_finished(struct lcp_context *context);
int ppp_engine_timeout(struct lcp_context *context);

#endif
