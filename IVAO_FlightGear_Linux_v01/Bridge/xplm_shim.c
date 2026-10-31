#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <limits.h>
__attribute__((noreturn)) static void missing(const char *name) {
 fprintf(stderr,"[XPLM SHIM] First unimplemented function: %s\n",name);
 fflush(stderr); abort();
}
static int native_paths_enabled = 0;
__attribute__((visibility("default"))) void XPLMEnableFeature(const char *feature, int enable) {
 if (feature && strcmp(feature,"XPLM_USE_NATIVE_PATHS")==0) native_paths_enabled = !!enable;
 fprintf(stderr, "[XPLM SHIM] XPLMEnableFeature(feature=%s, enable=%d) native_paths=%d\n", feature ? feature : "(null)", enable, native_paths_enabled);
 fflush(stderr);
}
__attribute__((visibility("default"))) int XPLMIsFeatureEnabled(const char *feature) {
 int enabled = feature && strcmp(feature,"XPLM_USE_NATIVE_PATHS") == 0 ? native_paths_enabled : 0;
 fprintf(stderr,"[XPLM SHIM] XPLMIsFeatureEnabled(feature=%s) -> %d [native paths only; other features unavailable]\n",feature?feature:"(null)",enabled); fflush(stderr); return enabled;
}
typedef float (*XPLMFlightLoop_f)(float, float, int, void *);
static struct { XPLMFlightLoop_f callback; float interval; void *refcon; int active; } flight_loops[128];
static int flight_loop_count;
__attribute__((visibility("default"))) void XPLMRegisterFlightLoopCallback(XPLMFlightLoop_f cb, float interval, void *refcon) {
 if (!cb || flight_loop_count >= 128) { fprintf(stderr,"[XPLM SHIM] Invalid flight loop registration\n"); abort(); }
 int i=flight_loop_count++; flight_loops[i].callback=cb; flight_loops[i].interval=interval; flight_loops[i].refcon=refcon; flight_loops[i].active=1;
 fprintf(stderr,"[XPLM SHIM] XPLMRegisterFlightLoopCallback(callback=%p, interval=%g, refcon=%p) [stored, not scheduled]\n",(void*)cb,(double)interval,refcon); fflush(stderr);
}
__attribute__((visibility("default"))) void XPLMUnregisterFlightLoopCallback(XPLMFlightLoop_f cb, void *refcon) {
 int count=0;
 for (int i=0; i<flight_loop_count; ++i) if (flight_loops[i].active && flight_loops[i].callback==cb && flight_loops[i].refcon==refcon) { flight_loops[i].active=0; count++; }
 fprintf(stderr,"[XPLM SHIM] XPLMUnregisterFlightLoopCallback(callback=%p, refcon=%p) removed=%d\n",(void*)cb,refcon,count); fflush(stderr);
}
static unsigned long shim_frame_number = 0;
static double shim_elapsed = 0.0;
static double shim_last_calls[128];
static unsigned long shim_due_frames[128];
__attribute__((visibility("default"))) int XPLMShimRunFrame(double delta_seconds) {
 if(delta_seconds<=0 || delta_seconds>1.0) return -1;
 shim_frame_number++; shim_elapsed+=delta_seconds;
 int executed=0; int limit=flight_loop_count;
 for(int i=0;i<limit;i++) {
  if(!flight_loops[i].active || !flight_loops[i].callback || flight_loops[i].interval==0) continue;
  float iv=flight_loops[i].interval;
  if(iv<0 && shim_frame_number<shim_due_frames[i]) continue;
  if(iv>0 && (shim_elapsed-shim_last_calls[i])+1e-6<(double)iv) continue;
  XPLMFlightLoop_f cb=flight_loops[i].callback;
  float since=(float)(shim_elapsed-shim_last_calls[i]);
  shim_last_calls[i]=shim_elapsed;
  float next=cb(since,(float)delta_seconds,(int)shim_frame_number,flight_loops[i].refcon);
  executed++;
  if(!flight_loops[i].active) continue;
  flight_loops[i].interval=next;
  if(next<0) { unsigned long steps=(unsigned long)(-next); if(steps<1) steps=1; shim_due_frames[i]=shim_frame_number+steps; }
  else shim_due_frames[i]=shim_frame_number+1;
 }
 return executed;
}
typedef void * XPLMMenuID;
typedef void (*XPLMMenuHandler_f)(void *, void *);
static int plugins_menu_token;
static struct { int token; XPLMMenuHandler_f callback; void *refcon; int item_count; int check[128]; } menu_slots[32];
static int menu_count;
__attribute__((visibility("default"))) XPLMMenuID XPLMFindPluginsMenu(void) {
 fprintf(stderr,"[XPLM SHIM] XPLMFindPluginsMenu -> diagnostic handle\n");
 return &plugins_menu_token;
}
__attribute__((visibility("default"))) int XPLMAppendMenuItem(XPLMMenuID menu, const char *name, void *itemRef, int deprecated) {
 (void)itemRef; (void)deprecated;
 int index = -1;
 if (menu == (void *)&plugins_menu_token) { static int plugins_count; index=plugins_count++; }
 else for (int i=0;i<menu_count;i++) if (menu == (void *)&menu_slots[i]) { index=menu_slots[i].item_count++; break; }
 if(index<0 || index>=128) { fprintf(stderr,"[XPLM SHIM] invalid menu append\n"); abort(); }
 fprintf(stderr,"[XPLM SHIM] XPLMAppendMenuItem(menu=%p, name=%s) -> %d\n",menu,name?name:"(null)",index);
 return index;
}
__attribute__((visibility("default"))) XPLMMenuID XPLMCreateMenu(const char *name, XPLMMenuID parent, int parentItem, XPLMMenuHandler_f handler, void *refcon) {
 (void)parent; (void)parentItem;
 if(menu_count>=32) { fprintf(stderr,"[XPLM SHIM] menu limit reached\n"); abort(); }
 int i=menu_count++; menu_slots[i].token=i+1; menu_slots[i].callback=handler; menu_slots[i].refcon=refcon;
 fprintf(stderr,"[XPLM SHIM] XPLMCreateMenu(name=%s) -> %p\n",name?name:"(null)",(void *)&menu_slots[i]);
 return &menu_slots[i];
}
__attribute__((visibility("default"))) void XPLMCheckMenuItem(XPLMMenuID menu, int index, int check) {
 if (check<0 || check>2) { fprintf(stderr,"[XPLM SHIM] XPLMCheckMenuItem invalid state=%d\n",check); abort(); }
 for(int i=0;i<menu_count;i++) if(menu==(void *)&menu_slots[i]) {
  if(index<0 || index>=menu_slots[i].item_count || index>=128) {fprintf(stderr,"[XPLM SHIM] XPLMCheckMenuItem invalid index=%d\n",index); abort();}
  menu_slots[i].check[index]=check;
  fprintf(stderr,"[XPLM SHIM] XPLMCheckMenuItem(menu=%p,index=%d,state=%d) [HEADLESS]\n",menu,index,check); fflush(stderr); return;
 }
 fprintf(stderr,"[XPLM SHIM] XPLMCheckMenuItem unknown menu=%p index=%d\n",menu,index); abort();
}
__attribute__((visibility("default"))) void XPLMDestroyMenu(XPLMMenuID menu) {
 fprintf(stderr,"[XPLM SHIM] XPLMDestroyMenu(%p)\n",menu);
}
typedef void *XPLMDataRef;
typedef int (*XPLMGetDatai_f)(void *);
typedef void (*XPLMSetDatai_f)(void *, int);
typedef float (*XPLMGetDataf_f)(void *);
typedef void (*XPLMSetDataf_f)(void *, float);
typedef double (*XPLMGetDatad_f)(void *);
typedef void (*XPLMSetDatad_f)(void *, double);
typedef int (*XPLMGetDatavi_f)(void *, int *, int, int);
typedef void (*XPLMSetDatavi_f)(void *, int *, int, int);
typedef int (*XPLMGetDatavf_f)(void *, float *, int, int);
typedef void (*XPLMSetDatavf_f)(void *, float *, int, int);
typedef int (*XPLMGetDatab_f)(void *, void *, int, int);
typedef void (*XPLMSetDatab_f)(void *, void *, int, int);
static struct { char name[256]; int type; int writable; int active; void *read_refcon; void *write_refcon; XPLMGetDatai_f get_i; XPLMSetDatai_f set_i; XPLMGetDataf_f get_f; XPLMSetDataf_f set_f; XPLMGetDatad_f get_d; XPLMGetDatavi_f get_vi; } registered_datarefs[128];
static int registered_datarefs_count = 0;
__attribute__((visibility("default"))) XPLMDataRef XPLMRegisterDataAccessor(
 const char *name, int type, int writable,
 XPLMGetDatai_f ri, XPLMSetDatai_f wi, XPLMGetDataf_f rf, XPLMSetDataf_f wf,
 XPLMGetDatad_f rd, XPLMSetDatad_f wd, XPLMGetDatavi_f ria, XPLMSetDatavi_f wia,
 XPLMGetDatavf_f rfa, XPLMSetDatavf_f wfa, XPLMGetDatab_f rb, XPLMSetDatab_f wb,
 void *read_refcon, void *write_refcon) {
 (void)wd; (void)wia; (void)rfa; (void)wfa; (void)rb; (void)wb;
 if (registered_datarefs_count >= 128) { fprintf(stderr,"[XPLM SHIM] DataRef registry full\n"); abort(); }
 int n=registered_datarefs_count++;
 snprintf(registered_datarefs[n].name, sizeof(registered_datarefs[n].name), "%s", name?name:"(null)");
 registered_datarefs[n].type=type; registered_datarefs[n].writable=writable; registered_datarefs[n].active=1;
 registered_datarefs[n].get_i=ri; registered_datarefs[n].set_i=wi; registered_datarefs[n].get_f=rf; registered_datarefs[n].set_f=wf; registered_datarefs[n].get_d=rd; registered_datarefs[n].get_vi=ria;
 registered_datarefs[n].read_refcon=read_refcon; registered_datarefs[n].write_refcon=write_refcon;
 fprintf(stderr,"[XPLM SHIM] XPLMRegisterDataAccessor(name=%s,type=%d,writable=%d) -> %p\n",registered_datarefs[n].name,type,writable,(void*)&registered_datarefs[n]);
 fflush(stderr); return &registered_datarefs[n];
}
typedef struct { const char *name; int type; double value; } FGRef;
static FGRef fg_refs[] = {
 {"sim/flightmodel/position/latitude", 4, 0},
 {"sim/flightmodel/position/longitude", 4, 0},
 {"sim/flightmodel/position/elevation", 4, 0},
 {"sim/flightmodel/position/theta", 2, 0},
 {"sim/flightmodel/position/phi", 2, 0},
 {"sim/flightmodel/position/psi", 2, 0},
 {"sim/flightmodel/position/groundspeed", 2, 0},
 {"sim/flightmodel/misc/h_ind", 2, 0},
 {"sim/flightmodel/failures/onground_any", 1, 1},
 {"sim/cockpit2/radios/actuators/com1_frequency_hz_833", 1, 118000},
 {"sim/cockpit2/radios/actuators/com2_frequency_hz_833", 1, 121500},
 {"sim/cockpit/radios/transponder_code", 1, 1200},
 {"sim/cockpit/radios/transponder_mode", 1, 1},
 {"sim/cockpit2/radios/actuators/com1_power", 1, 1},
 {"sim/cockpit2/radios/actuators/com2_power", 1, 1},
 {"sim/cockpit2/radios/actuators/audio_selection_com1", 1, 1},
 {"sim/cockpit2/radios/actuators/audio_selection_com2", 1, 0},
 {"sim/graphics/view/window_height", 1, 900},
 {"sim/graphics/view/using_modern_driver", 1, 0},
 {"sim/network/dataout/is_external_visual", 1, 0},
 {"sim/network/dataout/is_multiplayer_session", 1, 0},
 {"sim/network/misc/network_time_sec", 2, 0},
 {"sim/time/framerate_period", 2, 0.05},
 {"sim/cockpit/misc/barometer_setting", 2, 29.92},
 {"sim/physics/earth_pressure_p", 2, 101325},
 {"sim/flightmodel2/position/pressure_altitude", 2, 0},
 {"sim/weather/aircraft/altimeter_temperature_error", 2, 0},
 {"sim/cockpit2/radios/actuators/com1_standby_frequency_hz_833", 1, 121500},
 {"sim/cockpit2/radios/actuators/com2_standby_frequency_hz_833", 1, 121500},
 {"sim/cockpit/switches/audio_panel_out", 1, 1},
 {"sim/cockpit2/radios/actuators/audio_volume_com1", 2, 0.5},
 {"sim/cockpit2/radios/actuators/audio_volume_com2", 2, 0.5},
 {"sim/cockpit/radios/transponder_id", 1, 0},
 {"sim/cockpit/autopilot/autopilot_mode", 1, 0},
 {"sim/cockpit/switches/gear_handle_status", 1, 1},
 {"sim/cockpit/electrical/beacon_lights_on", 1, 0},
 {"sim/cockpit/electrical/landing_lights_on", 1, 0},
 {"sim/cockpit/electrical/nav_lights_on", 1, 0},
 {"sim/cockpit/electrical/strobe_lights_on", 1, 0},
 {"sim/cockpit/electrical/taxi_light_on", 1, 0},
 {"sim/aircraft/engine/acf_num_engines", 1, 1},
 {"sim/flightmodel/controls/flaprat", 2, 0},
 {"sim/flightmodel/controls/sbrkrqst", 2, 0},
 {"sim/atc/atis_enabled", 1, 0},
 {"sim/operation/prefs/replay_mode", 1, 0},
 {"sim/network/dataout/track_external_visual", 16, 0},
 {"sim/flightmodel/engine/ENGN_propmode", 16, 0},
 {"sim/flightmodel/engine/ENGN_thro", 8, 0},
 {"sim/flightmodel/engine/ENGN_running", 16, 0},
 {"sim/aircraft/view/acf_tailnum", 32, 0},
};
static const int fg_count = (int)(sizeof(fg_refs)/sizeof(fg_refs[0]));
static pthread_mutex_t command_lock = PTHREAD_MUTEX_INITIALIZER;
static int pending_command[3] = {0,0,0};
static int command_value[3] = {0,0,0};
__attribute__((visibility("default"))) int XPLMShimPopCommand(int *kind, int *value) {
 if(!kind || !value) return 0;
 pthread_mutex_lock(&command_lock);
 for(int i=0;i<3;i++) if(pending_command[i]) { *kind=i+1; *value=command_value[i]; pending_command[i]=0; pthread_mutex_unlock(&command_lock); return 1; }
 pthread_mutex_unlock(&command_lock); return 0;
}
__attribute__((visibility("default"))) void XPLMShimUpdateTelemetry(const double *values, int count) {
 if(!values || count!=12) { fprintf(stderr,"[FG BRIDGE] invalid telemetry array\n"); abort(); }
 const int indexes[]={0,1,2,3,4,5,6,7,8,9,10,11};
 for(int i=0;i<count;i++) fg_refs[indexes[i]].value=values[i];
}
__attribute__((visibility("default"))) XPLMDataRef XPLMFindDataRef(const char *name) {
 if (name) for(int i=0;i<fg_count;i++) if(strcmp(fg_refs[i].name,name)==0) return (XPLMDataRef)&fg_refs[i];
 if (name) for (int i=0; i<registered_datarefs_count; ++i) {
  if (registered_datarefs[i].active && __builtin_strcmp(registered_datarefs[i].name,name)==0) {
   fprintf(stderr,"[XPLM SHIM] XPLMFindDataRef(name=%s) -> %p [PLUGIN OWNED]\n",name,(void*)&registered_datarefs[i]);
   fflush(stderr); return &registered_datarefs[i];
  }
 }
 fprintf(stderr,"[XPLM SHIM] XPLMFindDataRef(name=%s) -> NULL [not registered]\n",name?name:"(null)");
 fflush(stderr); return NULL;
}
__attribute__((visibility("default"))) void XPLMSetDatai(XPLMDataRef ref, int value) {
 if (!ref) { fprintf(stderr,"[XPLM SHIM] XPLMSetDatai(NULL,%d) [ignored; invalid DataRef]\n",value); fflush(stderr); return; }
 for(int i=0;i<fg_count;i++) if(ref==(XPLMDataRef)&fg_refs[i]) {
  if(!(fg_refs[i].type & 1)) {fprintf(stderr,"[XPLM SHIM] XPLMSetDatai wrong type: %s\n",fg_refs[i].name); abort();}
  if(i==9 || i==10) { if(value<118000 || value>136990) {fprintf(stderr,"[FG BRIDGE] invalid COM frequency %d\n",value); return;} }
  if(i==11) {int x=value; for(int k=0;k<4;k++,x/=10) if(x%10>7 || x<0) {fprintf(stderr,"[FG BRIDGE] invalid squawk %d\n",value); return;} if(x) return;}
  fg_refs[i].value=value;
  int cmd = i==9?0:i==10?1:i==11?2:-1;
  if(cmd>=0) { pthread_mutex_lock(&command_lock); command_value[cmd]=value; pending_command[cmd]=1; pthread_mutex_unlock(&command_lock); }
  fprintf(stderr,"[XPLM SHIM] XPLMSetDatai(%s,%d) -> simulator ref%s\n",fg_refs[i].name,value,cmd>=0?" [FG command queued]":""); fflush(stderr); return;
 }
 for (int i=0; i<registered_datarefs_count; ++i) {
  if (ref == (XPLMDataRef)&registered_datarefs[i]) {
   if (!registered_datarefs[i].active || !(registered_datarefs[i].type & 1) || !registered_datarefs[i].writable || !registered_datarefs[i].set_i) {
    fprintf(stderr,"[XPLM SHIM] XPLMSetDatai(%s,%d) [rejected: inactive, non-int, read-only or callback missing]\n",registered_datarefs[i].name,value); fflush(stderr); return;
   }
   fprintf(stderr,"[XPLM SHIM] XPLMSetDatai(%s,%d) -> plugin callback\n",registered_datarefs[i].name,value); fflush(stderr);
   registered_datarefs[i].set_i(registered_datarefs[i].write_refcon,value); return;
  }
 }
 fprintf(stderr,"[XPLM SHIM] XPLMSetDatai(ref=%p,value=%d) unknown handle\n",ref,value); fflush(stderr); abort();
}
__attribute__((visibility("default"))) void XPLMSetDataf(XPLMDataRef ref, float value) {
 if (!ref) { fprintf(stderr,"[XPLM SHIM] XPLMSetDataf(ref=NULL,value=%g) [ignored invalid ref]\n",(double)value); fflush(stderr); return; }
 for(int i=0;i<fg_count;i++) if(ref==(void*)&fg_refs[i]) {
  if(!(fg_refs[i].type & 2)) { fprintf(stderr,"[XPLM SHIM] XPLMSetDataf(%s) wrong type\n",fg_refs[i].name); fflush(stderr); abort(); }
  fg_refs[i].value=value;
  fprintf(stderr,"[XPLM SHIM] XPLMSetDataf(%s,%g) [SIMULATOR REF]\n",fg_refs[i].name,(double)value); fflush(stderr); return;
 }
 for(int i=0;i<registered_datarefs_count;i++) if(ref==(void*)&registered_datarefs[i]) {
  if (!registered_datarefs[i].active || !(registered_datarefs[i].type & 2) || !registered_datarefs[i].writable || !registered_datarefs[i].set_f) {
   fprintf(stderr,"[XPLM SHIM] XPLMSetDataf(%s,%g) [read-only or missing setter]\n",registered_datarefs[i].name,(double)value); fflush(stderr); return;
  }
  registered_datarefs[i].set_f(registered_datarefs[i].write_refcon,value); return;
 }
 fprintf(stderr,"[XPLM SHIM] XPLMSetDataf(ref=%p,value=%g) unknown handle\n",ref,(double)value); fflush(stderr); abort();
}
static unsigned long null_reads_float, null_reads_int, null_reads_array;
static void shim_log_null_ref(const char *name) {
 unsigned long *counter = strcmp(name,"XPLMGetDataf")==0 ? &null_reads_float : strcmp(name,"XPLMGetDatai")==0 ? &null_reads_int : &null_reads_array;
 unsigned long n=++*counter;
 if(n==1 || n==100 || n%5000==0) { fprintf(stderr,"[XPLM SHIM] %s(NULL) count=%lu [missing reference: returned neutral value]\n",name,n); fflush(stderr); }
}
__attribute__((visibility("default"))) float XPLMGetDataf(XPLMDataRef ref) {
 for(int i=0;i<fg_count;i++) if(ref==(void*)&fg_refs[i]) return (float)fg_refs[i].value;
 if (!ref) {
  shim_log_null_ref("XPLMGetDataf");
  fflush(stderr);
  return 0.0f;
 }
 fprintf(stderr,"[XPLM SHIM] XPLMGetDataf(ref=%p) unsupported non-NULL DataRef\n",ref);
 fflush(stderr); abort();
}
typedef int (*XPLMDrawCallback_f)(int, int, void *);
__attribute__((visibility("default"))) int XPLMUnregisterDrawCallback(XPLMDrawCallback_f cb, int phase, int before, void *refcon) {
 fprintf(stderr,"[XPLM SHIM] XPLMUnregisterDrawCallback(callback=%p, phase=%d, before=%d, refcon=%p) -> 0 [not registered]\n",(void *)cb,phase,before,refcon);
 fflush(stderr);
 return 0;
}
__attribute__((visibility("default"))) void XPLMUnregisterDataAccessor(XPLMDataRef ref) {
 for (int i=0; i<registered_datarefs_count; ++i) {
  if (ref == (XPLMDataRef)&registered_datarefs[i]) { registered_datarefs[i].active=0; fprintf(stderr,"[XPLM SHIM] XPLMUnregisterDataAccessor(%s) [deactivated]\n",registered_datarefs[i].name); fflush(stderr); return; }
 }
 fprintf(stderr,"[XPLM SHIM] XPLMUnregisterDataAccessor(ref=%p) [unknown or NULL]\n",ref);
 fflush(stderr);
}
__attribute__((visibility("default"))) void XPLMDebugString(const char *message) {
 fprintf(stderr,"[XPLM DEBUG] %s", message ? message : "(null)");
 if (!message || !*message || message[__builtin_strlen(message)-1] != '\n') fputc('\n', stderr);
 fflush(stderr);
}
__attribute__((visibility("default"))) void XPLMGetVersions(int *outXPlaneVersion, int *outXPLMVersion, int *outHostID) {
 if(outXPlaneVersion) *outXPlaneVersion=12000;
 if(outXPLMVersion) *outXPLMVersion=400;
 if(outHostID) *outHostID=1;
 fprintf(stderr,"[XPLM SHIM] XPLMGetVersions -> XPlane=12000, XPLM=400, HostID=1 [EMULATED; not real X-Plane]\n");
 fflush(stderr);
}
__attribute__((visibility("default"))) const char *XPLMGetDirectorySeparator(void) {
 fprintf(stderr,"[XPLM SHIM] XPLMGetDirectorySeparator -> / [native-path diagnostic]\n");
 fflush(stderr); return "/";
}
__attribute__((visibility("default"))) void XPLMGetSystemPath(char *outSystemPath) {
 if (!outSystemPath) { fprintf(stderr,"[XPLM SHIM] XPLMGetSystemPath(NULL)\n"); abort(); }
 char cwd[450];
 if (!getcwd(cwd,sizeof(cwd))) { perror("[XPLM SHIM] getcwd"); abort(); }
 char isolated[512];
 int n=snprintf(isolated,sizeof(isolated),"%s/diagnostic_xplane_root",cwd);
 if (n<0 || n>=(int)sizeof(isolated)-2) { fprintf(stderr,"[XPLM SHIM] system path too long\n"); abort(); }
 if (mkdir(isolated,0700)!=0 && errno!=EEXIST) { perror("[XPLM SHIM] mkdir"); abort(); }
 snprintf(outSystemPath,512,"%s/",isolated);
 fprintf(stderr,"[XPLM SHIM] XPLMGetSystemPath -> %s [ISOLATED diagnostic root]\n",outSystemPath);
 fflush(stderr);
}
__attribute__((visibility("default"))) int XPLMGetMyID(void) {
 fprintf(stderr,"[XPLM SHIM] XPLMGetMyID -> 1 [EMULATED plugin ID]\n");
 fflush(stderr); return 1;
}
__attribute__((visibility("default"))) int XPLMGetDatai(XPLMDataRef ref) {
 for(int i=0;i<fg_count;i++) if(ref==(void*)&fg_refs[i]) return (int)fg_refs[i].value;
 if (!ref) { shim_log_null_ref("XPLMGetDatai"); return 0; }
 for (int i=0; i<registered_datarefs_count; ++i) {
  if (ref == (XPLMDataRef)&registered_datarefs[i]) {
   if (!registered_datarefs[i].active || !(registered_datarefs[i].type & 1) || !registered_datarefs[i].get_i) {
    fprintf(stderr,"[XPLM SHIM] XPLMGetDatai(%s) -> 0 [inactive/type mismatch/no callback]\n", registered_datarefs[i].name); fflush(stderr); return 0;
   }
   int value=registered_datarefs[i].get_i(registered_datarefs[i].read_refcon);
   fprintf(stderr,"[XPLM SHIM] XPLMGetDatai(%s) -> %d [plugin callback]\n",registered_datarefs[i].name,value); fflush(stderr); return value;
  }
 }
 fprintf(stderr,"[XPLM SHIM] XPLMGetDatai(ref=%p) unknown non-NULL handle\n",ref); fflush(stderr); abort();
}
__attribute__((visibility("default"))) double XPLMGetDatad(XPLMDataRef ref) {
 if (!ref) return 0.0;
 for(int i=0;i<fg_count;i++) if(ref==(void*)&fg_refs[i]) return fg_refs[i].value;
 for(int i=0;i<registered_datarefs_count;i++) if(ref==(void*)&registered_datarefs[i]) return registered_datarefs[i].get_d ? registered_datarefs[i].get_d(registered_datarefs[i].read_refcon):0.0;
 fprintf(stderr,"[XPLM SHIM] XPLMGetDatad unknown reference\n"); abort();
}
__attribute__((visibility("default"))) int XPLMGetDatavi(XPLMDataRef ref, int *outValues, int offset, int max) {
 if (!ref) { shim_log_null_ref("XPLMGetDatavi"); return 0; }
 for(int i=0;i<fg_count;i++) if(ref==(void*)&fg_refs[i] && (fg_refs[i].type & 16)) {
  if(offset<0 || max<0) return 0;
  const int length=8; if(!outValues) return length; if(offset>=length) return 0; int count=max<length-offset?max:length-offset;
  for(int k=0;k<count;k++) outValues[k]=(int)fg_refs[i].value; return count;
 }
 for (int i=0; i<registered_datarefs_count; ++i) {
  if (ref == (XPLMDataRef)&registered_datarefs[i]) {
   if (!registered_datarefs[i].active || !(registered_datarefs[i].type & 16) || !registered_datarefs[i].get_vi) { fprintf(stderr,"[XPLM SHIM] XPLMGetDatavi(%s) -> 0 [unavailable]\n",registered_datarefs[i].name); fflush(stderr); return 0; }
   if (offset<0 || max<0) { fprintf(stderr,"[XPLM SHIM] XPLMGetDatavi invalid offset/max\n"); abort(); }
   int count=registered_datarefs[i].get_vi(registered_datarefs[i].read_refcon,outValues,offset,max);
   fprintf(stderr,"[XPLM SHIM] XPLMGetDatavi(%s, offset=%d, max=%d) -> %d [plugin callback]\n",registered_datarefs[i].name,offset,max,count); fflush(stderr); return count;
  }
 }
 fprintf(stderr,"[XPLM SHIM] XPLMGetDatavi(ref=%p) unknown non-NULL handle\n",ref); fflush(stderr); abort();
}
__attribute__((visibility("default"))) int XPLMGetDatavf(XPLMDataRef ref, float *values, int offset, int max) {
 if(!ref || offset<0 || max<0) return 0;
 for(int i=0;i<fg_count;i++) if(ref==(void*)&fg_refs[i] && (fg_refs[i].type & 8)) {
  const int length=8; if(!values) return length; if(offset>=length) return 0; int count=max<length-offset?max:length-offset;
  for(int k=0;k<count;k++) values[k]=(float)fg_refs[i].value; return count;
 }
 fprintf(stderr,"[XPLM SHIM] XPLMGetDatavf unsupported handle=%p\n",ref); abort();
}
__attribute__((visibility("default"))) int XPLMGetDirectoryContents(const char *path, int first, char *names, int names_size, char **indices, int index_count, int *total, int *returned) {
 if(total) *total=0; if(returned) *returned=0;
 if(names && names_size>0) names[0]=0; if(indices && index_count>0) indices[0]=NULL;
 if(!path || first<0 || names_size<0 || index_count<0) { fprintf(stderr,"[XPLM SHIM] XPLMGetDirectoryContents invalid arguments\n"); return 0; }
 char cwd[450]; if(!getcwd(cwd,sizeof(cwd))) abort();
 char root[512]; snprintf(root,sizeof(root),"%s/diagnostic_xplane_root/",cwd);
 if(strncmp(path,root,strlen(root))!=0 || strstr(path,"/../") || strstr(path,"/./")) {
  fprintf(stderr,"[XPLM SHIM] XPLMGetDirectoryContents DENIED path=%s [outside isolated root]\n",path); fflush(stderr); return 0;
 }
 DIR *d=opendir(path); if(!d) { fprintf(stderr,"[XPLM SHIM] XPLMGetDirectoryContents(path=%s) -> missing directory (%s)\n",path,strerror(errno)); fflush(stderr); return 0; }
 struct dirent *ent; int files=0, emitted=0, cursor=0, available=1;
 while((ent=readdir(d))) {
  if(!strcmp(ent->d_name,".") || !strcmp(ent->d_name,"..")) continue;
  if(files++ < first) continue;
  int n=(int)strlen(ent->d_name)+1;
  if(!names || names_size<=0 || cursor+n+1>names_size || (indices && emitted+1>=index_count)) { available=0; continue; }
  memcpy(names+cursor,ent->d_name,(size_t)n); if(indices) indices[emitted]=names+cursor;
  cursor+=n; emitted++;
 }
 closedir(d); if(total) *total=files; if(returned) *returned=emitted;
 if(names && names_size>cursor) names[cursor]=0; if(indices && emitted<index_count) indices[emitted]=NULL;
 fprintf(stderr,"[XPLM SHIM] XPLMGetDirectoryContents(path=%s) -> total=%d, returned=%d, complete=%d\n",path,files,emitted,available); fflush(stderr);
 return available;
}
__attribute__((visibility("default"))) void XPLMGetFontDimensions(int font, int *width, int *height, int *digits_only) {
 if (width) *width = 8;
 if (height) *height = 12;
 if (digits_only) *digits_only = 0;
 fprintf(stderr,"[XPLM SHIM] XPLMGetFontDimensions(font=%d) -> 8x12 digits_only=0 [HEADLESS APPROXIMATION]\n",font); fflush(stderr);
}
typedef void *XPLMWindowID;
typedef void (*XPLMDrawWindow_f)(XPLMWindowID, void *);
typedef int (*XPLMHandleMouseClick_f)(XPLMWindowID, int, int, int, void *);
typedef void (*XPLMHandleKey_f)(XPLMWindowID, char, int, char, void *, int);
typedef int (*XPLMHandleCursor_f)(XPLMWindowID, int, int, void *);
typedef int (*XPLMHandleMouseWheel_f)(XPLMWindowID, int, int, int, int, void *);
typedef struct {
 int structSize, left, top, right, bottom, visible;
 XPLMDrawWindow_f drawWindowFunc;
 XPLMHandleMouseClick_f handleMouseClickFunc;
 XPLMHandleKey_f handleKeyFunc;
 XPLMHandleCursor_f handleCursorFunc;
 XPLMHandleMouseWheel_f handleMouseWheelFunc;
 void *refcon;
 int decorateAsFloatingWindow, layer;
 XPLMHandleMouseClick_f handleRightClickFunc;
} XPLMCreateWindow_t;
static struct { int active; int left, top, right, bottom, visible; } windows[32];
static int windows_count = 0;
__attribute__((visibility("default"))) XPLMWindowID XPLMCreateWindowEx(XPLMCreateWindow_t *p) {
 if (!p || p->structSize < (int)sizeof(XPLMCreateWindow_t) || windows_count >= 32) {
  fprintf(stderr,"[XPLM SHIM] XPLMCreateWindowEx invalid params (size=%d, expected>=%zu)\n",p?p->structSize:-1,sizeof(XPLMCreateWindow_t)); fflush(stderr); abort();
 }
 int i=windows_count++; windows[i].active=1; windows[i].left=p->left; windows[i].top=p->top; windows[i].right=p->right; windows[i].bottom=p->bottom; windows[i].visible=p->visible;
 fprintf(stderr,"[XPLM SHIM] XPLMCreateWindowEx -> %p geometry=%d,%d,%d,%d visible=%d [HEADLESS, CALLBACKS NOT CALLED]\n",(void*)&windows[i],p->left,p->top,p->right,p->bottom,p->visible); fflush(stderr);
 return &windows[i];
}
__attribute__((visibility("default"))) void XPLMSetWindowIsVisible(XPLMWindowID window, int visible) {
 for (int i=0; i<windows_count; ++i) if (window==(void*)&windows[i]) {
  if (!windows[i].active) { fprintf(stderr,"[XPLM SHIM] XPLMSetWindowIsVisible(%p,%d) rejected: destroyed window\n",window,visible); fflush(stderr); abort(); }
  windows[i].visible=!!visible;
  fprintf(stderr,"[XPLM SHIM] XPLMSetWindowIsVisible(window=%p, visible=%d) [HEADLESS]\n",window,windows[i].visible); fflush(stderr); return;
 }
 fprintf(stderr,"[XPLM SHIM] XPLMSetWindowIsVisible(window=%p, visible=%d) unknown handle\n",window,visible); fflush(stderr); abort();
}
__attribute__((visibility("default"))) int XPLMGetWindowIsVisible(XPLMWindowID window) {
 for (int i=0; i<windows_count; ++i) if (window==(void*)&windows[i]) {
  if (!windows[i].active) { fprintf(stderr,"[XPLM SHIM] XPLMGetWindowIsVisible(window=%p) rejected: destroyed window\n",window); fflush(stderr); abort(); }
  fprintf(stderr,"[XPLM SHIM] XPLMGetWindowIsVisible(window=%p) -> %d [HEADLESS]\n",window,windows[i].visible); fflush(stderr); return windows[i].visible;
 }
 fprintf(stderr,"[XPLM SHIM] XPLMGetWindowIsVisible(window=%p) unknown handle\n",window); fflush(stderr); abort();
}
__attribute__((visibility("default"))) void XPLMDestroyWindow(XPLMWindowID window) {
 for (int i=0;i<windows_count;i++) if(window==(void*)&windows[i]) {windows[i].active=0; fprintf(stderr,"[XPLM SHIM] XPLMDestroyWindow(%p) [HEADLESS]\n",window); fflush(stderr); return;}
 fprintf(stderr,"[XPLM SHIM] XPLMDestroyWindow(%p) [unknown; ignored]\n",window); fflush(stderr);
}
__attribute__((visibility("default"))) int XPLMFindPluginBySignature(const char *signature) {
 int id = signature && strcmp(signature,"aero.ivao.altitude")==0 ? 1 : -1;
 fprintf(stderr,"[XPLM SHIM] XPLMFindPluginBySignature(signature=%s) -> %d [SINGLE-PLUGIN REGISTRY]\n",signature?signature:"(null)",id); fflush(stderr);
 return id;
}
enum { SHIM_SCREEN_WIDTH = 1280, SHIM_SCREEN_HEIGHT = 800 };
__attribute__((visibility("default"))) void XPLMGetScreenSize(int *width, int *height) {
 if(width) *width=SHIM_SCREEN_WIDTH; if(height) *height=SHIM_SCREEN_HEIGHT;
 fprintf(stderr,"[XPLM SHIM] XPLMGetScreenSize -> %dx%d [VIRTUAL, HEADLESS]\n",SHIM_SCREEN_WIDTH,SHIM_SCREEN_HEIGHT); fflush(stderr);
}
__attribute__((visibility("default"))) void XPLMGetWindowGeometry(XPLMWindowID window,int *left,int *top,int *right,int *bottom) {
 for(int i=0;i<windows_count;i++) if(window==(void*)&windows[i] && windows[i].active) {
  if(left) *left=windows[i].left; if(top) *top=windows[i].top; if(right) *right=windows[i].right; if(bottom) *bottom=windows[i].bottom;
  fprintf(stderr,"[XPLM SHIM] XPLMGetWindowGeometry(%p) -> %d,%d,%d,%d [HEADLESS]\n",window,windows[i].left,windows[i].top,windows[i].right,windows[i].bottom); fflush(stderr); return;
 }
 fprintf(stderr,"[XPLM SHIM] XPLMGetWindowGeometry invalid window=%p\n",window); fflush(stderr); abort();
}
__attribute__((visibility("default"))) void XPLMSetWindowGeometry(XPLMWindowID window,int left,int top,int right,int bottom) {
 for(int i=0;i<windows_count;i++) if(window==(void*)&windows[i] && windows[i].active) {
  if(right<=left || top<=bottom) {fprintf(stderr,"[XPLM SHIM] Invalid window dimensions\n");abort();}
  windows[i].left=left; windows[i].top=top; windows[i].right=right; windows[i].bottom=bottom;
  fprintf(stderr,"[XPLM SHIM] XPLMSetWindowGeometry(%p) -> %d,%d,%d,%d [HEADLESS]\n",window,left,top,right,bottom); fflush(stderr); return;
 }
 fprintf(stderr,"[XPLM SHIM] XPLMSetWindowGeometry invalid window=%p\n",window); fflush(stderr); abort();
}
__attribute__((visibility("default"))) void XPLMAcquirePlanes(void) { missing("XPLMAcquirePlanes"); }
__attribute__((visibility("default"))) void XPLMCountAircraft(void) { missing("XPLMCountAircraft"); }
__attribute__((visibility("default"))) void XPLMCreateFlightLoop(void) { missing("XPLMCreateFlightLoop"); }
__attribute__((visibility("default"))) void XPLMCreateInstance(void) { missing("XPLMCreateInstance"); }
__attribute__((visibility("default"))) void XPLMCreateMapLayer(void) { missing("XPLMCreateMapLayer"); }
__attribute__((visibility("default"))) void XPLMCreateProbe(void) { missing("XPLMCreateProbe"); }
__attribute__((visibility("default"))) void XPLMDestroyFlightLoop(void) { missing("XPLMDestroyFlightLoop"); }
__attribute__((visibility("default"))) void XPLMDestroyInstance(void) { missing("XPLMDestroyInstance"); }
__attribute__((visibility("default"))) void XPLMDestroyMapLayer(void) { missing("XPLMDestroyMapLayer"); }
__attribute__((visibility("default"))) void XPLMDestroyProbe(void) { missing("XPLMDestroyProbe"); }
__attribute__((visibility("default"))) void XPLMDisableAIForPlane(void) { missing("XPLMDisableAIForPlane"); }
__attribute__((visibility("default"))) void XPLMDrawMapIconFromSheet(void) { missing("XPLMDrawMapIconFromSheet"); }
__attribute__((visibility("default"))) void XPLMDrawMapLabel(void) { missing("XPLMDrawMapLabel"); }
__attribute__((visibility("default"))) void XPLMDrawString(void) { missing("XPLMDrawString"); }
__attribute__((visibility("default"))) void XPLMDrawTranslucentDarkBox(void) { missing("XPLMDrawTranslucentDarkBox"); }
__attribute__((visibility("default"))) void XPLMFindSymbol(void) { missing("XPLMFindSymbol"); }
__attribute__((visibility("default"))) void XPLMGetCycleNumber(void) { missing("XPLMGetCycleNumber"); }
__attribute__((visibility("default"))) void XPLMGetPluginInfo(void) { missing("XPLMGetPluginInfo"); }
__attribute__((visibility("default"))) void XPLMInstanceSetPosition(void) { missing("XPLMInstanceSetPosition"); }
__attribute__((visibility("default"))) void XPLMLoadObject(void) { missing("XPLMLoadObject"); }
__attribute__((visibility("default"))) void XPLMLoadObjectAsync(void) { missing("XPLMLoadObjectAsync"); }
__attribute__((visibility("default"))) void XPLMLocalToWorld(void) { missing("XPLMLocalToWorld"); }
__attribute__((visibility("default"))) void XPLMMapExists(void) { missing("XPLMMapExists"); }
__attribute__((visibility("default"))) void XPLMMapProject(void) { missing("XPLMMapProject"); }
__attribute__((visibility("default"))) void XPLMMapScaleMeter(void) { missing("XPLMMapScaleMeter"); }
__attribute__((visibility("default"))) void XPLMProbeTerrainXYZ(void) { missing("XPLMProbeTerrainXYZ"); }
__attribute__((visibility("default"))) void XPLMReadCameraPosition(void) { missing("XPLMReadCameraPosition"); }
__attribute__((visibility("default"))) void XPLMRegisterDrawCallback(void) { missing("XPLMRegisterDrawCallback"); }
__attribute__((visibility("default"))) void XPLMRegisterMapCreationHook(void) { missing("XPLMRegisterMapCreationHook"); }
__attribute__((visibility("default"))) void XPLMReleasePlanes(void) { missing("XPLMReleasePlanes"); }
__attribute__((visibility("default"))) void XPLMScheduleFlightLoop(void) { missing("XPLMScheduleFlightLoop"); }
__attribute__((visibility("default"))) void XPLMSendMessageToPlugin(void) { missing("XPLMSendMessageToPlugin"); }
__attribute__((visibility("default"))) void XPLMSetActiveAircraftCount(void) { missing("XPLMSetActiveAircraftCount"); }
__attribute__((visibility("default"))) void XPLMSetDatab(void) { missing("XPLMSetDatab"); }
__attribute__((visibility("default"))) void XPLMSetDatavf(void) { missing("XPLMSetDatavf"); }
__attribute__((visibility("default"))) void XPLMSetDatavi(void) { missing("XPLMSetDatavi"); }
__attribute__((visibility("default"))) void XPLMSetGraphicsState(void) { missing("XPLMSetGraphicsState"); }
__attribute__((visibility("default"))) void XPLMShareData(void) { missing("XPLMShareData"); }
__attribute__((visibility("default"))) void XPLMUnloadObject(void) { missing("XPLMUnloadObject"); }
__attribute__((visibility("default"))) void XPLMUnshareData(void) { missing("XPLMUnshareData"); }
__attribute__((visibility("default"))) void XPLMWorldToLocal(void) { missing("XPLMWorldToLocal"); }
/* Host session status: read the genuine plugin-owned DataRef. */
__attribute__((visibility("default"))) int shim_ivao_online(void) {
 for(int i=0;i<registered_datarefs_count;i++) {
  if(strcmp(registered_datarefs[i].name,"ivaopilot/online")==0) {
   if(!registered_datarefs[i].active || !registered_datarefs[i].get_i) return 0;
   return registered_datarefs[i].get_i(registered_datarefs[i].read_refcon)!=0;
  }
 }
 return 0;
}
