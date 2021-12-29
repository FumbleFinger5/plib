#include <stdlib.h>
//#include <malloc.h>
#include <string.h>
#include <cstdarg>


#include "pdef.h"
//#include "db.h"
#include "memgive.h"
//#include "flopen.h"
#include "str.h"
#include "drinfo.h"

#include "dirscan.h"

DIRSCAN::DIRSCAN(const char *pth, const char *msk, int flg)
{
mask=maskext=NULL;
flag=flg;
if (msk!=NULL)
    {
    mask=stradup(msk);
    int dot=stridxc('.',mask);  // Is there a dot followed by mask for extension after filename mask?
    if (dot!=NOTFND)
        {
        mask[dot]=0;
        maskext=&mask[dot+1];
        }
    }
dir = opendir(pth);
}

DIRSCAN::~DIRSCAN()
{
closedir(dir);
if (mask) Scrap(mask);
}

// Return offset of LAST occurrence of char in string
static int stridxcr(char c,const char *s)
{
const char *p=strrchr(s,c);
return(p?p-s:NOTFND);
}

static int mask_okay(char *m, char *f) // pointers to wantedmask and filename
{
while (m && m[0]!='*')
    {
    if (!f[0]) return(NO); // Can't match if we've run out of character on filename side
    if (m[0]=='?' || m[0]==f[0]) {f++; m++; continue;}          // Advance both pointers in lockstep to next chars
    return(NO); // characters in mask and filename don't match
    }
return(YES);    // If we got here the filename must match the mask
}

struct dirent* DIRSCAN::next(void)
{
while ((entry = readdir(dir)) != NULL)
	{
    char n[FNAMSIZ];
    strcpy(n,entry->d_name);
    if (entry->d_type==DT_DIR) if ((flag&DS_DIR)==0) continue;
    if (n[0]=='.') if ((flag&DS_DOT)==0) continue;
    if (mask)
        {
        char *ext=NULL;
        int dot=stridxcr('.',n);
        if (dot!=NOTFND) {n[dot]=0; ext=&n[dot+1];}
        if (!mask_okay(mask,n)) continue;
        if (maskext!=NULL && ext!=NULL) if (!mask_okay(maskext,ext)) continue;
        }
    break;  // If we got here, break out of loop and return non-null "entry" ptr
    }
return(entry);
}
