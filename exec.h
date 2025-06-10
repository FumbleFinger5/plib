int drive_free(const char *pth);		// pth = file OR foldername (DRVSTAT containing device / drive)

int keypress_waiting(void);			// Return 0 immediately if no key pressed, else return the actual key

DYNAG *exec2tbl(const char *cmd);	// Execute shell command and return all output lines as a table
DYNAG *f2tbl(FILE *f);					// return all lines in passed file handle as a table

int exec_cmd(const char *cmd, char *buf, int bufsz); // 0 if okay, else error code
int exec_cmd_wait(const char *cmd); // Don't return until cmd has finished! Ret = 0 if okay, else error code
void execute(const char *prg, const char *arg);

extern bool exec_cmd_lf;            // set TRUE if want to read past any CR/LF after piping thru jq

// Execute a command with the shell, without capturing output
int exec_cmd_fork(const char *cmd); // 0 if okay, else error code

void visit_imdb_webpage(int32_t imno);

/// Rename fully-qualified file or folder OR MOVE IT. Crash if any error.
void exec_rename(const char *from, const char *to);

