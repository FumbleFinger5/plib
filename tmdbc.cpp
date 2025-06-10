#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <utime.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <cstdarg>

#include "pdef.h"
#include "db.h"
#include "cal.h"
#include "drinfo.h"
#include "flopen.h"
#include "log.h"
#include "memgive.h"
#include "str.h"
#include "smdb.h"
#include "parm.h"
#include "exec.h"
#include "imdb.h"
#include "my_json.h"
#include "omdb1.h"
#include "qblob.h"
#include "tmdbc.h"


#include <unicode/translit.h>
#include <unicode/unistr.h>
#include <ctype.h>

#define STD_ERRNULL "2>/dev/null"

bool uxlt(char *str)	// Normalise (dump UNICODE) passed string e.g. u"Noël Coward"   to "Noel Coward"  IN SITU
{
UErrorCode status = U_ZERO_ERROR;
icu::UnicodeString source(str);
icu::Transliterator* transliterator = icu::Transliterator::createInstance("NFD; [:Nonspacing Mark:] Remove; NFC", UTRANS_FORWARD, status);
if (U_SUCCESS(status)) transliterator->transliterate(source); else m_finish("Transliterator error");
std::string result;
source.toUTF8String(result);
bool amd=(strcmp(str,result.c_str())!=0);
if (amd) strcpy(str,result.c_str());
delete transliterator;
return(amd);
}


// Lookup table to convert MY rating 1-99 to TMDB rating 0.5-10 (to nearest 0.5)
// TMDB values are skewed (like IMDB ratings 1-9)
// each pair in this list means "all MY ratings up to and including VALUE1 become VALUE2 as TMDB rating"
static uchar rxlt[42]={	0,0, 9,5, 12,10, 15,15, 22,20, 27,25, 32,30, 37,35, 42,40, 47,45, 52,50,
								57,55, 62,60, 67,65, 72,70, 77,75, 82,80, 86,85, 90,90, 93,95, 99,100};

int rating2tmdb(int rating)
{
if (rating<0 || rating>99) m_finish("bad rating %d",rating);
int i=0;
while (rating>rxlt[i]) i+=2;
return(rxlt[i+1]);
}

bool save_new_session(char *session_id)
{
HDL f=flopen(SESSIONFIL,"w");
flput(session_id,SESSIONSIZ,f);
flclose(f);
return(true);
}


bool authenticate_request_token(char *request_token, char *session_id)
{
char cmd[256], buf8k[8192], wrk[128];
strcpy(cmd,"curl -s -X POST -H \"Content-Type: application/json\" -d");
strendfmt(cmd," '{\"request_token\":\"%s\"}'", request_token);
strendfmt(cmd,"  'https://api.themoviedb.org/3/authentication/session/new?api_key=%s'",tapikey());
int err=exec_cmd(cmd, buf8k, 8192);
strcpy(session_id,my_json_get(wrk,buf8k,sizeof(wrk),"session_id",0));
return(true);
}

bool get_request_token_url(char *url, char *request_token)
{
char cmd[256], buf8k[8192], wrk[256];
strfmt(cmd,"curl -s https://api.themoviedb.org/3/authentication/token/new?api_key=%s",tapikey());
int err=exec_cmd(cmd, buf8k, 8192);
if (err) request_token[0]=0;
else strcpy(request_token,my_json_get(wrk,buf8k,sizeof(wrk),"request_token",0));
if (err!=0 || request_token[0]==0) m_finish("tmdb request token error");
strfmt(url,"https://www.themoviedb.org/authenticate/%s",request_token);
return(true);
}


static bool less_than_50_minutes_ago(int32_t dttm)
{
int32_t secs=calnow()-dttm;
if ((secs/60)<50) return(true);
return(false);
}

bool session_id_read(char *session_id)
{
FILEINFO fi;
if (drinfo(SESSIONFIL,&fi)==0) return(false);
if (!less_than_50_minutes_ago(fi.dttm)) return(false);	// Don't even bother testing if < 10 minutes left to live!
HDL f=flopen(SESSIONFIL,"r");
if (flget(session_id,SESSIONSIZ,f)!=SESSIONSIZ) return(false);
flclose(f);
session_id[SESSIONSIZ]=0;		// because there's no EOS nullbyte in the 40-byte file itself
return(true);
}

