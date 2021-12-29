class DIRSCAN {
public:
DIRSCAN(const char *pth, const char *msk=NULL, int flg=0);		// Path to *.cfg file
~DIRSCAN();
struct dirent *next(void);
private:
DIR     *dir;
struct dirent *entry;
char *mask,*maskext;
int flag;
};

// flagbits
#define DS_DOT 1
#define DS_HIDDEN 2
#define DS_DIR 4
