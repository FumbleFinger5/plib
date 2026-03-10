#ifndef MVDB_H
#define MVDB_H

#include "pdef.h"
#include "db.h"
#include "memgive.h"

#pragma pack(push, 1)
struct IMSZ
   {
   int32_t imno;
   int64_t sz;
   };
struct RIM
   {
   uchar rating;
   int32_t imno;
   };
#pragma pack(pop)

#pragma pack(push, 1)
struct DRVDET
   {
   uchar number;     // the NN value of this FilmsNN drive
   RIM rim[2];       // lowest / highest 'rating+imno' key values on this drive
   int64_t drive_free_space;
   char fn[128];         // empty string if offline, otherwise full path to FilmsNN folder
   };
#pragma pack(pop)

struct BL_DET {short ct; int64_t sz;};
struct BL_CARGO // PERMANENT dets stored in dbf btree cargo
   {
   uchar number;      // 0 for .../_seen (if present, must be FIRST), else the number of this FilmsNN folder
   short Qlo,Qhi;       // OBSOLETE preferred range of ratings to be stored in this location
   BL_DET tot;       // Total ct+sz of all movies in this location
   int64_t drive_free_space;
   };

class MVDB : public DBX {				// Accesses "FilmsNN" database moovie.dbf
   public:
   MVDB(void);
   bool  get(int mode, BL_CARGO *blc, DYNTBL *tbl_imsz);
   void get_drvdets(DYNTBL *tbl_drvdets); // populate passed table with all FilmsNN details on file
   void  del(uchar number);
   bool  upd_if_changed(BL_CARGO *blc, DYNTBL *tbl_imsz);
   private:
   void	db_open(char *fn);
   HDL	bl_btr;                 // tree of barline data for all known .../FilmsNN
   struct	{char ver, fill[3]; RHDL bl_rh; char fill2[20];} hdr;
   };

char *mvdb_number2path(char *pth, int number, int mnt);    // gives fully-qualified path to FilmsNN (in media or mnt)

#endif // MVDB_H
