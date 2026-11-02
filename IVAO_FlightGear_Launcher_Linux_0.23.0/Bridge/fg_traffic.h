#ifndef FG_TRAFFIC_H
#define FG_TRAFFIC_H
#include "ivao_fsd.h"
#include <time.h>
struct fg_traffic_sender { int enabled, sock, ready, port; time_t last_connect, last_send; struct timespec last_tick; unsigned packets, failures; };
void fg_traffic_init(struct fg_traffic_sender *s);
void fg_traffic_tick(struct fg_traffic_sender *s, const struct fsd_traffic *t, time_t now);
void fg_traffic_clear(struct fg_traffic_sender *s);
void fg_traffic_close(struct fg_traffic_sender *s);
#endif
