
#define	DAY_SECS	(24L*60L*60L)
#define	ONE_DAY		86400		// Number of seconds in 24 hours
#define	BD1980		3652		// (short) binary date value of 01/01/1980
#define	BIGDATE		21915		// Latest (short) date we accept = 01/01/2030 (latest we COULD accept=24855 = 19/01/38)
#define	BIGDATL		(BIGDATE*ONE_DAY)

//	Calendar Period types
#define	CAL_DAYS	0
#define	CAL_WKDAYS	1
#define	CAL_WKS		2
#define	CAL_MTHS	3
#define	CAL_QTRS	4
#define	CAL_YRS		5

//	Calendar error indicators
#define CE_YR		1
#define CE_MTH		2
#define CE_DAY		3
#define CE_HR		4
#define CE_MIN		5
#define CE_SEC		6

#define	short_bd(dttm)	((short)((dttm)/ONE_DAY))
#define	long_date(bd)	((long)(bd)*ONE_DAY)
#define	short_hm(dttm)	((short)(((dttm)%ONE_DAY)/60))

char	*dmy_stri(short bd);
short cal_parse(const char *p);	// Parse emdb date string

struct _TMR
	{
	long	secs;
	int		ticks;
	};

int 	calerr(void);
char	*calfmt(char *str,const char *ctl,long cal);
long	caljoin(int yr,int mo,int dy,int hr,int mn,int sc);
long	calnow(void);		// Same as Linux time_t now = std::time(0)
void	calsplit(long cal,short *yr,short *mo,short *dy,short *hr,short *mn,short *sc);
//void	cal_check_date_range(short bd0, short bd1);

