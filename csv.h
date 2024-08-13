#include "smdb.h"

struct NAMCT {char nam[30]; short ct;};
// sort actors in ASCENDING order of "popularity" (No. of times they appear in my movies)
int _cdecl cp_namct(NAMCT *a, NAMCT *b);

// Sorts up to 5 comma-separated "Firstname(s) + Surname" sub-elements in each item
int cp_name(char *a, char *b);

/*
class CSV_READER {
public:
CSV_READER(const char *fn);
~CSV_READER();
int   fill_tbl(DYNTBL *tbl);
void  fill_starring(void);

private:
int 	readline(void);
int 	parse_first_csv_line(void);
int 	str2EM_KEY(EM_KEY1 *e);
void	str2starring(DYNTBL *a);	// 'a' = either actors in ALL movies, or just current one
void  biggest_stars_first(char *w);	// use 'ncs' to resequence passed list of actors names
char  buf[500];
DYNTBL *starring;
HDL 	f;
int	csv_title_fldno, csv_year_fldno, csv_emdb_num_fldno,
		csv_imno_fldno, csv_rating_fldno, csv_director_fldno,
		csv_added_fldno, csv_seen_fldno, csv_filesz_fldno, csv_cast_fldno, csv_runtime_fldno;
int	filedate;
};
*/