#include <dirent.h>

char *drfullpath(char *path, const char *p);		// Return FULLPATH which NEVER ends in '\'
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

int drinfo(const char *path, FILEINFO *pfile_info);
int drisdir(const char *directory_path);
int drattrget(const char *path, short *attr);



