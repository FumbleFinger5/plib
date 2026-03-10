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

// This class normally returns the movie title gotten from api call and stored in imdb.api
// BUT fmt_title() also uses OMDB1 param to lookup smdb.mst (if OM1_KEY.rhdl != NULL, return that) 
class TITLER	// Uses internal SCAN_ALL to format "Title (Year)" of passed OM1_KEY.imno
{
public:
TITLER(OMDB1 *_om);
char *fmt_title(OM1_KEY *k);	// Lookup k->imno using 'om' AND internal 'sa'
~TITLER();
private:
OMDB1 *om;
SCAN_ALL *sa;	// optimise retrieval of "re-loadable" values gotten from "expensive" omdb/tmdb api calls
char buf[128];
};
      
