#define _POSIX_C_SOURCE 200809L
#include "ivao_fsd.h"
#include <arpa/inet.h>
#include <dlfcn.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>

static int valid_callsign(const char *s) { size_t n=strlen(s); if(n<2 || n>16)return 0; for(size_t i=0;i<n;i++) if(!isalnum((unsigned char)s[i]) && s[i]!='_' && s[i]!='-')return 0; return 1; }
static int number(const char *s, double *v) { char *end=NULL; if(!s || !*s)return 0; *v=strtod(s,&end);return end && *end==0 && isfinite(*v); }
static int sq_valid(const char *s) { if(strlen(s)!=4)return 0;for(int i=0;i<4;i++)if(s[i]<'0'||s[i]>'7')return 0;return 1; }
void fsd_expire(struct fsd_traffic *t, time_t now, unsigned max_age_seconds) {
    for(size_t i=0; i<t->count;) {
        if(t->pilots[i].last_seen && now>t->pilots[i].last_seen &&
           (unsigned long)(now-t->pilots[i].last_seen)>max_age_seconds) {
            fprintf(stderr,"[FSD C] EXPIRE %s (no updates for %us)\n",t->pilots[i].callsign,max_age_seconds);
            if(i+1<t->count)memmove(&t->pilots[i],&t->pilots[i+1],(t->count-i-1)*sizeof(t->pilots[0]));
            t->count--; t->expired++;
        } else i++;
    }
}
static void parse_line(struct fsd_traffic *t, char *line) {
    /* FSD client deletion: #DP:CALLSIGN:... ; never retain disconnected traffic. */
    if(strncmp(line,"#DP",3)==0) {
        /* IVAO sends #DPCALLSIGN:VID (no colon after #DP). */
        const char *call=line+3;
        if(*call==':')call++; size_t len=strcspn(call,":\r\n");
        if(len>0 && len<24) for(size_t i=0;i<t->count;i++) {
            if(strlen(t->pilots[i].callsign)==len && strncmp(t->pilots[i].callsign,call,len)==0) {
                fprintf(stderr,"[FSD C] DELETE %s\n",t->pilots[i].callsign);
                if(i+1<t->count)memmove(&t->pilots[i],&t->pilots[i+1],(t->count-i-1)*sizeof(t->pilots[0]));
                t->count--; t->deletes++; break;
            }
        }
        return;
    }
    /* IVAO model info: -PDCALLSIGN:RECEIVER:ICAO4+LIVERY.
       Capture confirmed -PDEJU625L:AFR1244_OBS:A320EJUWL. */
    if(strncmp(line,"-PD",3)==0) {
        char *call=line+3;
        if(*call==':')call++;
        char *sep=strchr(call,':'); if(!sep)return;
        size_t n=(size_t)(sep-call); if(n<2||n>=24)return;
        char *model=strrchr(sep+1,':'); if(!model)return;model++;
        size_t m=strcspn(model,":\r\n"); if(m<4||m>=32)return;
        for(size_t i=0;i<m;i++)if(!isalnum((unsigned char)model[i]))return;
        for(size_t i=0;i<t->count;i++) {
            if(strlen(t->pilots[i].callsign)==n && strncmp(t->pilots[i].callsign,call,n)==0) {
                memcpy(t->pilots[i].aircraft_code,model,m);
                t->pilots[i].aircraft_code[m]=0;
                return;
            }
        }
        /* Model information may arrive before first position: cache it. */
        if(t->count>=FSD_MAX_PILOTS)return;
        struct fsd_pilot *pl=&t->pilots[t->count++];memset(pl,0,sizeof(*pl));
        memcpy(pl->callsign,call,n);pl->callsign[n]=0;
        memcpy(pl->aircraft_code,model,m);pl->aircraft_code[m]=0;
        return;
    }
    if(line[0]=='%' ) {t->atc_frames++;return;}
    if(line[0]!='@' || (line[1]!='N' && line[1]!='S' && line[1]!='Y') || line[2]!=':')return;
    char *parts[16]={0}; size_t n=0; char *p=line; parts[n++]=p;
    while(*p && n<16) { if(*p==':') { *p=0;parts[n++]=p+1; } p++; }
    if(n<9 || !valid_callsign(parts[1]) || !sq_valid(parts[2])) {t->rejected++;return;}
    double rating,lat,lon,alt,gs;
    if(!number(parts[3],&rating) || rating<0||rating>20||!number(parts[4],&lat)||!number(parts[5],&lon)||!number(parts[6],&alt)||!number(parts[7],&gs)||fabs(lat)>90||fabs(lon)>180||alt < -2000||alt>150000||gs<0||gs>2000) { t->rejected++;return; }
    /* Standard @ packet: PBH is the uint32 immediately after GS. */
    uint32_t packed=0; int has_pbh=0; double pitch=0,bank=0,heading=0;
    if(n>=9 && parts[8] && *parts[8]) {
        errno=0; char *end=NULL;
        unsigned long long v=strtoull(parts[8],&end,10);
        if(!errno && end!=parts[8] && *end==0 && v<=UINT32_MAX) {
            packed=(uint32_t)v; has_pbh=1;
            pitch=((packed>>22)&1023u)*360.0/1023.0;
            bank =((packed>>12)&1023u)*360.0/1023.0;
            heading=((packed>>2)&1023u)*360.0/1023.0;
            if(pitch>180.0)pitch-=360.0;
            if(bank>180.0)bank-=360.0;
        }
    }
    size_t i=0;for(;i<t->count;i++)if(strcmp(t->pilots[i].callsign,parts[1])==0)break;
    if(i==t->count) {if(t->count>=FSD_MAX_PILOTS)return;t->count++;memset(&t->pilots[i],0,sizeof(t->pilots[i]));}
    struct fsd_pilot *pl=&t->pilots[i];snprintf(pl->callsign,sizeof(pl->callsign),"%s",parts[1]);
    memcpy(pl->squawk,parts[2],4);pl->squawk[4]=0;pl->mode=line[1];pl->lat=lat;pl->lon=lon;pl->altitude_ft=alt;pl->ground_speed_kt=gs;pl->has_pbh=has_pbh;pl->packed_pbh=packed;pl->pitch_deg=pitch;pl->bank_deg=bank;pl->heading_deg=heading;pl->updates++;pl->last_seen=time(NULL);t->position_frames++;
}
void fsd_feed(struct fsd_traffic *t,const unsigned char *b,size_t len) {
    for(size_t i=0;i<len;i++) {
        unsigned char c=b[i];
        if(c=='\n' || c=='\r') {if(t->pending_len){t->pending[t->pending_len]=0;parse_line(t,t->pending);t->pending_len=0;}continue;}
        if(t->pending_len>=sizeof(t->pending)-1) { t->pending_len=0;t->rejected++;continue; }
        if(c==0 || (c<32 && c!='\t'))continue;
        t->pending[t->pending_len++]=(char)c;
    }
}
void fsd_print(const struct fsd_traffic *t,FILE *out) {
    fprintf(out,"[FSD C] pilots=%zu position_updates=%llu atc_updates=%llu rejected=%llu deletes=%llu expired=%llu\n",t->count,(unsigned long long)t->position_frames,(unsigned long long)t->atc_frames,(unsigned long long)t->rejected,(unsigned long long)t->deletes,(unsigned long long)t->expired);
    for(size_t i=0;i<t->count;i++){const struct fsd_pilot *p=&t->pilots[i];fprintf(out,"[FSD C] %-16s lat=%.6f lon=%.6f alt=%.0fft gs=%.0fkt squawk=%s updates=%llu\n",p->callsign,p->lat,p->lon,p->altitude_ft,p->ground_speed_kt,p->squawk,(unsigned long long)p->updates);}
}
/* Ethernet and BSD NULL/LOOP; IPv4/TCP, no fragmentation. Classic pcap only. */
static unsigned short be16(const unsigned char *p){return (unsigned short)((p[0]<<8)|p[1]);}
static uint32_t be32(const unsigned char *p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
struct stream_state {int valid;unsigned char src[4],dst[4];unsigned short sport,dport;uint32_t next_seq;};
static int extract(struct fsd_traffic *t,struct stream_state *state,int link,const unsigned char *p,size_t n,unsigned short port){
    size_t o=0;int ipv4=0;
    if(link==1) {if(n<14)return 0;int ty=be16(p+12);o=14;while((ty==0x8100||ty==0x88a8)&&n>=o+4){ty=be16(p+o+2);o+=4;}ipv4=ty==0x0800;}
    else if(link==0 || link==108) {if(n<4)return 0;o=4;ipv4=(p[o]>>4)==4;}
    else if(link==12 || link==101){ipv4=(p[0]>>4)==4;}
    if(!ipv4 || n<o+20)return 0;
    size_t ip_start=o, iplen=(size_t)(p[o]&15)*4;
    size_t total=(size_t)be16(p+o+2);
    if(iplen<20||total<iplen+20||n<o+iplen+20||p[o+9]!=6||(be16(p+o+6)&0x3fff))return 0;
    if(n>ip_start+total)n=ip_start+total;
    if(n<o+iplen+20)return 0;
    const unsigned char *src=p+o+12,*dst=p+o+16;o+=iplen;
    unsigned short sport=be16(p+o),dport=be16(p+o+2);if(sport!=port)return 0;
    uint32_t seq=be32(p+o+4);size_t tcplen=(size_t)(p[o+12]>>4)*4;if(tcplen<20||n<o+tcplen)return 0;
    size_t start=o+tcplen;size_t sz=n-start;
    if(!sz)return 0;
    if(!state->valid || state->sport!=sport||state->dport!=dport||memcmp(state->src,src,4)||memcmp(state->dst,dst,4)) {
       memset(state,0,sizeof(*state));state->valid=1;state->sport=sport;state->dport=dport;memcpy(state->src,src,4);memcpy(state->dst,dst,4);state->next_seq=seq;
    }
    if((int32_t)(seq-state->next_seq)<0){size_t skip=(size_t)(state->next_seq-seq);if(skip>=sz)return 0;start+=skip;sz-=skip;}
    else if(seq!=state->next_seq){t->pending_len=0;}
    fsd_feed(t,p+start,sz);state->next_seq=seq+(uint32_t)(n-(o+tcplen)); return 1;
}
static uint32_t u32(const unsigned char *p,int little){if(little)return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);return be32(p);}
int fsd_capture_pcap(struct fsd_traffic *t,const char *filename,unsigned short port){
    FILE *f=fopen(filename,"rb");if(!f){perror("[FSD C] open pcap");return -1;}
    unsigned char h[24];if(fread(h,1,24,f)!=24){fclose(f);return -1;}
    int little=h[0]==0xd4&&h[1]==0xc3&&h[2]==0xb2&&h[3]==0xa1;
    int big=h[0]==0xa1&&h[1]==0xb2&&h[2]==0xc3&&h[3]==0xd4;
    if(!little&&!big){fprintf(stderr,"[FSD C] unsupported PCAP magic\n");fclose(f);return -1;}
    int link=(int)u32(h+20,little);struct stream_state st={0};unsigned char rh[16];
    while(fread(rh,1,16,f)==16){uint32_t n=u32(rh+8,little);if(n>2*1024*1024)break;unsigned char *buf=malloc(n?n:1);if(!buf)break;if(fread(buf,1,n,f)!=n){free(buf);break;}extract(t,&st,link,buf,n,port);free(buf);}
    fclose(f);return 0;
}
/* libpcap is loaded dynamically: the same bridge works without libpcap headers. */
struct pcap_pkthdr_local { struct timeval ts; unsigned int caplen,len; };
/* macOS timeval has long tv_sec and int tv_usec on LP64; header layout is padded. */
int fsd_capture_live(struct fsd_traffic *t,const char *iface,unsigned short port,volatile int *stop){
    void *lib=dlopen("/usr/lib/libpcap.A.dylib",RTLD_NOW|RTLD_LOCAL);
    if(!lib)lib=dlopen("libpcap.so.1",RTLD_NOW|RTLD_LOCAL);
    if(!lib){fprintf(stderr,"[FSD C] libpcap unavailable: %s\n",dlerror());return -1;}
    void *(*open_live)(const char*,int,int,int,char*)=dlsym(lib,"pcap_open_live");
    int (*datalink)(void*)=dlsym(lib,"pcap_datalink");
    int (*next_ex)(void*,void**,const unsigned char**)=dlsym(lib,"pcap_next_ex");
    void (*close_pcap)(void*)=dlsym(lib,"pcap_close");
    const char *(*geterr)(void*)=dlsym(lib,"pcap_geterr");
    if(!open_live||!datalink||!next_ex||!close_pcap){fprintf(stderr,"[FSD C] incomplete libpcap API\n");dlclose(lib);return -1;}
    char err[256]={0};void *pc=open_live(iface,65535,0,500,err);
    if(!pc){fprintf(stderr,"[FSD C] cannot open interface %s: %s\n",iface,err);dlclose(lib);return -1;}
    int link=datalink(pc);struct stream_state st={0};
    unsigned long long packets=0,server_segments=0;
    time_t last_report=time(NULL);
    fprintf(stderr,"[FSD C] CAPTURE READY interface=%s linktype=%d server_tcp_port=%u\n",iface,link,port);
    fflush(stderr);
    while(!*stop){
        void *hdr=NULL;const unsigned char *bytes=NULL;
        int rc=next_ex(pc,&hdr,&bytes);
        if(rc<0){fprintf(stderr,"[FSD C] capture ended rc=%d: %s\n",rc,geterr?geterr(pc):"error");break;}
        if(rc==1 && hdr && bytes){
            const struct pcap_pkthdr_local *record=(const struct pcap_pkthdr_local *)hdr;
            unsigned int caplen=record->caplen;
            packets++;
            if(caplen>0 && caplen<=65535){
                uint64_t before=t->position_frames;
                if(extract(t,&st,link,bytes,caplen,port))server_segments++;
                if(t->position_frames!=before){
                    fprintf(stderr,"[FSD C] PILOT UPDATE total=%llu unique=%zu\n",(unsigned long long)t->position_frames,t->count);
                    for(size_t i=0;i<t->count;i++)if(t->pilots[i].updates) {
                        const struct fsd_pilot *pl=&t->pilots[i];
                        fprintf(stderr,"[FSD C] PILOT %s lat=%.6f lon=%.6f alt=%.0fft gs=%.0fkt xpdr=%s\n",pl->callsign,pl->lat,pl->lon,pl->altitude_ft,pl->ground_speed_kt,pl->squawk);
                    }
                    fflush(stderr);
                }
            }
        }
        time_t now=time(NULL);
        if(now-last_report>=5){
            fsd_expire(t,now,90);
            fprintf(stderr,"[FSD C] LIVE packets=%llu server_segments=%llu pilot_updates=%llu atc_updates=%llu rejected=%llu deletes=%llu expired=%llu pending=%zu linktype=%d interface=%s\n",
                packets,server_segments,(unsigned long long)t->position_frames,(unsigned long long)t->atc_frames,(unsigned long long)t->rejected,(unsigned long long)t->deletes,(unsigned long long)t->expired,t->pending_len,link,iface);
            fflush(stderr); last_report=now;
        }
    }
    fprintf(stderr,"[FSD C] CAPTURE STOP packets=%llu server_segments=%llu pilots=%zu positions=%llu\n",packets,server_segments,t->count,(unsigned long long)t->position_frames);
    fflush(stderr);
    close_pcap(pc);dlclose(lib);return 0;
}