bool session_id_works(char *session_id)
{
char cmd[256], buf8k[8192], wrk[32];
if (!session_id_read(session_id)) return(false); //m_finish("SessionID read error");
strfmt(cmd,"curl 'https://api.themoviedb.org/3/account?api_key=%s&session_id=%s' %s",tapikey(),session_id, STD_ERRNULL);
int err=exec_cmd(cmd, buf8k, 8192);
strcpy(wrk,my_json_get(wrk,buf8k,sizeof(wrk),"username",0));
bool ok=(err==0 && wrk[0]!=0);
return(ok);
}

const char *result_type[3]={"movie_results","tv_results",0};
const char *mtyp[2]={"movie","tv"};

static void json2blob(JBLOB_READER *jb, const char *new_name, const char *buf8k, const char *old_name)
{
char wrk[4096];			// March 2024 - now picking up "overview", so allow plenty of space
my_json_get(wrk,buf8k,sizeof(wrk),old_name,0);
jarray2list(wrk);
uxlt(wrk);						// 28/2/24 - "normalise" Unicode characters in ALL fields
if (SAME4BYTES(wrk,"[ ]")) MOVE4BYTES(wrk,"N/A");
if (*wrk)                  // no need to test & copy - just leave any existing value if no new value 
   jb->put(new_name, wrk);
}

static void copy2jb(JBLOB_READER *dst, JBLOB_READER *src, const char *id)
{
const char *value=src->get(id);
if (*value) dst->put(id, value);
}

static bool json_empty(const char *ptr)
{
return(ptr==NULL || *ptr==0 || SAME4BYTES(ptr,"N/A") || SAME4BYTES(ptr,"[ ]") || SAME3BYTES(ptr,"[]"));
}

static void add_missing_tmdb_from_omdb(JBLOB_READER *tmdb, JBLOB_READER *omdb, const char *name)
{
const char *ptr=tmdb->get(name), *ptr2;
if (json_empty(ptr))
   if (!json_empty(ptr2=omdb->get(name)))
      tmdb->put(name,ptr2);
}

static void add2tbuf_from_omdb(int32_t imno, JBLOB_READER *jb)
{
char buf8k[8196];
if (!omdb_all_from_number(imno, buf8k)) {sjhlog("add2tbuf_from_omdb FAILED imno:%d",imno); return;}
JBLOB_READER jb2(buf8k);
copy2jb(jb,&jb2,"imdbRating");
copy2jb(jb,&jb2,"imdbVotes");
// jb = TMDB values, jb2 = OMDB values
add_missing_tmdb_from_omdb(jb,&jb2,"Director");       // POPULATE MISSING TMDB VALUES FROM OMDB VALUES for these fields
add_missing_tmdb_from_omdb(jb,&jb2,"Actors");
add_missing_tmdb_from_omdb(jb,&jb2,"Genre");
}

// FLDX fld defines the field names expected in 'blob' regardless of what omdb or tmdb called them
static void add2tbuf(JBLOB_READER *jb, OMZ *k)
{
char cmd[512], buf8k[8192];
strfmt(cmd,"curl -s 'https://api.themoviedb.org/3/%s/%d?api_key=%s", mtyp[k->k.tv], k->k.tmno, tapikey());
strcat(cmd,"&append_to_response=credits,release_dates'");
strcat(cmd," | jq '. | {");

if (k->k.tv)
	strcat(cmd,"title: .name, release_year: .air_date, runtime: (.episode_run_time | if . then .[0] else null end)");
else
	strcat(cmd,"title: .title, release_year: .release_date, runtime: .runtime");

strcat(cmd,", director: [.credits.crew[] | select(.job == \"Director\") | .name]");
strcat(cmd,", actors: (if .credits.cast then [.credits.cast[0:5][] | .name] else [] end)");
strcat(cmd,", genres: [.genres[] | .name], overview: .overview");
strcat(cmd,"}'");
int err=exec_cmd(cmd, buf8k, sizeof(buf8k));
if (err) {sjhlog("tmdbc.cpp:add2tbuf() cmd:\n%s\nFailed!");}
json2blob(jb,"Title",buf8k,"title");
json2blob(jb,"Year",buf8k,"release_year");
json2blob(jb,"Runtime",buf8k,"runtime");
json2blob(jb,"Director",buf8k,"director");
json2blob(jb,"Actors",buf8k,"actors");
json2blob(jb,"Genre",buf8k,"genres");
json2blob(jb,"plot",buf8k,"overview");
}

