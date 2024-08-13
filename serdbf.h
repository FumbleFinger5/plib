struct SERIES
    {
    int32_t series_imno;
    uchar season, episode, rating, tv;
    int32_t watched;        // Contains "series_tmdbID"  series-level records (where season=0, episode=0)
    union {
        char *blob;         // ptr to character buffer 'blob' used by app code
        struct {
            int32_t rhlocn;   // Record handle of optional multiple 'instance' recs
            uchar locn[4];    // if only 1 'instance', this is it, else rhinst record contains 1 or more additional 
            };
        } uni;                // Using 'uni' as the name of the union
    };                  // blob is 8 bytes, but the btree key's rhdl containing the string is only 4 bytes!

struct SERIES_TITLE {char *name; int32_t imno;};


// uses multistring (tab-separated, not nullbytes), each starts/w 2 digits (uchar ID-code) followed by name
class LOCN_INST {
public:
LOCN_INST(const char *lox);   // tab-separated multistring of locations
~LOCN_INST();
char    *get(uchar n);     // return string Folder or DeviceName from 1-char ID
uchar   get(char *s);      // convert string "Folder or Device Within Path" to 1-char ID, adding to table if n/f
char *lox(void);            // return revised 'lox' for file update if more path elements were added
bool upd;         // set 'true' if path elements added
//private:
DYNAG *tbl;
};

class SERDBF : public DBX {
public:
SERDBF();				// Constructor used by app progs
~SERDBF();
void put(SERIES *ser);
void put(SERIES *ser, const char *pth);  // passed 'path' = location of one episode
void del(SERIES *ser);
bool get(SERIES *ser, bool blob_wanted=false);   // allocate ser.blob and load if wanted=true
bool get_next(SERIES *ser, int *again);          // Get next rec (ascending key). FIRST rec when *again=0 
void update_rating(SERIES *ser);
void list(int flag);        // bitflag 1:list blob, 2:list locn(s)

private:
void	db_open(char *fn);
HDL	db,ser_btr;
struct	{char ver, fill[3]; RHDL ser_rhdl, locn_rhdl; char fill2[20];} hdr;
};
