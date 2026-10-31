#ifndef IVAO_FSD_H
#define IVAO_FSD_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#define FSD_MAX_PILOTS 256
struct fsd_pilot { char callsign[24]; char squawk[5]; char aircraft_code[32]; char mode; double lat, lon, altitude_ft, ground_speed_kt; double pitch_deg, bank_deg, heading_deg; int has_pbh; uint32_t packed_pbh; uint64_t updates; time_t last_seen; };
struct fsd_traffic { struct fsd_pilot pilots[FSD_MAX_PILOTS]; size_t count; uint64_t position_frames, atc_frames, rejected, deletes, expired; char pending[8192]; size_t pending_len; };
void fsd_feed(struct fsd_traffic *t, const unsigned char *buf, size_t len);
void fsd_expire(struct fsd_traffic *t, time_t now, unsigned max_age_seconds);
void fsd_print(const struct fsd_traffic *t, FILE *out);
/* Read an offline classic PCAP, or capture live on an interface with libpcap.
 * Only server->client data from TCP src port is inspected. */
int fsd_capture_pcap(struct fsd_traffic *t, const char *filename, unsigned short port);
int fsd_capture_live(struct fsd_traffic *t, const char *iface, unsigned short port, volatile int *stop);
#endif
