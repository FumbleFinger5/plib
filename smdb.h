#ifndef SMDB_H
#define SMDB_H

#include "db.h"

#include <cstdint>

struct	EMK {char locn, rating, partwatch, dvd; short seen; int64_t sz;};
struct	EMKi {int32_t imno; EMK e;};

class SMDB {	// maintains record of all FilesNN copies of each movie in the library 
public:
SMDB();
~SMDB();
void add_or_upd(EMKi *k);
bool del(EMKi *k);
DYNTBL *get(int32_t imno);		// return (maybe empty) table of EMK entries for movie
DYNTBL *get(uchar locn);			// return (maybe empty) table of EMK entries for locn
void list_media(int32_t imno);
void list_dvd(void);
const char *filename(void) {return(dbfnam(db));}
private:
HDL	db,flm_btr;
struct	{char ver, fill[3]; RHDL flm_rhdl; char fill2[20];} hdr;
void	db_open(char *fn);
};

#endif
