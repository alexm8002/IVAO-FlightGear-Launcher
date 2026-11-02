#include "ivao_fsd.h"
#include <stdio.h>
#include <string.h>
int main(void){
 struct fsd_traffic t={0};
 const char *a="@N:AFR123:2000:2:43.655622:7.281726:2192:209:0\r\n";
 fsd_feed(&t,(const unsigned char*)a,13);
 fsd_feed(&t,(const unsigned char*)a+13,strlen(a)-13);
 if(t.count!=1||t.position_frames!=1||strcmp(t.pilots[0].callsign,"AFR123")||t.pilots[0].altitude_ft!=2192)return 1;
 fsd_feed(&t,(const unsigned char*)"%LFMN_TWR:118.705:0:0:0:43.665280:7.215000\r\n",49);
 if(t.atc_frames!=1)return 2;
 /* Verify PBH bit extraction and signed normalization. */
 const uint32_t packed=((uint32_t)1023<<22)|((uint32_t)256<<12)|((uint32_t)512<<2);
 char frame[256]; snprintf(frame,sizeof(frame),"@N:TEST77:2000:2:43.6:7.2:3000:130:%u:0\r\n",packed);
 memset(&t,0,sizeof(t)); fsd_feed(&t,(const unsigned char*)frame,strlen(frame));
 if(t.count!=1 || !t.pilots[0].has_pbh || t.pilots[0].packed_pbh!=packed)return 3;
 if(t.pilots[0].pitch_deg > 0.01 || t.pilots[0].pitch_deg < -0.01)return 4;
 if(t.pilots[0].heading_deg < 179.9 || t.pilots[0].heading_deg>180.4)return 5;
 puts("PASS: split FSD pilot frame, ATC classification and traffic state");return 0;
}
