#ifndef TVDB_H
#define TVDB_H

#include "db.h"
#include "memgive.h"
#include "str.h"
#include "qblob.h"
#include "tvdb.h"

extern const char *CATEGORIES;

#define TVD_TV 1            // TMDB calls it "tv", not "movie"
#define TVD_NO_SEASONS 2    // There's no season-level folder (if TV series, all episodes are Season 1)

struct LOCN_SZ
   {
   short locn_id;    // 0=Unused (not all 4 present) else subscript of this episode fullpath into dbf master paths table
   uchar locn_sz;    // approximate filesize mangled into a single byte, burely for comparisons
   char   filler;
   };

struct EPI_DET
   {
   short year, runtime;
   int32_t id;
   };

struct TVD
   {
   int32_t series_imno;
   uchar   season, episode;
   uchar   flag, rating;       // flagbits: 1=TV, 2=No_seasons
   union
      {
      int32_t watched;        // Contains "series_tmdbID"  series-level records (where season=0, episode=0)
      int32_t series_tmno;
      } uni1;
   LOCN_SZ lsz[4];      // Store up to 4 "locn+sz" tuples for this imdbID+Season+Episode]
   };

struct TVE
   {
   TVD tvd;
   JBLOB_READER *jb;
   RHDL rh;
   };

struct TREE_LEG
   {
   int32_t series_imno;
   uchar   season, episode;
   short legs_i[4];           // Up to 4 legs in the common parts of episode path starting from Category
   short drives_i[4];         // Subscripts of up to 4 different "drives" components in tracked episode paths
   };



void grab(int i);

class TVDB : public DBX {
public:
TVDB();				// Constructor used by app progs
~TVDB();
void put(TVE *tvd);
void put(TVE *tvd, PATHDYNAG *pd);  // full path to folder containing ONE episode (in tvd database)
void del(TVE *tvd);        // delete the specified episode
bool  del_path(const char *path);  // Delete path and remove all references to it in episode records
                              // If that leaves an episode with no "locn" entries, DELETE the episode
                              // otherwise update locn number list in all episodes GE this number

bool get(TVE *tvd, bool blob_wanted=false);   // allocate tvd.blob and load if wanted=true
bool get_next(TVE *tvd, int *again);          // Get next rec (ascending key). FIRST rec when *again=0 
bool key_exists(TVD *tvd);
void update_rating(TVE *tvd);
bool list(int flag, const char *pth);  // bitflag 1:list blob, 2:list locn(s) for each episode
                                       // if pth!=NULL, list only entries for that specfic location
void list_locn_all(void);   // just list all location strings
DYNTBL *xport(void);   // write episodes.lst for mytree into a table

private:
void	db_open(char *fn);
short locn_sz_find_or_add(DYNAG *subpath);
short get_locn_id(const char *pth);
void  possible_anchor_update(RHDL new_locn_rh);    // Update anchor hdr if passed rh != existing hdr.locn_rh
HDL	ser_btr;
struct	{char ver, fill[3]; RHDL ser_rhdl, locn_rhdl; char fill2[20];} hdr;
DYNAG *tpth;   // Table of locn subpaths. Variable-length, no sequence (dummy element 0 contains "n/a" as a convenience)
const char *dummy_path0="Dummy subscript zero path";
};


struct PRVI {int32_t i, line;};

class TREE_BUILDER {
public:
TREE_BUILDER(TVDB *_tvdb);
//~TREE_BUILDER();
bool  next(char *desc, int *parent_id);
void  listall(void);
private:
//bool seasonless(TREE_LEG *tl);
TVDB *tvdb;
DYNTBL *epi_legs;
DYNAG *all_legs;
int tli=0, lct=0;
PRVI prvi[4];
};


#endif   // TVDB_H