static bool get_tmdb_result_type(uchar *tv, const char *buf8k) /// does buf8k contain tmdb "tv" or "movie" or NO results?
{
for (int i=0;i<2;i++) if (json_contains_data(buf8k, result_type[i])) {*tv=i; return(true);}
sjhlog("bad TMNO");
return(false);
}

// Passed 'buf' contains tmdb response to 'basic details' api call citing the IMDB number
// Get TMDB number, and check which of "movie/tv results" exists, setting tmdbTV to 0/1
// Put those 2 fields into a blob and call Add2tbuf to retrieve & add all other fields of interest
// Before return, replace the passed contents of 'buf' with the newly-constructed json doc in the blob.
static bool tmdb2omdb(OMZ *oz, char *buf8k)
{
char wrk[128];
JBLOB_READER jb("");								// Create initially empty parser
jb.put("imdbID",strfmt(wrk,"%d",oz->k.imno));
if (!get_tmdb_result_type(&oz->k.tv,buf8k)) return(false);
my_json_get(wrk,buf8k,sizeof(wrk),"id",result_type[oz->k.tv],0);	// get the TMDB id
oz->k.tmno = a2l(wrk,0);
if (oz->k.tmno<1) m_finish("Shite!");
jb.put("tmdbID",wrk);
jb.put("tmdbTV",oz->k.tv?"1":"0");

//my_json_get(wrk,buf8k,sizeof(wrk),"first_air_date",result_type[oz->k.tv],0);
// Nov '24 (Red Rose) passed buf8k has movie AND tv_results, but "first_air_date" is only in tv_results
if (json_contains_data(buf8k,result_type[1]))   // are tv_results present?
   {
   my_json_get(wrk,buf8k,sizeof(wrk),"first_air_date",result_type[1],0);
   if (valid_movie_year(a2i(wrk,wrk[4]=0)))
      jb.put("Year",wrk);
   }
add2tbuf(&jb, oz);			// UniqCall
add2tbuf_from_omdb(oz->k.imno,&jb);
strcpy(buf8k,jb.get(NULL));
return(true);
}

bool tmdb_find_imno(int32_t imno, char *buf8k)
{
char cmd[256];       /// todo  -sS should be ok here rather than STD_ERRNULL
//strfmt(cmd,"curl -sS 'https://api.themoviedb.org/3/find/tt%07d?api_key=%s&external_source=imdb_id'", imno, tapikey());
strfmt(cmd,"curl 'https://api.themoviedb.org/3/find/tt%07d?api_key=%s", imno, tapikey());
strendfmt(cmd,"%s %s","&external_source=imdb_id'", STD_ERRNULL);
int err=exec_cmd(cmd, buf8k, 8192);
return(err==0);
}

bool tmdb_all_from_number(OMZ *oz, char *buf8k)
{
if (!tmdb_find_imno(oz->k.imno,buf8k)) // "first_air_date":"1996-05-12" // CAN'T SUCCEED IF oz->k.imno==0 !!!!!!!
	return(false);
return(tmdb2omdb(oz, buf8k));	// UniqCall RE-fill buf8k with data gotten from tmdb, reformatted to look like omdb
}


static bool is_success_true(const char* ip_buf)
{
struct json_object *parsed_json, *success;
bool result = false;
parsed_json = json_tokener_parse(ip_buf);
if (parsed_json != NULL && json_object_object_get_ex(parsed_json, "success", &success))
   result = json_object_get_boolean(success);
json_object_put(parsed_json); // Properly release the JSON object
return result;
}

