class CSV_READER {
public:
CSV_READER(const char *fn);
~CSV_READER();
int   fill_tbl(DYNTBL *tbl);

private:
int 	readline(void);
int 	parse_first_csv_line(void);
int 	str2EM_KEY(EM_KEY1 *e);
char  buf[500];
HDL 	f;
int	csv_title_fldno, csv_year_fldno, csv_emdb_num_fldno,
		csv_imdb_num_fldno, csv_rating_fldno, csv_director_fldno,
		csv_added_fldno, csv_seen_fldno, csv_filesz_fldno;
int	again, filedate;
};

