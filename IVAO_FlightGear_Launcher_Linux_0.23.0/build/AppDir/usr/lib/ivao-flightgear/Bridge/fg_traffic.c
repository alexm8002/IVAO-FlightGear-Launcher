/* FlightGear AI aircraft integration, using the exact Telnet/Nasal
 * framing and add-aiobject aircraft type validated by test_fg_ai.py.
 * Loopback only. FSD fields are checked before inserting in Nasal. */
#define _POSIX_C_SOURCE 200809L
#include "fg_traffic.h"
#include <arpa/inet.h>
#include <errno.h>
#include <math.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

static int write_all(int fd, const char *p, size_t n) {
    while(n) { ssize_t k=send(fd,p,n,0);if(k<=0)return -1;p+=k;n-=(size_t)k; }return 0;
}
static int connect_fg(int port) {
    int fd=socket(AF_INET,SOCK_STREAM,0);if(fd<0)return -1;
#ifdef SO_NOSIGPIPE
    int one=1;(void)setsockopt(fd,SOL_SOCKET,SO_NOSIGPIPE,&one,sizeof(one));
#endif
    struct timeval tv={.tv_sec=2,.tv_usec=0};
    (void)setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&tv,sizeof(tv));
    (void)setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));
    struct sockaddr_in a={0};a.sin_family=AF_INET;a.sin_port=htons((unsigned short)port);
    if(inet_pton(AF_INET,"127.0.0.1",&a.sin_addr)!=1){close(fd);return -1;}
    if(connect(fd,(struct sockaddr*)&a,sizeof(a))!=0){close(fd);return -1;}return fd;
}
static size_t read_reply(int fd, char *out, size_t cap, unsigned milliseconds) {
    size_t n=0;unsigned elapsed=0;
    while(elapsed<milliseconds && n+1<cap) {
        fd_set f;FD_ZERO(&f);FD_SET(fd,&f);
        struct timeval tv={.tv_sec=0,.tv_usec=100000};
        int rc=select(fd+1,&f,NULL,NULL,&tv);
        elapsed+=100;
        if(rc<0 && errno==EINTR)continue;
        if(rc<=0)continue;
        ssize_t k=recv(fd,out+n,cap-1-n,0);
        if(k<=0)break;n+=(size_t)k;out[n]=0;
        /* Consume idle silence after first response, allowing trailing lines. */
        if(n && elapsed+200<milliseconds && strstr(out,"</PropertyList>"))break;
    }
    out[n]=0;return n;
}
static int emit(char *buf,size_t cap,size_t *off,const char *line) {
    size_t n=strlen(line);if(n>=cap-*off)return 0;memcpy(buf+*off,line,n);*off+=n;buf[*off]=0;return 1;
}
static int valid_cs(const char *s) {
    size_t n=strlen(s);if(!n||n>20)return 0;
    for(size_t i=0;i<n;i++)if(!((s[i]>='A'&&s[i]<='Z')||(s[i]>='0'&&s[i]<='9')))return 0;
    return 1;
}
/* Model index generated locally from installed FG_ROOT/AI/Aircraft XMLs. */
struct model_entry { char icao[5], airline[24], path[240]; };
static struct model_entry models[4096];
static size_t nmodels=0;
struct model_trace { char callsign[24], aircraft_code[32], model[240]; };
static struct model_trace model_traces[256];
static void trace_model(const struct fsd_pilot *p, const char *selected) {
    size_t slot=256;
    for(size_t i=0;i<256;i++) {
        if(strcmp(model_traces[i].callsign,p->callsign)==0){slot=i;break;}
        if(slot==256 && !model_traces[i].callsign[0])slot=i;
    }
    if(slot==256)return;
    struct model_trace *q=&model_traces[slot];
    if(strcmp(q->aircraft_code,p->aircraft_code)==0 && strcmp(q->model,selected)==0)return;
    const char *reason=(!p->aircraft_code[0])?"NO_IVAO_MODEL":
        (strcmp(selected,"Aircraft/A320/A320-default.xml")==0?"A320_FALLBACK_OR_DEFAULT":
         (strcmp(selected,"Aircraft/738/738-default.xml")==0?"B738_DEFAULT":"CATALOGUE_MATCH"));
    fprintf(stderr,"[FG MODEL SELECT] callsign=%s ivao=%s selected=%s reason=%s\n",
        p->callsign,p->aircraft_code[0]?p->aircraft_code:"(pending)",selected,reason);
    snprintf(q->callsign,sizeof(q->callsign),"%s",p->callsign);
    snprintf(q->aircraft_code,sizeof(q->aircraft_code),"%s",p->aircraft_code);
    snprintf(q->model,sizeof(q->model),"%s",selected);
}
static FILE *open_model_index(void) {
    FILE *f = fopen("fg_models.tsv", "r");
    if (f) return f;
    f = fopen("Bridge/fg_models.tsv", "r");
    if (f) return f;
#ifdef __APPLE__
    /* Launcher starts the host with an arbitrary working directory.
       Resolve the catalogue relative to the host executable instead. */
    char exe[4096]; uint32_t len = sizeof(exe);
    if (_NSGetExecutablePath(exe, &len) == 0) {
        char *slash = strrchr(exe, '/');
        if (slash) {
            *slash = 0;
            const char *suffixes[] = {
                "/fg_models.tsv", "/../Bridge/fg_models.tsv",
                "/../Resources/Bridge/fg_models.tsv",
                "/../../Resources/Bridge/fg_models.tsv",
                "/../../../Resources/Bridge/fg_models.tsv"
            };
            for (size_t j = 0; j < sizeof(suffixes)/sizeof(suffixes[0]); j++) {
                char path[4608];
                if (snprintf(path, sizeof(path), "%s%s", exe, suffixes[j]) >= (int)sizeof(path)) continue;
                f = fopen(path,"r");
                if (f) { fprintf(stderr,"[FG MODEL] catalogue=%s\n",path); return f; }
            }
        }
    }
#endif
#ifdef __linux__
    char exe[4096]; ssize_t n=readlink("/proc/self/exe",exe,sizeof(exe)-1);
    if(n>0 && (size_t)n<sizeof(exe)) {
        exe[n]=0; char *slash=strrchr(exe,'/');
        if(slash){*slash=0; char path[4608];
            if(snprintf(path,sizeof(path),"%s/fg_models.tsv",exe)<(int)sizeof(path)) {
                f=fopen(path,"r"); if(f){fprintf(stderr,"[FG MODEL] catalogue=%s\n",path);return f;}
            }
        }
    }
#endif
    return NULL;
}
static void load_models(void) {
    FILE *f=open_model_index();
    if(!f){fprintf(stderr,"[FG MODEL] catalogue inaccessible; fallback A320\n");return;}
    char line[512];
    while(nmodels<4096&&fgets(line,sizeof(line),f)) {
        char *type=strtok(line,"\t\r\n");char *airline=strtok(NULL,"\t\r\n");char *path=strtok(NULL,"\t\r\n");
        if(!type||!airline||!path||strlen(type)!=4||strncmp(path,"Aircraft/",9)!=0||strstr(path,".."))continue;
        struct model_entry *m=&models[nmodels++];
        snprintf(m->icao,sizeof(m->icao),"%.4s",type);
        snprintf(m->airline,sizeof(m->airline),"%.23s",airline);
        snprintf(m->path,sizeof(m->path),"%.239s",path);
    }
    fclose(f);fprintf(stderr,"[FG MODEL] indexed %zu aircraft XMLs; ICAO+airline liveries enabled\n",nmodels);
}
static const char *choose_model(const struct fsd_pilot *p) {
    static const char *fallback="Aircraft/A320/A320-default.xml";
    if(strlen(p->aircraft_code)<4)return fallback;
    char type[5]; memcpy(type,p->aircraft_code,4); type[4]=0;
    int b737_family=strncmp(type,"B73",3)==0;
    /* FlightGear's available B738 AI models represent the full B73 family.
       A20N can use A320 models when no A20N catalogue entry exists. */
    const char *icao=b737_family?"B738":type;
    char model_operator[4]={0}, callsign_operator[4]={0};
    if(strlen(p->aircraft_code)>=7) {
        const char *v=p->aircraft_code+4;
        if(v[0]>='A'&&v[0]<='Z'&&v[1]>='A'&&v[1]<='Z'&&v[2]>='A'&&v[2]<='Z')
            memcpy(model_operator,v,3);
    }
    if(strlen(p->callsign)>=4) {
        const char *v=p->callsign;
        if(v[0]>='A'&&v[0]<='Z'&&v[1]>='A'&&v[1]<='Z'&&v[2]>='A'&&v[2]<='Z')
            memcpy(callsign_operator,v,3);
    }
    const char *best=NULL; int best_score=-1;
    for(int pass=0;pass<2;pass++) {
        const char *lookup=pass==0?icao:"A320";
        if(pass==1 && strcmp(type,"A20N")!=0)break;
        for(size_t i=0;i<nmodels;i++) {
            const struct model_entry *m=&models[i];
            if(strcmp(m->icao,lookup)!=0)continue;
            const char *name=strrchr(m->path,'/');name=name?name+1:m->path;
            int score=-1;
            if(model_operator[0] && strcmp(m->airline,model_operator)==0)score=400;
            else if(callsign_operator[0] && strcmp(m->airline,callsign_operator)==0)score=350;
            else if(strcmp(m->airline,"---")==0)
                score=(strstr(name,"-Default.") || strstr(name,"-default."))?200:100;
            if(score>best_score){best_score=score;best=m->path;}
        }
        if(best)break;
    }
    if(best)return best;
    if(b737_family)return "Aircraft/738/738-default.xml";
    return fallback;
}

