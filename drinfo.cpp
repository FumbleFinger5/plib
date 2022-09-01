#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <cstdio>
#include <dirent.h>
#include <stdlib.h>

#include "pdef.h"
#include "str.h"
#include "memgive.h"
#include "log.h"
#include "drinfo.h"


// PATH_MAX

// Parse 'fullpath' copying the directory part (incl. final '/') to 'dir_part
// Return pointer to the null byte at the end of 'dir_part', so app code can easily concatenate a different filename
// LINUX - remove code looking for BACKSLASH as well as '/'
char  *drsplitpath(char *dir_part, const char *fullpath)
{
int	offset;
char	*filnam = (char*)fullpath;
while	((offset=stridxc('/',filnam)) != NOTFND) filnam += offset+1;	// step through separators to find last one
offset = filnam - fullpath;
memmove (dir_part, fullpath, offset);		// copy directory part to passed dir_part parameter
*(filnam=&dir_part[offset])=0;
return(filnam);							// return ptr to just after last separator
}

int drunlink(const char *filename)	// MY function returns YES if success, 0 if failed (opposite of 'unlink')
{
return(!unlink(filename));
}

int drrename(const char *oldfilename,const char *newfilename)
{
char	nfn[FNAMSIZ];
strcpy(drsplitpath(nfn, oldfilename),newfilename);
int err=rename(oldfilename, nfn);
if (err) m_finish("bugger");
return(YES);
}

char *drfullpath(char *path, const char *p)	// Return FULLPATH which NEVER ends in '/'
{
return(realpath(p,path));
}

char *drfulldir(char *fullpath, const char *path)	// Return FULLPATH which ALWAYS ends in '/'
{
if (strlen(drfullpath(fullpath,path))>3) strcat(fullpath,"/");
return(fullpath);
}

DIR *drscnist(const char *path)
{
return(opendir(path));
}

int drscnnxt(DIR *scn, FILEINFO *fi)
{
struct dirent *dp;
do	{
	dp=readdir(scn);
	if (dp==NULLPTR) return(NO);
	} while (0);
strcpy(fi->name,dp->d_name);
fi->attr=0;
// Aug21 fi->attr=dp->d_type; // use values from stat() instead
if (dp->d_type & 4)
	fi->attr=FA_DIR;

struct stat sb;
stat(fi->name,&sb);				// MAY 2022  -  should(???) be FULLPATH, but that's not available here!!!

//if (S_ISDIR(sb.st_mode)) fi->attr=FA_DIR;
//else if (S_ISREG(sb.st_mode)) fi->attr= ??? ordinary file - pforce attr was just 0

fi->size=sb.st_size;
return(YES);
}

void drscnrls(DIR *scn)
{
closedir(scn);
}

// Get file details, return YES = OK, NO = Fail
int drinfo(const char *path, FILEINFO *fi)
{
struct stat sb;
realpath(path,fi->name);			// Fully expand passed path into passed FI structure
if (stat(path,&sb))
	{
	return(NO);	// some kind of error (copied to system global 'errno')
	}
fi->attr=FA_SPECIAL;
if (S_ISDIR(sb.st_mode))
	{
	fi->attr=FA_DIR;
	}
else if (S_ISREG(sb.st_mode)) fi->attr=FA_ORDINARY; // ordinary file - pforce attr is just 0
fi->size=sb.st_size;
fi->dttm=sb.st_mtime;		// Aug21 - Needs converting from time_t to cal format
return(YES);
}



int drattrget(const char *path, short *attr)
{
FILEINFO fi;
int ok=drinfo(path,&fi);
if (ok && attr) *attr=fi.attr;
return(ok);
}


/*
Ulong drdsksp(char drive)
{
ULARGE_INTEGER
	FreeBytesAvailable,     // bytes available to caller
	TotalNumberOfBytes,     // bytes on disc
	TotalNumberOfFreeBytes; // free bytes on disc
int drv=TOUPPER(drive);
assert(drv>='A' && drv<='Z');
char dir[3]={drv,':',0};
if (GetDiskFreeSpaceEx(dir,&FreeBytesAvailable,&TotalNumberOfBytes,&TotalNumberOfFreeBytes))
	 return(FreeBytesAvailable.LowPart);
return(0);
}
*/

//static char* get_filename_offset(char *path, const char *dflt_path)
//{return(strend(strcat(drfullpath(path,*path?path:dflt_path),"/")));}


int drisdir(const char *directory_path)
{
char pth[FNAMSIZ];
drfullpath(pth,directory_path);

struct stat sb;
if (stat(pth,&sb)) return(NO);		// non-zero (always -1?) return value from stat() mean ERROR

if (S_ISDIR(sb.st_mode))
	return(YES);
return(NO);
}

