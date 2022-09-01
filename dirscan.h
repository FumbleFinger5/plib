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
