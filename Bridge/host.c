#define _POSIX_C_SOURCE 200809L
#include <dlfcn.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <time.h>
#include <pthread.h>
#include "ivao_fsd.h"
#include "fg_traffic.h"

typedef int (*Start)(char*, char*, char*);
typedef int (*Enable)(void);
typedef void (*Disable)(void);
typedef void (*Stop)(void);

typedef void (*UpdateTelemetry)(const double *, int);
typedef int (*RunFrame)(double);
typedef int (*ShimOnline)(void);
typedef int (*PopCommand)(int *, int *);
static PopCommand pop_command=NULL;
static RunFrame run_frame = NULL;
static ShimOnline shim_online = NULL;
static UpdateTelemetry update_telemetry = NULL;
static int udp_socket = -1;
static int bind_udp(void) {
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if(s<0) { perror("[FG BRIDGE] socket"); return -1; }
    struct sockaddr_in addr={0}; addr.sin_family=AF_INET; addr.sin_port=htons(5501); addr.sin_addr.s_addr=inet_addr("127.0.0.1");
    if(bind(s,(struct sockaddr*)&addr,sizeof(addr))<0) { perror("[FG BRIDGE] bind localhost:5501"); close(s); return -1; }
    fprintf(stderr,"[FG BRIDGE] Listening on UDP 127.0.0.1:5501 (FlightGear generic ASCII)\n");
    return s;
}
static void receive_udp(void) {
    char buf[1024]; struct sockaddr_in source={0}; socklen_t size=sizeof(source);
    ssize_t n=recvfrom(udp_socket,buf,sizeof(buf)-1,0,(struct sockaddr*)&source,&size);
    if(n<=0 || source.sin_addr.s_addr!=inet_addr("127.0.0.1")) return;
    buf[n]=0;
    double v[12]={0};
    int num=sscanf(buf,"%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf",
        &v[0],&v[1],&v[2],&v[3],&v[4],&v[5],&v[6],&v[7],&v[8],&v[9],&v[10],&v[11]);
    if(num!=12) { fprintf(stderr,"[FG BRIDGE] malformed frame: %d fields\n",num); return; }
    for(int i=0;i<12;i++) if(!isfinite(v[i])) return;
    if(fabs(v[0])>90 || fabs(v[1])>180 || v[2]<-2000 || v[2]>100000 || v[6]<0 || v[6]>3000) return;
    /* Input: altitude ft, groundspeed kt; XPLM: elevation m, groundspeed m/s. */
    v[2]*=0.3048; v[6]*=0.514444444444;
    v[7]*=0.3048; /* indicated altitude ft -> m */
    v[9]*=1000.0; v[10]*=1000.0; /* MHz -> XPLM 8.33 frequency integer */
    update_telemetry(v,12);
    static unsigned frames; if(++frames%50==1)
        fprintf(stderr,"[FG BRIDGE] %u frames: lat=%.6f lon=%.6f elev=%.1fm heading=%.1f speed=%.1fm/s COM1=%.0f XPDR=%.0f\n",
            frames,v[0],v[1],v[2],v[5],v[6],v[9],v[11]);
}
/* FlightGear telnet property interface: --telnet=5502.
 * Explicit per-property updates only; telemetry never triggers commands. */
static void deliver_fg_commands(void) {
    if(!pop_command) return;
    int kind=0, value=0;
    while(pop_command(&kind,&value)) {
        const char *path = kind==1 ? "/instrumentation/comm[0]/frequencies/selected-mhz" :
                           kind==2 ? "/instrumentation/comm[1]/frequencies/selected-mhz" :
                           kind==3 ? "/instrumentation/transponder/id-code" : NULL;
        if(!path) continue;
        char command[256];
        if(kind<3) snprintf(command,sizeof(command),"set %s %.3f\r\n",path,(double)value/1000.0);
        else snprintf(command,sizeof(command),"set %s %04d\r\n",path,value);
        int fd=socket(AF_INET,SOCK_STREAM,0);
        if(fd<0) {perror("[FG COMMAND] socket"); continue;}
#ifdef SO_NOSIGPIPE
        int no_sigpipe=1; setsockopt(fd,SOL_SOCKET,SO_NOSIGPIPE,&no_sigpipe,sizeof(no_sigpipe));
#endif
        struct timeval tv={.tv_sec=1,.tv_usec=0};
        setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&tv,sizeof(tv));
        setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));
        struct sockaddr_in addr={0}; addr.sin_family=AF_INET; addr.sin_port=htons(5502); addr.sin_addr.s_addr=inet_addr("127.0.0.1");
        if(connect(fd,(struct sockaddr*)&addr,sizeof(addr))<0) {
            fprintf(stderr,"[FG COMMAND] cannot connect localhost:5502; enable FlightGear --telnet=5502 (%s)\n",strerror(errno));
            close(fd); continue;
        }
        size_t len=strlen(command); ssize_t wrote=send(fd,command,len,0);
        fprintf(stderr,"[FG COMMAND] %s -> %s",wrote==(ssize_t)len?"SENT":"FAILED",command);
        shutdown(fd,SHUT_WR); close(fd);
    }
}
static volatile sig_atomic_t interrupted = 0;
static struct fsd_traffic traffic;
static struct fg_traffic_sender fgtraffic;
static int traffic_send = 0;
static const char *capture_iface = NULL;
static volatile int capture_stop = 0;
static void *traffic_thread(void *unused) {
    (void)unused;
    (void)fsd_capture_live(&traffic,capture_iface,6809,&capture_stop);
    return NULL;
}

