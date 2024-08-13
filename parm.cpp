#include <cstdio>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "pdef.h"
#include "flopen.h"
#include "memgive.h"
#include "str.h"
#include "parm.h"
#include "log.h"
#include "cal.h"

PARM::PARM(void)
{
char s[1024];
path=stradup(strfmt(s,"/home/%s/.config/Softworks/SMDB.ini",getenv("USER")));
nam = new DYNAG(0);
val = new DYNAG(sizeof(char*));
HDL f=flopen(path,"R");
if (f)
    {
    int len, eq;
    while ((len=flgetln(s,1024,f))>=0)
        {
        if ((eq=stridxc('=',s))<1) continue;
        s[eq]=0;
        nam->put(s);                    // the names are in a space-saving "multi-string" array
        char *vv=stradup(&s[eq+1]);
        val->put(&vv);                  // the VALUES are in an array of (allocated) POINTERS to the actual values
        }
    flclose(f);
    }
}

PARM::~PARM()
{
int i;
char s[1024], *v, **vv;
if (upd)
    {
    HDL f=flopen(path,"w");
    for (i=0;i<nam->ct;i++)
        {
        vv=(char**)val->get(i);
        if (vv!=NULL && vv[0]!=0)
            {
            v=vv[0];
            if (*v)
                {
                sprintf(s,"%s=%s",(char*)nam->get(i),v);
                flputln(s,f);
//flputs("\n",f);
                }
            }
        }
    flclose(f);
    }

for (i=0;i<val->ct;i++)
    {
    vv=(char**)val->get(i);
    if (vv)
        {
        v=*vv;
        memtake(v);
        }
    }

Scrap(path);
SCRAP(nam);
SCRAP(val);
}

const char* PARM::get(const char *name)
{
for (int i=0;i<nam->ct;i++)
    {
    char *n=(char*)nam->get(i);
    if (strcmp(n,name)) continue;   // this entry isn't the one we're looking for
    char **p = (char**)val->get(i);
    return(*p);
    }
return("");
}

// Same as standard strcmp() except it allows for NULLPTR to match pointer to "" (EOS nullbyte of empty string)
static int strcmpEx(const char *str1, const char *str2)
{
if ((str1!=NULL) && (str2!=NULL)) return(strcmp(str1,str2));    // If both are valid string pointers, just compare
if (((str1!=NULL) && str1[0]) || ((str2!=NULL) && str2[0])) return(YES); // different if EITHER is a non-null string
return(NO); // No difference - one or both strings are NULLPTR, and neither points to a non-null string
}

void PARM::set(const char *name, const char *value)
{
int i, fnd;
const char **vv, *v;
char    *vn=NULL;
for (i=fnd=0;!fnd && i<nam->ct;i++)
    {
    const char *n=(const char*)(nam->get(i));
    if (strcmp(n,name)) continue;     // this entry isn't the one we're looking for
    vv=(const char**)val->get(i);
    v=*vv;
    if (!strcmpEx(v,value)) return;   // the VALUE for this entry hasn't changed
    memtake(v);
    if ((value!=NULL) && value[0]) vn=stradup(value);
    memmove(vv,&vn,sizeof(char*));
    fnd=YES;
    }
// ?? What if passed "value" is NULLPTR or points to empty string AND fnd=NO (no current setting to change)
if (!fnd)
    {
    nam->put(name);
    if ((value!=NULL) && value[0]) vn=stradup(value);
    val->put(&vn);
    }
upd=YES;
}


RECENT::RECENT()
{
tbl=new DYNAG(sizeof(int32_t));
char s[256];
get_conf_path(fn, "smdb.bck");
MOVE4BYTES(&fn[strlen(fn)-3],"rec");
HDL f=flopen(fn, "r");
if (f)
    {
    while (!fleof(f))
        {
        flgetln(s,250,f);
        int32_t imno=a2l(s,0);
        if (imno) tbl->put(&imno);
        }
    flclose(f);
    }
}

void RECENT::put(int32_t imno)
{
char s[256];
HDL f=flopen(fn, "w");
if (f==0) m_finish("Can't write %s",fn);
int i=tbl->in(&imno);
if (i!=NOTFND) tbl->del(i);
tbl->put(&imno,0);
for (int i=0; i<tbl->ct; i++)
    {
    flputln(strfmt(s,"%d",*(int32_t*)tbl->get(i)),f);
    }
flclose(f);
}

int RECENT::pos(int32_t imno)
{
for (int i=0; i<tbl->ct; i++)
    if (*(int32_t*)tbl->get(i) == imno) return(i);
return(255);
}

const char *apikey()
{
static char ky[40];
if (!ky[0])
    {
    const char *k[3]={"66e03abd", "c9497777", "fef8c54f"};
    PARM pm;
    const char *p=pm.get("apikey");
    if (p==NULL || !p[0]) p=(char*)k[0];
    strncpy(ky,p,sizeof(ky));
    }
return(ky);
}

char *parm_str(const char *setting, char *buf, const char *deflt)   // buf must be >= 128 bytes
{
PARM pm;
if (!*strncpy(buf,pm.get(setting), 127))
    strncpy(buf,deflt, 127);
return(buf);
}


const char *tapikey(int throttle)
{
static char ky[40];
static HDL tmr=NULL;
if (!ky[0])
    {
//    const char *k[1]={"ad3ce4561240b7c76d8d85adc7b513f9"};      // This is FumbleFingerP NEW Proton account!!!
    const char *k[1]={"5dc1fb73e1dee8cb0384875bd1239acb"};      // FumbleFingers original account from 2017
    PARM pm;
    const char *p=pm.get("tapikey");
    if (p==NULL || !p[0]) p=(char*)k[0];
    strancpy(ky,p,sizeof(ky));              // ALWAYS outputs string terminating EOS null byte
    }
if (throttle)   // limit to 25 calls per second by just sleeping until it's time to return the value
    {
    if (throttle==NOTFND) {SCRAP(tmr); return(NULL);}
    if (tmr==NULL) tmr=tmrist(0);
    while (tmrelapsed(tmr)<2) usleep(10000);   // 1,000,000 microseconds = 1 second
//    while (tmrelapsed(tmr)<4) usleep(10000);   // 1,000,000 microseconds = 1 second
    tmrreset(tmr,0);
    }
return(ky);
}

char *get_conf_path(char *fn, const char *base_filename)
{
PARM parm;
strcpy(fn,parm.get(base_filename));
if (!fn[0]) {sjhlog("No configured path for %s",base_filename); throw(102);}

int sub=stridxc('$',fn);
if (sub!=NOTFND && ISDIGIT(fn[sub+1]))
    {
    const char *k;
    char ks[3];
    k=parm.get(strancpy(ks,&fn[sub],3));    // strancpy "$n" + EOS nullbyte to sought param-name
    if (!k[0]) {sjhlog("No configured path for %s",ks); throw(102);}
    strins(strdel(&fn[sub],2),k);   // replace $n with retrieved string
    }

if (fn[strlen(fn)-1]!='/') strcat(fn,"/");
strendfmt(fn,"%s",base_filename);
return(fn);
}

