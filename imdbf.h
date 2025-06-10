#ifndef IMDBF_H
#define IMDBF_H

#include "db.h"
#include "imdb.h"

//char *genre_compress(const char *ip, char *op); // convert full genre names to single characters  STATIC!!
char *genre_uncompress(const char *ip, char *op); // or convert back again. 'op' must be big enough!

const char *get_fld_name(char fld_id);


class IMDB_FLD : public DBX	// maintain imdb.dbf optimised store of api values used by q3
{										// IMDB_FLD no longer supports get(MovieID,FldID), 'cos SCAN_ALL does that now
public:
	IMDB_FLD(void); // Constructor used by app progs
	~IMDB_FLD();
	bool exists(int32_t imno) { return (imx->find(&imno) != NULL); };
	void load_fctl(FCTL *f);
	int recct(void) { return (imx->ct); };
	void put(int32_t imno, const char *buf); // store the entire 'buf' returned by api call
	void put_imno(int32_t imno);				  // append another movieeNo to FID_IMDB record
	bool put2a(const char *val);						  // GENRE append ky.fid value for new movieNo to fld-specific record
	bool put2b(const char *val);						  // CAST append ky.fid value for new movieNo to fld-specific record
	bool put2c(const char *val);						  // 1-char
	bool del(int32_t imno);						  // delete this movie completely
	bool upd(DYNAG *avd);			// rewrite FLD_TITLE av rec with amended list
	bool upd_title(int32_t imno, const char *title);
private:
	bool del2a(char fld_id, int xlt); // delete movie at pos 'xlt' from this fld_id (already deleted in master table)
	bool del2b(char fld_id, int xlt); // same as del2a() except that does GENRE, this does CAST (maybe others later)
	bool del2c(char fld_id, int xlt); // ditto but YEAR, RUNTIME (1 or 2-byte rH table ONLY)
	bool del_imno(int32_t imno); // delete the master record
	void db_open(char *fn);
	void load_imx(void);
	void getkey(char fld_id);
	void *load_rH(int elem_sz, int adj); // load rH record and check size (number of elements)

	HDL fld_btr;
	DYNTBL *imx; // Array of IMN2XLT elements (1 for each imno, with associated xlt redirection)
	struct {char ver, fill[31]; RHDL fld_rhdl; char fill2[24];} hdr;
	RHDL rH;
	KYX kY;
//   JSS4 *jss4;
};


#endif // IMDB_H
