class SCAN_ALL {
public:
SCAN_ALL(void);						// Constructor used by app progs
char *get(int32_t imno, char fld_id, char *buf);
int32_t scan_all(int *subscript); // successively return 'Next IMDB Number' until NOTFND
~SCAN_ALL();
int mct;
private:
void init(void);
FCTL *_ff, *ff;
char idx[32];
int fct;
};
