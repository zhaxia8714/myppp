#ifndef VIRTUAL_TIMER_H
#define VIRTUAL_TIMER_H

#define S_TIMER_RESERVED  0
#define S_TIMER_STOPPED   1
#define S_TIMER_RUNNING   2

#define MAX_TIMER_NUM     200

typedef void (*timer_callback_t)(void *data);

typedef struct timer {
  uint8_t state;
  uint64_t counter;

  void *data;
  timer_callback_t fun;
} virtual_timer_t;

void timer_init();
int timer_new(timer_callback_t fun, void *data);
int timer_distroy(int id);
int timer_start_one_shot(int id, int ms);
int timer_stop(int id);

#endif
