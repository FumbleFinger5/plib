#include <dirent.h>

char *drfullpath(char *path, const char *p);		// Return FULLPATH which NEVER ends in '\'
char *drfullpath(char *in_situ_path);	// as 2-param version, but updates passed param IN SITU
char *drfulldir(char *fullpath, const char *path);	// Return FULLPATH which ALWAYS ends in '/'

int drunlink(const char *filename);


struct FILEINFO
	{
	ushort	attr;		// attributes of file : system, hidden, directory etc
	long	dttm;		// creation date and time of file
	int64_t	size;		// size of file in bytes
	char	name[255];	// name of file (or directory)
	};

DIR		*drscnist(const char *path);

int		drscnnxt(DIR *scn, FILEINFO *fi);
void	drscnrls(DIR *scn);

bool drinfo(const char *path, FILEINFO *pfile_info);
int drisdir(const char *directory_path);
bool drisvid(const char *extension);
const char *drext(const char *directory_path);
bool drattrget(const char *path, short *attr); // YES if file exists (fill optional 'attr')

int megabytes(int64_t size);  // DON'T USE! Just use size/1000000
uint64_t get_free_space(const char *path);  // drive free space (bytes)

bool unwanted_filename(const char *n);	// YES if it's a rarbg / yts / yify torrent file that we don't want


int64_t dr_filesize(const char *fn);		// the actual code uses #ifdef to compile special Raspberry Pi version
int64_t dr_foldersize(const char *pth);	// Recursively sum total size of specified folder

char  *drsplitpath(char *dir_part, const char *fullpath);	// basename() and dirname() give same results!

char *Basename(const char *fullpath);		// 'safe' wrapper copies fullpath to ensure it's not modified
char *Dirname(const char *fullpath);		// 'safe' wrapper copies fullpath to ensure it's not modified

int valid_films_path(char *pth);  // accept just 1-2 digits, and silently change to /media/.../FilmsNN
