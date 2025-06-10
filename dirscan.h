// Returns just 'filename' in next 'dirent', but FULLPATH in FILEINFO if passed
class DIRSCAN {
public:
DIRSCAN(const char *pth, const char *msk=NULL);
~DIRSCAN();
struct dirent *next(FILEINFO *fi=0);	// If !=0 use stat() for fi->size AND put FULLPATH into fi->name
private:
DIR     *dir;
struct dirent *entry;	// member 'd_type' contains flagbits as below
char *mask,*maskext;
char *_pth;
};

// flagbits
//#define DS_DOT 1
//#define DS_HIDDEN 2
//#define DS_DIR 4		// dirent.h defines DT_DIR 4

/*
new function
int type_filter_wanted, type_filter_unwanted=(DS_DOT|DS_HIDDEN);
void set_type_filter(int bitflags, bool wanted)
{
if (wanted) type_filter_wanted=bitflags;
else type_filter_unwanted=bitflags;
}
add to next()...
if (type_filter_wanted!=0 && (type_filter_wanted&entry->d_type)==0) continue
if (type_filter_unwanted!=0 && (type_filter_unwanted&entry->d_type)!=0) continue
*/

bool fn_cdx(const char *n);		// Check if video filename ends with "CDn" for multi-part movie.

// Simple class to load directory contents into a DYNTBL
class DIRTBL : public DYNAG	 {  // subclass of DYNAG - returns JUST filename - not FULLPATH
public:
DIRTBL	(const char *pth);
private:
};

