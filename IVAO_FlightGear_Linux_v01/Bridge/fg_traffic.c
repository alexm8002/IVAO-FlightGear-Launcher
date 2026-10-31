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
static void load_models(void) {
    FILE *f=fopen("fg_models.tsv","r");if(!f){fprintf(stderr,"[FG MODEL] fg_models.tsv missing; fallback A320\n");return;}
    char line[512];
    while(nmodels<4096&&fgets(line,sizeof(line),f)) {
        char *type=strtok(line,"\t\r\n");char *airline=strtok(NULL,"\t\r\n");char *path=strtok(NULL,"\t\r\n");
        if(!type||!airline||!path||strlen(type)!=4||strncmp(path,"Aircraft/",9)!=0||strstr(path,".."))continue;
        struct model_entry *m=&models[nmodels++];
        snprintf(m->icao,sizeof(m->icao),"%.4s",type);
        snprintf(m->airline,sizeof(m->airline),"%.23s",airline);
        snprintf(m->path,sizeof(m->path),"%.239s",path);
    }
    fclose(f);fprintf(stderr,"[FG MODEL] indexed %zu aircraft XMLs\n",nmodels);
}
static const char *choose_model(const struct fsd_pilot *p) {
    static const char *fallback="Aircraft/A320/A320-default.xml";
    if(strlen(p->aircraft_code)<4)return fallback;
    const char *code=p->aircraft_code;
    const char *airline=code+4;
    /* IVAO and FG liveries have overlapping airline codes for easyJet/Wizz. */
    if(strncmp(airline,"EJU",3)==0 || strncmp(airline,"EZS",3)==0)airline="EZY";
    else if(strncmp(airline,"WMT",3)==0)airline="WZZ";
    const char *best=NULL;
    int best_score=-1;
    for(size_t i=0;i<nmodels;i++) {
        const struct model_entry *m=&models[i];
        if(strncmp(m->icao,code,4)!=0)continue;
        int score=0;
        if(strlen(airline)>=3 && strncmp(m->airline,airline,3)==0)score=100;
        else if(strcmp(m->airline,"---")==0) {
            const char *name=strrchr(m->path,'/');name=name?name+1:m->path;
            score=(strstr(name,"-Default.") || strstr(name,"-default."))?70:20;
        } else score=30;
        /* No random livery if a generic one is available for this type. */
        if(score>best_score) {best_score=score;best=m->path;}
    }
    return best?best:fallback;
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
void fg_traffic_close(struct fg_traffic_sender *s){fg_traffic_clear(s);}
void fg_traffic_tick(struct fg_traffic_sender *s,const struct fsd_traffic *t,time_t now) {
    if(!s->enabled||(s->failures&&now-s->last_connect<10))return;
    struct timespec tick={0};clock_gettime(CLOCK_MONOTONIC,&tick);
    double since=(double)(tick.tv_sec-s->last_tick.tv_sec)+1e-9*(double)(tick.tv_nsec-s->last_tick.tv_nsec);
    if(s->last_tick.tv_sec && since<0.50)return;
    s->last_tick=tick;s->last_send=now;s->last_connect=now;
    char code[65536];size_t off=0;unsigned count=0;
    /* Compact Nasal code: data is serialized once, logic runs once.
       The old code expanded ~600 bytes per pilot and exceeded the FlightGear
       Telnet/Nasal command parser for normal nearby traffic counts. */
    if(!emit(code,sizeof(code),&off,"nasal\r\n"
        "var updated=0; var created=0; var removed=0; var errors=0; var seen=0; var orphans=0;\n"
        "var active={};\n"
        "setprop('/sim/ivao-bridge/status','started');\n"
        "var root=props.globals.getNode('/ai/models',1);\n"
        "var flights=[\n"))return;
    for(size_t i=0;i<t->count&&count<32;i++) {
        const struct fsd_pilot *p=&t->pilots[i];
        if(!p->updates||p->last_seen<now-45||!valid_cs(p->callsign)||
            !isfinite(p->lat)||!isfinite(p->lon)||!isfinite(p->altitude_ft)||
            fabs(p->lat)>90||fabs(p->lon)>180||
            (fabs(p->lat)<0.01&&fabs(p->lon)<0.01)||
            p->altitude_ft< -2000||p->altitude_ft>100000)continue;
        double latitude=p->lat, longitude=p->lon;
        double age=difftime(now,p->last_seen);
        if(age>0.0 && age<=4.0 && p->ground_speed_kt>3.0 && p->has_pbh) {
            double nm=p->ground_speed_kt*age/3600.0;
            double rad=p->heading_deg*(3.14159265358979323846/180.0);
            latitude += nm*cos(rad)/60.0;
            double coslat=cos(p->lat*(3.14159265358979323846/180.0));
            if(fabs(coslat)>0.01)longitude += nm*sin(rad)/(60.0*coslat);
            if(longitude>180.0)longitude-=360.0;
            if(longitude< -180.0)longitude+=360.0;
        }
        const char *model=choose_model(p);
        char part[640];
        int k=snprintf(part,sizeof(part),
            "['IVAO-%s',%.8f,%.8f,%.2f,%.4f,%.4f,%.4f,'%s'],\n",
            p->callsign,latitude,longitude,p->altitude_ft,
            p->heading_deg,p->pitch_deg,p->bank_deg,model);
        if(k<0||(size_t)k>=sizeof(part)||!emit(code,sizeof(code),&off,part))break;
        count++;
    }
    if(!emit(code,sizeof(code),&off,
        "];\n"
        "foreach(var p; flights) {\n"
        "  var cs=p[0]; seen+=1; active[cs]=1;\n"
        "  var found=nil;\n"
        "  foreach(var ai; root.getChildren('aircraft')) {\n"
        "    var c=ai.getNode('callsign');\n"
        "    if(c!=nil and c.getValue()==cs) {found=ai;break;}\n"
        "  }\n"
        "  if(found==nil) {\n"
        "    var args=props.Node.new({type:'aircraft',model:p[7],"
        "latitude:p[1],longitude:p[2],altitude:p[3],heading:p[4],callsign:cs});\n"
        "    if(fgcommand('add-aiobject',args)) created+=1; else errors+=1;\n"
        "  } else {\n"
        "    found.getNode('position/latitude-deg',1).setDoubleValue(p[1]);\n"
        "    found.getNode('position/longitude-deg',1).setDoubleValue(p[2]);\n"
        "    found.getNode('position/altitude-ft',1).setDoubleValue(p[3]);\n"
        "    found.getNode('orientation/true-heading-deg',1).setDoubleValue(p[4]);\n"
        "    found.getNode('orientation/pitch-deg',1).setDoubleValue(p[5]);\n"
        "    found.getNode('orientation/roll-deg',1).setDoubleValue(p[6]);\n"
        "    updated+=1;\n"
        "  }\n"
        "}\n"
        "foreach(var ai; root.getChildren('aircraft')) {\n"
        "  var cn=ai.getNode('callsign');\n"
        "  if(cn==nil) continue;\n"
        "  var cs=cn.getValue();\n"
        "  if(cs==nil or substr(cs,0,5)!='IVAO-') continue;\n"
        "  if(active[cs]!=nil) continue;\n"
        "  var idnode=ai.getNode('id');\n"
        "  var rid=(idnode!=nil and idnode.getValue()!=nil) ? idnode.getValue() : ai.getIndex();\n"
        "  if(fgcommand('remove-aiobject',props.Node.new({id:rid}))) removed+=1; else {errors+=1;orphans+=1;}\n"
        "}\n"
        "setprop('/sim/ivao-bridge/last-removed',removed);\n"
        "setprop('/sim/ivao-bridge/last-seen',seen);\n"
        "setprop('/sim/ivao-bridge/last-created',created);\n"
        "setprop('/sim/ivao-bridge/last-updated',updated);\n"
        "setprop('/sim/ivao-bridge/last-errors',errors);\n"
        "setprop('/sim/ivao-bridge/last-orphans',orphans);\n"
        "setprop('/sim/ivao-bridge/status','finished');\n"
        "\r\n##EOF##\r\n"))return;
    int fd=connect_fg(s->port);
    if(fd<0){s->failures++;fprintf(stderr,"[FG TRAFFIC] TCP connect 127.0.0.1:%d failed errno=%d\n",s->port,errno);return;}
    char response[16384];(void)read_reply(fd,response,sizeof(response),250);
    /* Same framing as the standalone, successful test: CRLF. No brittle
       dependency on the textual appearance of elapsed-sec in GET output. */
    if(write_all(fd,"get /position/latitude-deg\r\n",strlen("get /position/latitude-deg\r\n"))!=0){
        s->failures++;fprintf(stderr,"[FG TRAFFIC] GET send failed\n");close(fd);return;
    }
    size_t n=read_reply(fd,response,sizeof(response),900);
    if(!n){s->failures++;fprintf(stderr,"[FG TRAFFIC] GET reply empty on TCP %d; Nasal not submitted\n",s->port);close(fd);return;}
    if(write_all(fd,code,off)!=0){s->failures++;fprintf(stderr,"[FG TRAFFIC] Nasal send failed errno=%d\n",errno);close(fd);return;}
    /* Wait for the Nasal interpreter to complete before issuing GET. */
    n=read_reply(fd,response,sizeof(response),700);
    if(n && strstr(response,"Error"))fprintf(stderr,"[FG TRAFFIC] Nasal error: %.300s\n",response);
    const char *queries="get /sim/ivao-bridge/status\r\n"
        "get /sim/ivao-bridge/last-seen\r\n"
        "get /sim/ivao-bridge/last-created\r\n"
        "get /sim/ivao-bridge/last-updated\r\n"
        "get /sim/ivao-bridge/last-removed\r\n"
        "get /sim/ivao-bridge/last-errors\r\n"
        "get /sim/ivao-bridge/last-orphans\r\n"
        "get /ai/models/count\r\n";
    if(write_all(fd,queries,strlen(queries))!=0){s->failures++;close(fd);return;}
    n=read_reply(fd,response,sizeof(response),1200);
    fprintf(stderr,"[FG TRAFFIC] batch=%u aircraft=%u reply_bytes=%zu\n",++s->packets,count,n);
    if(n){
        char *save=NULL;char *ln=strtok_r(response,"\r\n",&save);
        while(ln){
            if(strstr(ln,"ivao-bridge")||strstr(ln,"count")||strstr(ln,"Error")||strstr(ln,"error"))
                fprintf(stderr,"[FG TRAFFIC] Reply: %.220s\n",ln);
            ln=strtok_r(NULL,"\r\n",&save);
        }
    } else fprintf(stderr,"[FG TRAFFIC] no confirmation of Nasal execution\n");
    (void)write_all(fd,"quit\r\n",6);close(fd);s->failures=0;
}
