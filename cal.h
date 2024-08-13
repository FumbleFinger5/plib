#ifndef CAL_H
#define CAL_H

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
#define	long_bd(bd)	((int32_t)(bd)*ONE_DAY)
#define	short_hm(dttm)	((short)(((dttm)%ONE_DAY)/60))

char	*dmy_str(int32_t bd);
char	*dmy_stri(short bd);
char	*dmy_hm_str(int32_t dttm);		// Consecutively uses one of 4 static areas for formatting
short cal_parse(const char *p);	// Parse emdb date string

int 	calerr(void);
char	*calfmt(char *str,const char *ctl,int32_t cal);
int32_t	caljoin(int yr,int mo,int dy,int hr,int mn,int sc);
int32_t	calnow(void);				// Same as Linux time_t now = std::time(0)
int32_t	cal_build_dttm(const char *d, const char *t);	// Parses __DATE__ and __TIME__ strings set by compiler
//char 		*cal_build_dttm_str(char *s);	// Parses __DATE__ and __TIME__ strings set by compiler
void	calsplit(int32_t cal,short *yr,short *mo,short *dy,short *hr,short *mn,short *sc);
//void	cal_check_date_range(short bd0, short bd1);
bool valid_movie_year(int yr);	// Is year between 1925 and current year?
int valid_3char_month(const char *s);

struct _TMR
	{
	int32_t	secs;
	int		ticks;
	};

HDL	tmrist(int32_t csecs);
void tmrreset(HDL tmr,int32_t csecs);
void tmrrls(HDL tmr); // get rid of a timer
int32_t	tmrelapsed(HDL tmr);	// check how much time has elapsed since set point
									// returns negative number if before

#endif
