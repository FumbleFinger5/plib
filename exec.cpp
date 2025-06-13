#include <stdio.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <cstdio>
#include <string.h>
#include <cstdlib>

#include <unistd.h>
#include <sys/wait.h>

#include "pdef.h"
#include "memgive.h"
#include "str.h"
#include "log.h"
#include "flopen.h"

#include "exec.h"
#include "parm.h"



#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>

static int kbhit(void) {
    struct termios oldt, newt;
    int ch;
    int oldf;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    ch = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if (ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }

    return 0;
}

/*static int kbhit()
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
}*/

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

int exec_cmd_wait(const char *cmd) // 0 if ok, else error code
{
   int status;
   pid_t pid = fork();
   if (pid == 0) // Child process
      {
      execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
      _exit(127); // Only reached if execl() fails
      }
   if (pid < 0) return -1; // Fork failed
   waitpid(pid, &status, 0); // Wait for ffmpeg to finish
   return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

int exec_cmd(const char *cmd, char *buf, int bufsz) // 0 if ok, else error code
{
//sjhlog("exec_cmd: %s",cmd);
FILE *f = popen(cmd,"r");
int bytes=fread(buf, 1, bufsz, f);
if (bytes==bufsz) {pclose(f); return(SE_BUFF64K);}   // return -268 = buffer overflow = we're fucked!
int err=pclose(f);
if (!err) buf[bytes]=0;       // ensure the buffer is a null-terminated string
//sjhlog("err:%d bytes:%d",err,bytes);
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


int exec_cmd_fork(const char *cmd)   // Returns 0 if ok, else error code
{
pid_t pid = fork();
if (pid < 0) return -1;         // Fork failed
if (pid == 0)                   // Child process - Execute the command with the shell, without capturing output
    {
    execlp("sh", "sh", "-c", cmd, (char *)NULL);
    _exit(EXIT_FAILURE);
    }
int status;                     // Parent process
waitpid(pid, &status, 0);       // Wait for child process to finish
if (WIFEXITED(status))          // Return the exit code of the child process
   return WEXITSTATUS(status);
return -1;                      // if we get here, the command did not exit normally
}

void visit_imdb_webpage(int32_t imno)
{
char browser[128], s[128];
strfmt(s,"%s%07d","https://www.imdb.com/title/tt",imno);    // open a browser tab for imdb webpage for movie
execute(parm_str("browser",browser, "xdg-open"),s);
}

// Rename fully-qualified file or folder. Return YES if successful, else NO (fatal error?)
void exec_rename(const char *from, const char *to)
{
char cmd[1024];
int statusflag;
strfmt(cmd, "%s %c%s%c %c%s%c", "mv", CHR_QTDOUBLE, from, CHR_QTDOUBLE, CHR_QTDOUBLE, to, CHR_QTDOUBLE);
int id = fork();
if (id == 0) // then it's the child process
	execlp("/bin/sh", "/bin/sh", "-c", cmd, (char *)NULL);
wait(&statusflag); // wait(&status) until child returns, else it might not be ready to use results
if (statusflag!=0)
	{
	sjhlog("cmd: %s",cmd);
	m_finish("Error moving/renaming %s to %s",from,to);
	}
}

#include <iostream>
#include <cstring>
#include <curl/curl.h>

static char *easy_escape(char *dst, const char *src)
{
CURL *curl = curl_easy_init();
if (!curl) m_finish("Failed to initialize libcurl");
char *cstr=curl_easy_escape(curl,src,0);
strcpy(dst,cstr);
curl_free(cstr);
curl_easy_cleanup(curl);
return(dst);
}

static void easy_escape_in_situ(char *parm)
{
const char *tmp=stradup(parm);
easy_escape(parm,tmp);
memtake(tmp);
}

void escape_ampersand(char *s)	// replace every occurence of "&" with "&amp;" and make URL-compliant
{
int p;
while ((p=stridxc('&',s))!=NOTFND) s[p]='\t';
while ((p=stridxc('\t',s))!=NOTFND) strins(strdel(&s[p],1),"&amp;");
//while ((p=stridxc(CHR_QTSINGLE,s))!=NOTFND) strins(strdel(&s[p],1),"%27");
easy_escape_in_situ(s);
}
