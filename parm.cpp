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

PARM::PARM(const char *pth)
{
nam = new DYNAG(0);
val = new DYNAG(sizeof(char*));
HDL f=flopen(path=stradup(pth),"R");
if (f)
    {
    int len, eq;
    char s[1024];
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
/*if (upd)
    {
    HDL f=flopen(path,"w");
    for (i=0;i<nam->ct;i++)
        {
        vv=(char**)val->get(i);
        if (vv)
            {
            v=*vv;
            if (*v)
                {
                sprintf(s,"%s=%s",(char*)nam->get(i),v);
                flputln(s,f);
flputs("\n",f);
                }
            }
        }
    flclose(f);
    }*/

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
return(NULL);
}

// Same as standard strcmp() except it allows for NULLPTR to match pointer to "" (EOS nullbyte of empty string)
static int strcmpEx(const char *str1, const char *str2)
{
int dif=0;
if ((str1!=NULL) && (str2!=NULL)) return(strcmp(str1,str2));    // If both are valid string pointers, just compare
if (((str1!=NULL) && str1[0]) || ((str2!=NULL) && str2[0])) return(YES); // different if EITHER is a non-null string
return(NO); // No difference - one or both strings are NULLPTR, and neither points to a non-null string
}

/*void PARM::set(const char *name, const char *value)
{
int i, fnd;
const char **vv;
const char *v;
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
}*/