static void on_signal(int sig) { (void)sig; interrupted = 1; }

static void usage(const char *program) {
    fprintf(stderr, "Usage: %s /absolute/path/ivao_pilot_test.xpl [--run | --enable-diagnostic | --hold-diagnostic SECONDS | --flightloop-diagnostic SECONDS] [--traffic-interface en0 | --traffic-pcap capture.pcap] [--fg-ai] [--fg-ai-port 5503] [--verbose | --log-file PATH]\n", program);
    fprintf(stderr, "  --run                      run indefinitely with flightloop callbacks at 20 Hz (until SIGTERM/SIGINT)\n");
    fprintf(stderr, "  --verbose                  show complete diagnostics on terminal\n");
    fprintf(stderr, "  --log-file PATH            capture complete diagnostics into specified file\n");
    fprintf(stderr, "  --enable-diagnostic         original behavior: enable, disable, stop, dlclose (may crash on audio shutdown)\n");
    fprintf(stderr, "  --hold-diagnostic SECONDS   enable, remain alive for inspection, exit WITHOUT unsafe plugin cleanup\n");
    fprintf(stderr, "  --flightloop-diagnostic SECONDS   additionally run saved flight loop callbacks at 20 Hz\n");
}

int main(int argc, char **argv) {
    int do_enable = 0, hold = 0, flightloop = 0, forever = 0;
    int verbose = 0;
    const char *log_file = NULL;
    unsigned long seconds = 0;
    if(argc==3 && !strcmp(argv[1],"--traffic-pcap-only")) {
        if(fsd_capture_pcap(&traffic,argv[2],6809)!=0)return 11;
        fsd_print(&traffic,stdout);
        return 0;
    }
    int ai_port = 5503;
    const char *pcap_path = NULL;
    int mode_seen = 0;
    for (int i = 2; i < argc; i++) {
        const char *arg = argv[i];
        if (!strcmp(arg, "--verbose")) {
            verbose = 1;
        } else if (!strcmp(arg, "--log-file")) {
            if (++i >= argc) { usage(argv[0]); return 2; }
            log_file = argv[i];
        } else if (!strcmp(arg, "--fg-ai")) {
            traffic_send = 1;
        } else if (!strcmp(arg, "--fg-ai-port")) {
            if (++i >= argc) { usage(argv[0]); return 2; }
            char *end = NULL;
            errno = 0;
            long port = strtol(argv[i], &end, 10);
            if (errno || !end || *end || port < 1024 || port > 65535) { usage(argv[0]); return 2; }
            ai_port = (int)port;
        } else if (!strcmp(arg, "--traffic-interface")) {
            if (++i >= argc || pcap_path || capture_iface) { usage(argv[0]); return 2; }
            capture_iface = argv[i];
        } else if (!strcmp(arg, "--traffic-pcap")) {
            if (++i >= argc || pcap_path || capture_iface) { usage(argv[0]); return 2; }
            pcap_path = argv[i];
        } else if (!strcmp(arg, "--run")) {
            if (mode_seen++) { usage(argv[0]); return 2; }
            do_enable = hold = flightloop = forever = 1;
        } else if (!strcmp(arg, "--enable-diagnostic")) {
            if (mode_seen++) { usage(argv[0]); return 2; }
            do_enable = 1;
        } else if (!strcmp(arg, "--hold-diagnostic") || !strcmp(arg, "--flightloop-diagnostic")) {
            if (mode_seen++ || ++i >= argc) { usage(argv[0]); return 2; }
            flightloop = !strcmp(arg, "--flightloop-diagnostic");
            char *end = NULL;
            errno = 0;
            seconds = strtoul(argv[i], &end, 10);
            if (errno || !end || *end || seconds < 1 || seconds > 3600) { usage(argv[0]); return 2; }
            do_enable = hold = 1;
        } else {
            fprintf(stderr, "[HOST] Unknown option: %s\n", arg);
            usage(argv[0]);
            return 2;
        }
    }

    /* Production mode: do not flood terminal with plugin, shim or FSD traces.
       --verbose keeps legacy stderr/stdout; --log-file captures both to a file. */
    if (!verbose) {
        FILE *sink = fopen(log_file ? log_file : "/dev/null", "a");
        if (!sink) { fprintf(stderr, "Cannot open diagnostic log file: %s\n", log_file); return 2; }
        fflush(stdout); fflush(stderr);
        if (dup2(fileno(sink), STDOUT_FILENO) < 0 || dup2(fileno(sink), STDERR_FILENO) < 0) {
            fprintf(stderr, "Failed to redirect diagnostic output\n"); fclose(sink); return 2;
        }
        fclose(sink);
    }
    if(pcap_path) {
        if(fsd_capture_pcap(&traffic,pcap_path,6809)!=0)return 11;
        fsd_print(&traffic,stderr);
    }
    // This diagnostic does NOT block external network access. Keep the host offline.
    fprintf(stderr, "[HOST] Mode: %s\n", forever ? "RUN CONTINUOUS" : hold ? "HOLD DIAGNOSTIC (external networking NOT blocked)" : do_enable ? "ENABLE DIAGNOSTIC (may initialize networking)" : "START/STOP ONLY");
    fprintf(stderr, "[HOST] Loading: %s\n", argv[1]);
    void *h = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!h) { fprintf(stderr, "[HOST] dlopen failed: %s\n", dlerror()); return 3; }
    fprintf(stderr, "[HOST] Mach-O and dependencies resolved.\n");
    void *shim=dlopen("libXPLMShim.dylib",RTLD_NOW | RTLD_LOCAL);
    if(!shim) shim=dlopen("./libXPLMShim.dylib",RTLD_NOW | RTLD_LOCAL);
    if(shim) { update_telemetry=(UpdateTelemetry)dlsym(shim,"XPLMShimUpdateTelemetry"); run_frame=(RunFrame)dlsym(shim,"XPLMShimRunFrame"); pop_command=(PopCommand)dlsym(shim,"XPLMShimPopCommand"); }
    if(flightloop && !pop_command) { fprintf(stderr,"[HOST] command queue export missing\n"); return 9; }
    if(flightloop && !run_frame) { fprintf(stderr,"[HOST] scheduler export missing\n"); return 8; }
    if(hold && !update_telemetry) { fprintf(stderr,"[FG BRIDGE] cannot find shim telemetry export\n"); return 7; }

    Start start = (Start)dlsym(h, "XPluginStart");
    Enable enable = (Enable)dlsym(h, "XPluginEnable");
    Disable disable = (Disable)dlsym(h, "XPluginDisable");
    Stop stop = (Stop)dlsym(h, "XPluginStop");
    if (!start || !enable || !disable || !stop) {
        fprintf(stderr, "[HOST] Missing plugin entrypoint.\n");
        dlclose(h);
        return 4;
    }
    fprintf(stderr, "[HOST] Entry points found. Calling XPluginStart...\n");
    char name[256] = {0}, sig[256] = {0}, desc[256] = {0};
    int started = start(name, sig, desc);
    fprintf(stderr, "[HOST] XPluginStart returned %d: name=%s signature=%s\n", started, name, sig);
    if (started) {
        if (do_enable) {
            fprintf(stderr, "[HOST] Calling XPluginEnable ...\n");
            fflush(stderr);
            int enabled = enable();
            fprintf(stderr, "[HOST] XPluginEnable returned %d.\n", enabled);
            fflush(stderr);
            if (hold) {
                fprintf(stderr, "[HOST] HOLD ACTIVE: pid=%ld, duration=%lu seconds. Inspect with: lsof -nP -a -p %ld -iTCP\n", (long)getpid(), seconds, (long)getpid());
                fprintf(stderr, "[HOST] FlightGear DataRefs update from UDP; callbacks %s.\n",flightloop ? "ENABLED at 20 Hz (experimental)" : "remain disabled");
                fprintf(stderr, "[HOST] NO XPluginDisable, XPluginStop, or dlclose will be called in HOLD mode (shutdown crash isolation).\n");
                fflush(stderr);
                signal(SIGINT, on_signal);
                signal(SIGTERM, on_signal);
                pthread_t traffic_tid; int traffic_running=0;
                if(capture_iface) {
                    if(pthread_create(&traffic_tid,NULL,traffic_thread,NULL)==0)traffic_running=1;
                    else fprintf(stderr,"[FSD C] cannot start passive capture thread\n");
                }
                if(traffic_send){
                    if(shim)shim_online=(ShimOnline)dlsym(shim,"shim_ivao_online");
                    if(!shim_online)fprintf(stderr,"[FG TRAFFIC] ERROR: regenerated shim lacks shim_ivao_online; traffic disabled (check make_shim.py)\n");
                    fg_traffic_init(&fgtraffic);fgtraffic.port=ai_port;
                    fprintf(stderr,"[FG TRAFFIC] Startup reconciliation of leftover IVAO aircraft\n");
                    fg_traffic_clear(&fgtraffic);
                    fprintf(stderr,"[FG TRAFFIC] Dedicated AI Telnet port=%d (radio commands remain on 5502)\n",ai_port);}

                udp_socket=bind_udp();
                if(udp_socket<0) _exit(7);
                time_t end_time=forever ? 0 : time(NULL)+(time_t)seconds;
                struct timespec last_tick={0}, now_tick={0};
                clock_gettime(CLOCK_MONOTONIC,&last_tick);
                unsigned long frame_count=0;
                int previous_online=0;
                while(!interrupted && (forever || time(NULL)<end_time)) {
                    fd_set rfds; FD_ZERO(&rfds); FD_SET(udp_socket,&rfds);
                    struct timeval timeout={.tv_sec=0,.tv_usec=flightloop ? 10000 : 100000};
                    int ready=select(udp_socket+1,&rfds,NULL,NULL,&timeout);
                    if(ready>0 && FD_ISSET(udp_socket,&rfds)) receive_udp();
                    if(ready<0 && errno!=EINTR) break;
                    deliver_fg_commands();
                    if(traffic_send){
                        int online=shim_online ? shim_online():0;
                        if(!online && previous_online) {
                            fprintf(stderr,"[FG TRAFFIC] IVAO session went offline; removing its aircraft\n");
                            fg_traffic_clear(&fgtraffic);
                        }
                        if(online && !previous_online)fprintf(stderr,"[FG TRAFFIC] IVAO session online; traffic enabled\n");
                        previous_online=online;
                        if(online)fg_traffic_tick(&fgtraffic,&traffic,time(NULL));
                    }
                    if(flightloop) {
                        clock_gettime(CLOCK_MONOTONIC,&now_tick);
                        double elapsed=(double)(now_tick.tv_sec-last_tick.tv_sec)+(double)(now_tick.tv_nsec-last_tick.tv_nsec)*1e-9;
                        if(elapsed>=0.05) {
                            last_tick=now_tick;
                            if(elapsed>0.25) elapsed=0.05;
                            int calls=run_frame(elapsed);
                            if(++frame_count%100==1) fprintf(stderr,"[HOST] FRAME %lu, callbacks=%d, dt=%.3f s\n",frame_count,calls,elapsed);
                        }
                    }
                }
                capture_stop=1;
                if(traffic_send)fg_traffic_close(&fgtraffic);
                /* pcap_next_ex has a 500ms read timeout; join to finish capture before reporting. */
                if(traffic_running)pthread_join(traffic_tid,NULL);
                if(capture_iface)fsd_print(&traffic,stderr);
                close(udp_socket);
                fprintf(stderr, "[HOST] %s. Exiting without invoking unsafe plugin destructors.\n", forever ? "STOP REQUESTED" : "HOLD FINISHED");
                fflush(stderr);
                _exit(enabled ? 0 : 6);
            }
            if (enabled) {
                fprintf(stderr, "[HOST] Calling XPluginDisable ...\n");
                fflush(stderr);
                disable();
                fprintf(stderr, "[HOST] XPluginDisable returned normally.\n");
            }
        } else {
            fprintf(stderr, "[HOST] Not calling XPluginEnable (default safe diagnostic).\n");
        }
        fprintf(stderr, "[HOST] Calling XPluginStop ...\n");
        fflush(stderr);
        stop();
        fprintf(stderr, "[HOST] XPluginStop returned normally.\n");
    }
    fprintf(stderr, "[HOST] Calling dlclose ...\n");
    fflush(stderr);
    int close_rc = dlclose(h);
    fprintf(stderr, "[HOST] dlclose returned %d.\n", close_rc);
    fprintf(stderr, "[HOST] Diagnostic complete, exit=%d.\n", started ? 0 : 5);
    return started ? 0 : 5;
}
