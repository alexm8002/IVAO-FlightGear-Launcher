#include "ivao_fsd.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void){
 struct fsd_traffic t={0};
 const char *pd="-PDAFR59TU:AFR1244_OBS:A320AFR\r\n";
 fsd_feed(&t,(const unsigned char*)pd,strlen(pd));
 assert(t.count==1 && strcmp(t.pilots[0].aircraft_code,"A320AFR")==0);
 const char *position="@N:AFR59TU:1000:4:45.52534400:5.11204200:3278:148:4261683860:-289\r\n";
 fsd_feed(&t,(const unsigned char*)position,strlen(position));
 assert(t.count==1 && t.pilots[0].updates==1 && strcmp(t.pilots[0].aircraft_code,"A320AFR")==0);
 const char *del="#DPAFR59TU:12345\r\n";
 fsd_feed(&t,(const unsigned char*)del,strlen(del));
 assert(t.count==0);
 puts("PASS: -PD before @N and #DP model matching lifecycle");return 0;
}
