#include <stdio.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <cstdio>
#include <string.h>
#include <cstdlib>

#include <unistd.h>

#include "pdef.h"
#include "memgive.h"
#include "str.h"
#include "log.h"
#include "flopen.h"

#include "exec.h"

static int kbhit()
{
static const int STDIN = 0;
static int initialized = 0;
if (! initialized)
    {
    struct termios term;
    tcgetattr(STDIN, &term);
    term.c_lflag &= ~ICANON;
    tcsetattr(STDIN, TCSANOW, &term);
    setbuf(stdin, NULL);
    initialized = 1;
    }
int bytesWaiting;
ioctl(STDIN, FIONREAD, &bytesWaiting);
return bytesWaiting;
}

static int  getch(void)
{
struct termios oldattr, newattr;
int ch;
tcgetattr( STDIN_FILENO, &oldattr );
newattr = oldattr;
newattr.c_lflag &= ~( ICANON | ECHO );
tcsetattr( STDIN_FILENO, TCSANOW, &newattr );
ch = getchar();
tcsetattr( STDIN_FILENO, TCSANOW, &oldattr );
return ch;
}

int keypress_waiting(void)
{
fflush(stdout);
if (kbhit()) return(getch());
return(0);
}

DYNAG *f2tbl(FILE *f)
{
DYNAG *t=NULL;
char *buf=(char*)memgive(4096);
int i;
while (fgets(buf,4096,f)!=0)
    {
    if (t==NULL) t=new DYNAG(0);
    while ((i=strlen(buf))>0 && buf[i-1]<SPACE) buf[i-1]=0; // remove any trailing CR and/or LF, etc.
    t->put(strtrim(buf));
    }
memtake(buf);
return(t);
}

DYNAG *exec2tbl(const char *cmd)
{
FILE *f = popen(cmd,"r");
if (f==NULL) throw(77);
DYNAG *t=f2tbl(f);
int err=pclose(f);
if (err<0) {sjhlog("\ncommand [%s] failed with %d\n",cmd,err); throw(77);}
//if (err) sjhlog("\ncommand [%s] returned exit code:%d\n",cmd,err); // Esc / WndClose / etc - don't care
return(t);
}

int drive_free(const char *pth)  // pth = file OR foldername (DRVSTAT containing device / drive)
{
char cmd[256];
strfmt(cmd,"df -BG --output=avail \"%s\"",pth);
DYNAG *t=exec2tbl(cmd);
char *s=(char*)t->get(1);       // Read the SECOND line output by 'df' command
if (!s) {sjhlog("Command [%s] failed",cmd); throw(77);}
int gb=a2i(s,0);
if (!*s || a2err==2 || (a2err==1 && a2err_char!='G')) {sjhlog("Command [%s] failed",cmd); throw(77);}
delete t;
return(gb);
}

int exec_cmd(const char *cmd, char *buf, int bufsz) // 0 if ok, else error code
{
FILE *f = popen(cmd,"r");
int bytes=fread(buf, 1, bufsz-2, f);
buf[bytes]=0;
buf[bufsz-1]=0;
int err=pclose(f);
if (err<0) {Sjhlog("exec_cmd crashed!\r\n%s\r\n",cmd); throw(55);}
return(err);
}

void execute(const char *prg, const char *arg)
{
int id = fork();
if (id == 0)  // child process
    {
    execlp(prg, prg, arg, (char *)NULL);
    }
}


