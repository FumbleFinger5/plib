int drive_free(const char *pth);		// pth = file OR foldername (DRVSTAT containing device / drive)

int keypress_waiting(void);			// Return 0 immediately if no key pressed, else return the actual key

DYNAG *exec2tbl(const char *cmd);	// Execute shell command and return all output lines as a table
DYNAG *f2tbl(FILE *f);					// return all lines in passed file handle as a table
