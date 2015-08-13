#include <stdio.h>
#include <stdlib.h>

#include <stdint.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include "virtual_timer.h"

#ifdef _WIN32
#include <Windows.h>
#include <Mmsystem.h>
#pragma comment(lib, "Winmm.lib")
MMRESULT timer_id;
#else
#include <unistd.h>
#include <sys/time.h>
#endif

#define TIMER_PERIOD_MS 10  //ms

uint64_t m_base_counter = 0;
virtual_timer_t m_timer[MAX_TIMER_NUM];

void sig_timer_fun(int sig) {
  uint32_t i;

  sig = sig;
  m_base_counter += TIMER_PERIOD_MS;
  for (i = 0; i < MAX_TIMER_NUM; i++) {
    if (m_timer[i].state != S_TIMER_RUNNING) {
      continue;
    }

    if (m_timer[i].counter <= m_base_counter) {
      m_timer[i].state = S_TIMER_STOPPED;
      (*m_timer[i].fun)(m_timer[i].data);
    }
  }
}

#ifdef _WIN32
void WINAPI onTimeFunc(UINT wTimerID, UINT msg,DWORD dwUser,DWORD dwl,DWORD dw2) {
  sig_timer_fun(0);
}
#endif

void timer_init(void) {
#ifdef _WIN32
  memset(m_timer, 0, sizeof(m_timer));
  timer_id = timeSetEvent(TIMER_PERIOD_MS, 1, (LPTIMECALLBACK)onTimeFunc, 0, TIME_PERIODIC);
#else
  struct sigaction act;
  struct itimerval val;

  memset(m_timer, 0, sizeof(m_timer)); 	 
  act.sa_handler = sig_timer_fun;
  act.sa_flags = 0;
  sigemptyset(&act.sa_mask);
  sigaction(SIGPROF, &act, NULL);

  val.it_value.tv_sec = 0;
  val.it_value.tv_usec = TIMER_PERIOD_MS*1000;
  val.it_interval = val.it_value;
  setitimer(ITIMER_PROF, &val, NULL);
#endif
}

int timer_new(timer_callback_t fun, void *data) {
  int i;

  for (i = 0; i < MAX_TIMER_NUM; i++) {
    if (m_timer[i].state != S_TIMER_RESERVED) {
      continue;
    }

    m_timer[i].state = S_TIMER_STOPPED;
    m_timer[i].data = data;
    m_timer[i].fun = fun;
    break;
  }

  return (i >= MAX_TIMER_NUM)? -1: i;
}

int timer_start_one_shot(int id, int ms) {
  if (id >= MAX_TIMER_NUM)
    return -1;

  if (m_timer[id].state == S_TIMER_RESERVED)
    return -1;

  m_timer[id].state = S_TIMER_RUNNING;
  m_timer[id].counter = m_base_counter + ms;
  return 0;
}

int timer_stop(int id) {
  if (id >= MAX_TIMER_NUM)
    return -1;

  m_timer[id].state = S_TIMER_STOPPED;
  return 0;
}

int timer_distroy(int id) {
  if (id >= MAX_TIMER_NUM)
    return -1;

  m_timer[id].state = S_TIMER_RESERVED;
  return 0;
}