void fg_traffic_init(struct fg_traffic_sender *s){memset(s,0,sizeof(*s));s->enabled=1;s->sock=-1;s->port=5503;load_models();}
void fg_traffic_clear(struct fg_traffic_sender *s) {
    if(!s->enabled)return;
    struct fsd_traffic empty={0};
    s->last_tick.tv_sec=0;s->last_tick.tv_nsec=0;
    s->last_send=0;s->failures=0;
    fprintf(stderr,"[FG TRAFFIC] Clearing IVAO aircraft on logout/shutdown\n");
    fg_traffic_tick(s,&empty,time(NULL));
}
void fg_traffic_close(struct fg_traffic_sender *s){fg_traffic_clear(s);if(s->sock>=0){close(s->sock);s->sock=-1;}}
void fg_traffic_tick(struct fg_traffic_sender *s,const struct fsd_traffic *t,time_t now) {
    if(!s->enabled||(s->failures&&now-s->last_connect<10))return;
    struct timespec tick={0};clock_gettime(CLOCK_MONOTONIC,&tick);
    double since=(double)(tick.tv_sec-s->last_tick.tv_sec)+1e-9*(double)(tick.tv_nsec-s->last_tick.tv_nsec);
    if(s->last_tick.tv_sec && since<0.045)return;
    s->last_tick=tick;s->last_send=now;s->last_connect=now;
    char code[65536];size_t off=0;unsigned count=0;
    /* v41: XPMP2 is the sole source of remote-aircraft motion. There is no
       FSD prediction, no six-sample correction and no Nasal timer. */
    if(!emit(code,sizeof(code),&off,"nasal\r\n"
        "var updated=0; var created=0; var removed=0; var errors=0; var seen=0; var changed=0;\n"
        "var active={};\n"
        "var root=props.globals.getNode('/ai/models',1);\n"
        "var flights=[\n"))return;
    for(size_t i=0;i<t->count&&count<32;i++) {
        const struct fsd_pilot *p=&t->pilots[i];
        if(!p->xpmp_valid || !valid_cs(p->callsign) ||
            !isfinite(p->xpmp_lat) || !isfinite(p->xpmp_lon) ||
            !isfinite(p->xpmp_alt_ft) || !isfinite(p->xpmp_heading) ||
            !isfinite(p->xpmp_pitch) || !isfinite(p->xpmp_roll) ||
            fabs(p->xpmp_lat)>90 || fabs(p->xpmp_lon)>180 ||
            (fabs(p->xpmp_lat)<0.01 && fabs(p->xpmp_lon)<0.01) ||
            p->xpmp_alt_ft < -2000 || p->xpmp_alt_ft > 100000) continue;
        const char *model=choose_model(p);
        trace_model(p,model);
        char part[540];
        int k=snprintf(part,sizeof(part),
            "['IVAO-%s',%.8f,%.8f,%.3f,%.4f,%.4f,%.4f,'%s'],\n",
            p->callsign,p->xpmp_lat,p->xpmp_lon,p->xpmp_alt_ft,
            p->xpmp_heading,p->xpmp_pitch,p->xpmp_roll,model);
        if(k<0||(size_t)k>=sizeof(part)||!emit(code,sizeof(code),&off,part))break;
        count++;
    }
    if(!emit(code,sizeof(code),&off,
        "];\n"
        "var existing={};\n"
        "var stale=[];\n"
        "foreach(var ai; root.getChildren('aircraft')) {\n"
        " var c=ai.getNode('callsign');\n"
        " if(c==nil) continue;\n"
        " var cs=c.getValue();\n"
        " if(cs==nil or substr(cs,0,5)!='IVAO-') continue;\n"
        " if(existing[cs]==nil) existing[cs]=ai; else append(stale,ai);\n"
        "}\n"
        "foreach(var p; flights) {\n"
        " var cs=p[0];\n"
        " if(active[cs]!=nil) continue;\n"
        " active[cs]=1; seen+=1;\n"
        " var ai=existing[cs];\n"
        " if(ai==nil) {\n"
        "  var a=props.Node.new({type:'aircraft',model:p[7],latitude:p[1],longitude:p[2],altitude:p[3],heading:p[4],pitch:p[5],roll:p[6],speed:0,'speed-ktas':0,'vertical-speed-fps':0,'on-ground':0,callsign:cs});\n"
        "  if(fgcommand('add-aiobject',a)) created+=1; else errors+=1;\n"
        " } else {\n"
        "  var stored=ai.getNode('ivao-bridge-model',1);\n"
        "  var last=stored.getValue();\n"
        "  if(last==nil) { stored.setValue(p[7]); }\n"
        "  else if(last!=p[7]) {\n"
        "   var idnode=ai.getNode('id');\n"
        "   var rid=(idnode!=nil and idnode.getValue()!=nil)?idnode.getValue():ai.getIndex();\n"
        "   if(fgcommand('remove-aiobject',props.Node.new({id:rid}))) {\n"
        "    var a=props.Node.new({type:'aircraft',model:p[7],latitude:p[1],longitude:p[2],altitude:p[3],heading:p[4],pitch:p[5],roll:p[6],speed:0,'speed-ktas':0,'vertical-speed-fps':0,'on-ground':0,callsign:cs});\n"
        "    if(fgcommand('add-aiobject',a)) {changed+=1;created+=1;} else errors+=1;\n"
        "   } else errors+=1;\n"
        "   continue;\n"
        "  }\n"
        "  ai.getNode('speed-ktas',1).setDoubleValue(0);\n"
        "  ai.getNode('speed',1).setDoubleValue(0);\n"
        "  ai.getNode('velocities',1).getNode('vertical-speed-fps',1).setDoubleValue(0);\n"
        "  ai.getNode('velocities',1).getNode('speed-down-fps',1).setDoubleValue(0);\n"
        "  ai.getNode('controls',1).getNode('flight',1).getNode('vertical-mode',1).setValue('pitch');\n"
        "  ai.getNode('controls',1).getNode('flight',1).getNode('target-pitch',1).setDoubleValue(p[5]);\n"
        "  var pos=ai.getNode('position',1);\n"
        "  var ori=ai.getNode('orientation',1);\n"
        "  pos.getNode('latitude-deg',1).setDoubleValue(p[1]);\n"
        "  pos.getNode('longitude-deg',1).setDoubleValue(p[2]);\n"
        "  pos.getNode('altitude-ft',1).setDoubleValue(p[3]);\n"
        "  ori.getNode('true-heading-deg',1).setDoubleValue(p[4]);\n"
        "  ori.getNode('pitch-deg',1).setDoubleValue(p[5]);\n"
        "  ori.getNode('roll-deg',1).setDoubleValue(p[6]);\n"
        "  updated+=1;\n"
        " }\n"
        "}\n"
        "foreach(var ai; root.getChildren('aircraft')) {\n"
        " var c=ai.getNode('callsign');\n"
        " if(c==nil) continue;\n"
        " var cs=c.getValue();\n"
        " if(cs!=nil and substr(cs,0,5)=='IVAO-' and active[cs]==nil) append(stale,ai);\n"
        "}\n"
        "foreach(var ai; root.getChildren('static')) {\n"
        " var c=ai.getNode('callsign');\n"
        " if(c==nil) continue;\n"
        " var cs=c.getValue();\n"
        " if(cs!=nil and substr(cs,0,5)=='IVAO-') append(stale,ai);\n"
        "}\n"
        "var removedIds={};\n"
        "foreach(var ai; stale) {\n"
        " var idnode=ai.getNode('id');\n"
        " var rid=(idnode!=nil and idnode.getValue()!=nil)?idnode.getValue():ai.getIndex();\n"
        " var key=ai.getName()~':'~rid;\n"
        " if(removedIds[key]!=nil) continue;\n"
        " removedIds[key]=1;\n"
        " if(fgcommand('remove-aiobject',props.Node.new({id:rid}))) removed+=1; else errors+=1;\n"
        "}\n"
        "setprop('/sim/ivao-bridge/last-seen',seen);\n"
        "setprop('/sim/ivao-bridge/last-created',created);\n"
        "setprop('/sim/ivao-bridge/last-updated',updated);\n"
        "setprop('/sim/ivao-bridge/last-removed',removed);\n"
        "setprop('/sim/ivao-bridge/last-model-changes',changed);\n"
        "setprop('/sim/ivao-bridge/last-errors',errors);\n"
        "setprop('/sim/ivao-bridge/last-orphans',0);\n"
        "setprop('/sim/ivao-bridge/status','finished-v50');\n"
        "\r\n##EOF##\r\n"))return;
    /* v42: persistent Telnet session. XPMP2 supplies each new pose; no
       interpolation is performed here. Diagnostic round trips previously
       blocked this worker for multiple seconds per update. */
    if(s->sock<0) {
        s->sock=connect_fg(s->port);
        if(s->sock<0) {
            s->failures++;
            fprintf(stderr,"[FG TRAFFIC] v42 TCP connect failed errno=%d\n",errno);
            return;
        }
        char greeting[2048];
        size_t gn=read_reply(s->sock,greeting,sizeof(greeting),100);
        fprintf(stderr,"[FG TRAFFIC] v42 persistent TCP connected port=%d greeting=%zu bytes\n",s->port,gn);
    }
    if(write_all(s->sock,code,off)!=0) {
        fprintf(stderr,"[FG TRAFFIC] v42 TCP write failed errno=%d; reconnect next frame\n",errno);
        close(s->sock);s->sock=-1;s->failures++;
        return;
    }
    s->failures=0;
    ++s->packets;
    /* Drain interpreter output without waiting for a property query. The
       FlightGear Telnet protocol may acknowledge a Nasal block asynchronously. */
    char response[4096];
    size_t n=read_reply(s->sock,response,sizeof(response),20);
    if(n && (strstr(response,"Error") || strstr(response,"error")))
        fprintf(stderr,"[FG TRAFFIC] v42 Nasal error: %.1500s\n",response);
    if(s->packets<=3 || s->packets%100==0)
        fprintf(stderr,"[FG TRAFFIC] v42 XPMP direct packet=%u planes=%u bytes=%zu reply=%zu\n",s->packets,count,off,n);
}
