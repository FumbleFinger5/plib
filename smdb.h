struct	EM_KEY {char nam[60]; short year, emdb_num; int32_t imdb_num; char rating;}; // Each BTR entry includes RHDL (maybe NULL) to 'mct' MEDIA entries
struct	EM_KEY1 {EM_KEY e; char director[30], cast[60]; short added, seen, filesz;};

#pragma pack(push, 1)
struct	EM_MEDIA {char locn, fct, dct, dvd; __int64 sz;};		// The (variable length) array of media locations is in no particular sequence!!!
#pragma pack(pop)

class SMDB {
public:
SMDB(const char *path);						// Constructor used by app progs
~SMDB();
void	update_all(int slot, DYNAG *em);
int		scan_all(EM_KEY *e, int reverse_order, int *again);
void	list_slots(void);
void	list_numcopies(void);
void	list_selection(void);
void	list_no_media(void);
void	josh_copy(DYNTBL *wanted_imdb_numbers);
void	prepare_copy(void);
void	find_movie_by_imdb_num(int emdb_num);
void	test1(void);
void	test2(void);
void	test3(void);
void	purge(char *movie_folder_pth);
int		add_or_upd(EM_KEY *e, EM_MEDIA *m);	// Add or update the KEY + RECORD with passed key using passed buffers
void	remove_unmatched(DYNTBL *emk);
int		return_all_keys(DYNTBL *emk);	// Populate passed table wth all key values in *.dbf
int     get(EM_KEY *e, const char *nam=NULL, int year=0);

private:
int		ct;
HDL		db,em_btr;
RHDL	rhdl;
struct	{char ver, fill[3]; RHDL em_rhdl; char fill2[24];} hdr;
void	db_open(char *fn);
RHDL	add_or_upd_media(RHDL rh, EM_MEDIA *m, EM_MEDIA *mm, int ct); // 'ct' = number of entries in 'mm' ('m' is just a ptr to a single instance)
void	list_media(EM_KEY *e, RHDL rh, int dets);	// If dets=True, list all slot folder+size entries
};

