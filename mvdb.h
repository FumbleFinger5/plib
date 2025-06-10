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
   ~MVDB();
   bool  get(int mode, BL_CARGO *blc, DYNTBL *tbl_imsz);
   void  del(uchar number);
   bool  upd_if_changed(BL_CARGO *blc, DYNTBL *tbl_imsz);
   private:
   void	db_open(char *fn);
   HDL	bl_btr;                 // tree of barline data for all known .../FilmsNN
   struct	{char ver, fill[3]; RHDL bl_rh; char fill2[20];} hdr;
   };

char *mvdb_number2path(char *pth, int number, int mnt);    // gives fully-qualified path to FilmsNN (in media or mnt)

