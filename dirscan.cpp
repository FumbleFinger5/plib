#include <stdlib.h>
#include <string.h>
#include <cstdarg>
#include <sys/stat.h>

#include "pdef.h"
#include "memgive.h"
#include "str.h"
#include "drinfo.h"
#include "log.h"

#include "dirscan.h"

DIRSCAN::DIRSCAN(const char *pth, const char *msk)
{
mask=maskext=NULL;
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
dir = opendir(_pth=stradup(pth));
if (dir==NULL) sjhlog("opendir(%s) failed!",_pth);
}

DIRSCAN::~DIRSCAN()
{
closedir(dir);
if (mask) Scrap(mask);
Scrap(_pth);
}

// Return offset of LAST occurrence of char in string
static int stridxcr(char c,const char *s)
{
const char *p=strrchr(s,c);
return(p?p-s:NOTFND);
}

static int mask_okay(char *m, char *f) // pointers to wantedmask and filename
{
while (m[0] && m[0]!='*')
    {
    if (!f[0]) return(NO); // Can't match if we've run out of character on filename side
    if (m[0]=='?' || m[0]==f[0]) {f++; m++; continue;}          // Advance both pointers in lockstep to next chars
    return(NO); // characters in mask and filename don't match
    }
return(YES);    // If we got here the filename must match the mask
}

struct dirent* DIRSCAN::next(FILEINFO *fi)
{
while ((entry = readdir(dir)) != NULL)
	{
    char n[FNAMSIZ];
    strcpy(n,entry->d_name);                                // This is just bare filename (not "pathed")
    if (SAME2BYTES(n,".") || SAME3BYTES(n,"..")) continue;  // ignore 1 or 2 dots followed by EOS nullbyte
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
if (entry!=0 && fi!=0)  // Then copy FULL path to fi->name, along with correct filesize from stat()
    {
    struct stat sb;
    stat(strfmt(fi->name,"%s/%s",_pth,entry->d_name),&sb);
    fi->attr=entry->d_type;
    fi->size=sb.st_size;
    fi->dttm=sb.st_mtime;
    }
return(entry);
}