bool tmdb_set_rating(bool (*authenticate)(char *session_id),
								int32_t tmno, int32_t imno, int tv, int season, int episode, int my_rating)
{
char session_id[64];
bool ok=authenticate(session_id);
if (!ok) m_finish("shite!");
char cmd[256], item[32], buf8k[8192];
if (my_rating==0)		// then we want to DELETE the tmdb rating
	{
	strfmt(item,"%d",tmno);
	if (season) strendfmt(item,"/season %d",season);
	if (episode) strendfmt(item,"/episode %d",episode);
	strcpy(cmd,"curl -X DELETE -H \"Content-Type: application/json\" -d '{}' 'https://api.themoviedb.org/3/");
	strendfmt(cmd,"%s/%s/rating?api_key=%s&session_id=%s' %s",mtyp[tv],item,tapikey(),session_id, STD_ERRNULL);
	}
else
	{
	int rating=rating2tmdb(my_rating);
	strfmt(item,"%d",rating/10);
	if ((rating%10)!=0) strendfmt(item,".%d",rating%10);
	strfmt(cmd,"curl -s -X POST -H \"Content-Type: application/json\" -d '{\"value\": %s}'",item);
	strendfmt(cmd," 'https://api.themoviedb.org/3/%s/%d/rating",mtyp[tv],tmno);
	strendfmt(cmd,"?api_key=%s&session_id=%s' %s",tapikey(),session_id, STD_ERRNULL);
	}
if (tmdb_api(cmd, buf8k, 8192))
	{
	bool ok=is_success_true(buf8k);
	if (!ok) sjhlog("cmd\n%s\nbuf8k\n%s\n",cmd,buf8k);
	return(true);
	}
sjhlog("I:%d rating change failed!",imno);
printf("cmd\n%s\nRating change failed!\n",cmd);
return(false);
}

int tmdb_get_rating(bool (*authenticate)(char *session_id),int32_t tmno, int32_t imno, int tv, int sea, int epi)
{
char session_id[64];
bool ok=authenticate(session_id);
int rating=0;
if (!ok) m_finish("shite!");
char cmd[256], item[32], buf8k[8192];
strfmt(item,"%d",tmno);
if (sea) strendfmt(item,"/season %d",sea);
if (epi) strendfmt(item,"/episode %d",epi);
strcpy(cmd,"curl -s 'https://api.themoviedb.org/3/");
strendfmt(cmd,"%s/%s/account_states?api_key=%s&session_id=%s' %s",mtyp[tv],item,tapikey(),session_id, STD_ERRNULL);
if (tmdb_api(cmd, buf8k, 8192))
	{
	struct json_object *parsed_json, *rated;
	const char *str = NULL;
	char wrk[32] ={0}, wrk2[32];
	parsed_json = json_tokener_parse(buf8k);
	if (parsed_json != NULL && json_object_object_get_ex(parsed_json, "rated", &rated)
	 && (str=json_object_get_string(rated))!=nullptr)
	   strancpy(wrk,str,sizeof(wrk));
	json_object_put(parsed_json); // Properly release the JSON object
	if (!wrk[0] || !strcmp(wrk,"false")) return(0);

my_json_get(wrk2, wrk, sizeof(wrk2), "value", 0); // fldnam THEN section

	rating=a2i(wrk2,0)*10;
	int i;
	if ((i=stridxc('.',wrk2))!=NOTFND)
		rating+=a2i(&wrk2[i+1],1);				// should only be "n.5"
	return(rating);
	}
sjhlog("I:%d rating query failed!",imno);
printf("cmd\n%s\nRating query failed!\n",cmd);
return(rating);
}

bool tmdb_api(const char *cmd, char *buf, int bufsz)
{
bool nobuf=(buf==0);
if (nobuf != (bufsz==0)) m_finish("buf8k param error");
if (nobuf) buf=(char*)memgive(bufsz=8192);
int err=exec_cmd(cmd, buf, bufsz);
if (nobuf) memtake(buf);
return(err==0);
}
