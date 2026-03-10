#include "pdef.h"
#include "memgive.h"
#include "db.h"
#include "mvdb.h"

#pragma pack(push, 1)
struct NN_FREE {uchar number; int64_t free_space;};
struct NN_KEY {NN_FREE nf; RHDL rh;};    // rhdl of btr containing rating+imno+sz
#pragma pack(pop)

class MVHL : public DBX {				   // Access "FilmsNN" database MVHL.dbf
public:
MVHL(bool create=false);
DYNAG *get(uchar number);     // DYNAG, not DYNTBL - search with in_table() + comparator cp_long
DYNAG *NNget(void);           // returns NN_FREE tbl of all DrvNum+FreeSpace (not IMSZ imno+size table)
bool NNput(uchar number, int64_t free_space, DYNAG *tbl); // If changed, update FilmsNN. Return 'changed'
private:
void db_open(char *fn);
//HDL	 db;      // btree of all FilmsNN drives/folders
struct	{char ver, fill[3]; RHDL nn_rh; } hdr;
};
