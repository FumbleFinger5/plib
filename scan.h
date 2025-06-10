#include "memgive.h"
#include "imdbf.h"

class SCAN_ALL {
public:
SCAN_ALL(void);						// Constructor used by app progs
char *get(int32_t imno, char fld_id, char *buf);   // Return 'buf' as convenience with requested field value
int32_t scan_all(int *subscript); // successively return 'Next IMDB Number' until NOTFND
~SCAN_ALL();
int mct;
private:
FCTL *_ff, *ff;
char idx[32];
int fct;
};
