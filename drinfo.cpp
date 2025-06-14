#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <cstdio>
#include <dirent.h>
#include <stdlib.h>

#include <stdio.h>
#include <stdint.h>
#include <sys/statvfs.h>

#include "pdef.h"
#include "str.h"
#include "memgive.h"
#include "log.h"
#include "drinfo.h"
#include "dirscan.h"

#include <libgen.h>

char *Basename(const char *fullpath)		// 'safe' wrapper returns static copy of RIGHTMOST element in fullpath
{
static char tmp[_POSIX_PATH_MAX];
return(basename(strcpy(tmp, fullpath)));	
}

char *Dirname(const char *fullpath)		// 'safe' wrapper returns static copy of PATH (LEFT of fullpath)
{
static char tmp[_POSIX_PATH_MAX];
return(dirname(strcpy(tmp, fullpath)));	
}
// Parse 'fullpath' copying the directory part (incl. final '/') to 'dir_part
// Return pointer to the null byte at the end of 'dir_part', so app code can easily concatenate a different filename
// LINUX - remove code looking for BACKSLASH as well as '/'
// TO DO - maybe allow dir_part=NULL, so not populated, but still return offset of bare filename in fullpath
char  *drsplitpath(char *dir_part, const char *fullpath)	// Return address changed 05/03/24 (now "filepart")
{
int	offset;
char	*filnam = (char*)fullpath;
while	((offset=stridxc('/',filnam)) != NOTFND) filnam += offset+1;	// step through separators to find last one
offset = filnam - fullpath;
memmove (dir_part, fullpath, offset);		// copy directory part to passed dir_part parameter
*(filnam=&dir_part[offset])=0;
//return(filnam);							// return ptr to just after last separator
return((char*)&fullpath[offset]);			// return ptr to just after last separator WITHIN PASSED FULLPATH
}

int drunlink(const char *filename)	// MY function returns YES if success, 0 if failed (opposite of 'unlink')
{
return(!unlink(filename));
}

int drrename(const char *oldfilename,const char *newfilename) 
{
char	nfn[FNAMSIZ];
//strcpy(drsplitpath(nfn, oldfilename),newfilename);
drsplitpath(nfn, oldfilename);
strcat(nfn,newfilename);
int err=rename(oldfilename, nfn);
if (err) m_finish("bugger");
return(YES);
}

#include <filesystem>
#include <iostream>
static bool allow_missing_file=false;
static char *my_realpath(const char *ip, char *op)
{
int err=NO;
try {         // Create a filesystem path from the user input
   std::filesystem::path p(ip);
   std::filesystem::path resolved_path = std::filesystem::canonical(p);
   std::string sstr = resolved_path.string();
   const char *str=sstr.c_str();
   strcpy(op,str);
   }
catch (const std::filesystem::filesystem_error& e)
   {
   err=SE_ERR_PATH;
   if (!allow_missing_file)
      sjhlog("Invalid path:%s",ip);
   }
return(err?NULL:op);
}



char *drfullpath(char *path, const char *p)	// Return FULLPATH in path, which NEVER ends in '/'
{
return(my_realpath(p,path));			// Fully expand passed SRC p into passed DST path and return path as a convenience
}

char *drfullpath(char *path)	// IN SITU version of drfullpath(src,dst)
{
char wrk[512];
if (drfullpath(wrk,path)==NULL) return(NULL);
return(strcpy(path,wrk));
}

