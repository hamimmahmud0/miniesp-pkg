// host simulation of the wtms accounting: a simulated 1000 L tank, usage pattern, pump relay and sonar
#include <string.h>
#define main wtms_main
#define NO_SETUP 1
#include "/home/hamim-mahmud/Workspace/miniesp/miniesp-pkg/programs/wtms.c"
#undef main
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

static unsigned now_s = 1760054400u;          // 2026-10-10 00:00:00 UTC
static double vol = 600;                       // litres in the tank
static int pump = 0, mq_state = 2, dry = 0; static unsigned dry_after;
static char kvk[64][32], kvv[64][1024]; static int kvl[64], kvn;
static double drawn_today, pumped_today; static int curdate;
static double tot_use[40], tot_in[40];
static int day_idx;
static const char *cur_path;

static int kvfind(const char *k){ for(int i=0;i<kvn;i++) if(!strcmp(kvk[i],k)) return i; return -1; }
int sys_kv_get(const char *k, void *b, int n){ int i=kvfind(k); if(i<0) return -1; int l=kvl[i]<n?kvl[i]:n; memcpy(b,kvv[i],l); return l; }
int sys_kv_set(const char *k, const void *b, int n){ int i=kvfind(k); if(i<0){ i=kvn++; strcpy(kvk[i],k);} memcpy(kvv[i],b,n); kvl[i]=n; return 0; }
int sys_kv_del(const char *k){ int i=kvfind(k); if(i>=0) kvl[i]=-1; return 0; }
typedef struct { FILE *f; } fdent; static FILE *fds[16];
static void mkpath(char *o, const char *p){ sprintf(o,"www_tank/%s", strrchr(p,'/')+1); }
int sys_open(const char *p, int mode){ char q[200]; mkpath(q,p); FILE *f = fopen(q, mode==0?"rb":mode==1?"wb":"ab"); if(!f) return -1; for(int i=3;i<16;i++) if(!fds[i]){fds[i]=f;return i;} return -1; }
int sys_close(int fd){ fclose(fds[fd]); fds[fd]=0; return 0; }
int sys_read(int fd, void *b, int n){ return fds[fd]? (int)fread(b,1,n,fds[fd]):-1; }
int sys_write(int fd, const void *b, int n){ if(fd==1||fd==2) return (int)fwrite(b,1,n,stdout); return (int)fwrite(b,1,n,fds[fd]); }
int sys_fsize(const char *p){ char q[200]; mkpath(q,p); FILE *f=fopen(q,"rb"); if(!f) return -1; fseek(f,0,SEEK_END); int n=ftell(f); fclose(f); return n; }
int sys_rename(const char *a, const char *b){ char x[200],y[200]; mkpath(x,a); mkpath(y,b); return rename(x,y); }
int sys_mkdir(const char *p){ return 0; }
int sys_stat(const char *p){ return -1; }
unsigned sys_time(void){ return now_s; }
unsigned sys_millis(void){ return (unsigned)(now_s*1000u); }
int sys_localtime(unsigned e, int *o, int b){ time_t t=e; struct tm tm; gmtime_r(&t,&tm); o[0]=tm.tm_sec;o[1]=tm.tm_min;o[2]=tm.tm_hour;o[3]=tm.tm_mday;o[4]=tm.tm_mon;o[5]=tm.tm_year+1900;o[6]=tm.tm_wday;o[7]=tm.tm_yday; return 0; }
void sys_sleep_ms(unsigned ms){}
int sys_ds18b20(int p){ return -9999; }
int sys_sonar_pulse(int t,int e,int to){ double h_cm = vol/8.517; double d_mm = 205 + (1000.0/8.517 - h_cm)*10 + ((rand()%3)-1)*1.0; return (int)(d_mm*20000/3434.0); }
int sys_mqtt_state(void){ return mq_state; }
int sys_mqtt_sub(const char *t,int q){ return 0; }
int sys_mqtt_age(const char *t){ return 100; }
int sys_mqtt_get(const char *t, void *b, int n){ if(strstr(t,"/state")&&strstr(t,"relay")){ strcpy(b, pump?"ON":"OFF"); return pump?2:3;} if(strstr(t,"/status")){ strcpy(b,"online"); return 6;} return -1; }
int sys_mqtt_pub(const char *t, const void *p, int n, int r, int q){ if(strstr(t,"waterpump/set")){ pump = !strncmp(p,"ON",2); } return 0; }
int sys_netinfo(int w,char *b,int n){ strcpy(b,"test"); return 4; }
int sys_mqtt_info(int w,char *b,int n){ b[0]=0; return 0; }
int sys_mqtt_config(const char*a,int b,const char*c,const char*d,const char*e){return 0;}
unsigned sys_random(void){ return rand(); }
int sys_isatty(int f){return 0;} int sys_getkey(int m){return -1;} int sys_run(const char*a,int b,int c,int d,int e,int f){return 0;} int sys_tz(const char*s){return 0;}

static double use_rate_lpm(unsigned t){ int h=(t%86400)/3600; double base = (h>=6&&h<9)?2.2:(h>=18&&h<22)?1.4:(h>=11&&h<14)?0.8:(h<5?0.0:0.15); return base; }

int main(int argc, char **argv)
{
    int days = argc > 1 ? atoi(argv[1]) : 8;
    srand(1); if (argc > 2) now_s = (unsigned)strtoul(argv[2], 0, 10);
    if (argc > 3) { dry = 1; dry_after = now_s + (unsigned)atoi(argv[3]) * 86400; }
    char b0[]="wtms",b1[]="config",b2[]="full_mm=205",b3[]="ml_cm=8517",b4[]="vol_l=1000",b5[]="hist=60",b6[]="interval=5",b7[]="min_in=100",b8[]="flow_min=2"; char *a[] = { b0,b1,b2,b3,b4,b5,b6,b7,b8 };
    wtms_main(9, a);
    char r0[]="wtms",r1[]="run"; char *r[] = { r0, r1 };
    int rm_ok = system("rm -f www_tank/*.csv www_tank/state.json");
    double use_total = 0, in_total = 0; double dayuse[40]={0}, dayin[40]={0}; int di = 0; unsigned day0 = now_s/86400;
    for (unsigned step = 0; step < (unsigned)days*86400/5; step++) {
        now_s += 5;
        double dtmin = 5/60.0;
        double u = use_rate_lpm(now_s) * dtmin; if (u > vol) u = vol;
        double inn = pump ? (dry && now_s > dry_after ? 2.0 : 32.0)*dtmin : 0;
        vol += inn - u; if (vol > 1000) vol = 1000;
        dayuse[(now_s/86400)-day0] += u; dayin[(now_s/86400)-day0] += inn;
        wtms_main(2, r);                     // the service ticks every 5 s; the program gates itself to every 15 s
    }
    printf("true daily usage / intake (L):\n");
    for (int i=0;i<=days;i++) printf("  day %d  use %.1f  in %.1f\n", i, dayuse[i], dayin[i]);
    return 0;
}
int sys_argc(void){return 0;} int sys_arg(int i,char*b,int n){return -1;}