char *drfulldir(char *fullpath, const char *path)	// Return FULLPATH which ALWAYS ends in '/'
{
//if (strlen(drfullpath(fullpath,path))>3) strcat(fullpath,"/");
if (drfullpath(fullpath,path)==NULL) return(NULL);
if (strlen(fullpath)>3) strcat(fullpath,"/");
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
{closedir(scn);}

// Get file details, return YES = OK, NO = Fail
bool drinfo(const char *path, FILEINFO *fi)
{
struct stat sb;
allow_missing_file=true;
drfullpath(fi->name, path);			// Fully expand passed path into passed FI structure
allow_missing_file=false;
if (stat(path,&sb)) return false;	// some kind of error (copied to system global 'errno')
fi->attr=FA_SPECIAL;
if (S_ISDIR(sb.st_mode)) fi->attr=FA_DIR;
else if (S_ISREG(sb.st_mode)) fi->attr=FA_ORDINARY; // ordinary file - pforce attr is just 0
fi->size=sb.st_size;
fi->dttm=sb.st_mtime;		// Aug21 - Needs converting from time_t to cal format
return true;
}

int megabytes(int64_t size) {return(size >> 20);}  // DON'T USE! Just use size/1000000

uint64_t get_free_space(const char *path)    // free dspace in bytes of drive containing 'path' (dir/file)
{
struct statvfs buf;
if (statvfs(path, &buf) != 0) sjhlog("statvfs failed for %s\n",path);
return(uint64_t)buf.f_bavail * (uint64_t)buf.f_frsize;
}

bool drattrget(const char *path, short *attr)	// YES if file exists (fill optional 'attr')
{
FILEINFO fi;
bool ok=drinfo(path,&fi);
if (ok && attr) *attr=fi.attr;
return ok;
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


int drisdir(const char *directory_path)	// YES/NO - is path a directory?
{
char pth[FNAMSIZ];
drfullpath(pth,directory_path);

struct stat sb;
if (stat(pth,&sb)) return(NO);		// non-zero (always -1?) return value from stat() mean ERROR

if (S_ISDIR(sb.st_mode))
	return(YES);
return(NO);
}

bool unwanted_filename(const char *n)
{
if (SAME4BYTES(drext(n),".srt")) return(false);
if (strcasestr(n,"RARBG") != 0 || strcasestr(n,"WWW.YTS") != 0) return(true);
if (strcasestr(n,"Torrent")!=0 && strcasestr(n,"downloaded")!=0) return(true);
if (strcasestr(n,"demonoid")!=0) return(true);
if (strcasestr(n,"Downloaded from")!=0) return(true);
if (strcasestr(n,"YIFYStatus")!=0) return(true);
if (stridxs("Xclusive",n)!=NOTFND) return(true);
if (stridxs("PublicHD",n)!=NOTFND) return(true);
if (strcasestr(n,"torrentgalaxy")!=0) return(true);
if (strcasestr(n,"yify")!=0 && SAME4BYTES(drext(n),".jpg")) return(true);
return(false);
}


#include <fstream>
#include <iostream>

int64_t dr_filesize(const char *fn)		// the actual code uses #ifdef to compile special Raspberry Pi version
{
int64_t sz=0;
#ifdef FOR_PI
std::ifstream file(fn, std::ios::binary | std::ios::ate);
if (file.is_open())
    {
	std::streamsize size = file.tellg();
	file.close();
	sz=size;
    }
#else
FILEINFO fi;
if (drinfo(fn,&fi))
	sz=fi.size;
#endif
return(sz);
}

int64_t dr_foldersize(const char *pth)
{
int64_t total=0;
DIR *scn=drscnist(pth);
FILEINFO fi;
while (drscnnxt(scn,&fi))	// Scan each file or folder in the passed path
	{
   if (fi.name[0]=='.' && (fi.name[1]==0 || fi.name[1]=='.')) continue;
   char name[FILENAME_MAX];
   strfmt(name,"%s/%s",pth,fi.name);
	if (fi.attr&FA_DIR)
      {
      total+=dr_foldersize(name);
      }
   else
      {
//      total+=fi.size;             // 24/1/25  - Doesn't work! dunno why fi holds high spurious size
      total+=dr_filesize(name);
      }
	}
drscnrls(scn);
return(total);
}

const char *drext(const char *fn)	// returns ptr-> ".ext" (incl. the dot), or static 4 nulls
{
static char nulls[4]={0,0,0,0};
int pos=strlen(fn)-4;
if (pos<0) return(nulls);
return(&fn[pos]);
}

bool drisvid(const char *fn)
{
return(stridxs(fn, ".mp4.mkv.avi")!=NOTFND);
}


static bool find_films_folder(int n, char *pth)
{
const char *drv="/media/steve";
DIRSCAN ds(drv);
struct dirent *de;
while ((de=ds.next())!=NULL)
	if (de->d_type&DT_DIR)	// d_type&4 = Directory
      {
      DIRSCAN ds1(strfmt(pth,"%s/%s",drv,de->d_name),"Films*");
      struct dirent *de1;
      while ((de1=ds1.next())!=NULL)
         {
   	   if ((de1->d_type&DT_DIR)==0) throw(71);
         int n1=a2i(&de1->d_name[5],0);
         if (a2err || n1<1 || n1>99) throw(72);
         if (n1==n)
            {
            strfmt(pth,"%s/%s/%s",drv,de->d_name,de1->d_name);
            return(true);
            }
         }
      }
return(false);
}

int valid_films_path(char *pth)  // accept just 1-2 digits, and silently change to /media/.../FilmsNN
{
if (ISDIGIT(*pth))
   {
   int n=a2i(pth,0);
   if (a2err || n<1 || n>99 || !find_films_folder(n,pth)) return(NO);
   }

if (drisdir(pth)) return(YES);
printf("Invalid path %s\n",pth);
return(NO);
}

