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
//sjhlog("Authemticated session_id [%s] length:%d",session_id,strlen(session_id));
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
//sjhlog("User:%s",wrk);
bool ok=(err==0 && wrk[0]!=0);
//sjhlog("session:%40.40s %s",session_id,ok?"ok":"BAD!");
return(ok);
}

const char *result_type[3]={"movie_results","tv_results",0};
const char *mtyp[2]={"movie","tv"};

static void json2blob(JBLOB_READER *jb, const char *new_name, const char *buf8k, const char *old_name)
{
char wrk[4096];			// March 2024 - now picking up "overview", so allow plenty of space
my_json_get(wrk,buf8k,sizeof(wrk),old_name,0);
if (SAME3BYTES(wrk,"[ \""))
    jarray2list(wrk);
uxlt(wrk);						// 28/2/24 - "normalise" Unicode characters in ALL fields 
jb->put(new_name, wrk);
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
if (err) {sjhlog("tmdbc.cpp Line 161, cmd:\n%s\nFailed!");}

json2blob(jb,"Title",buf8k,"title");
json2blob(jb,"Year",buf8k,"release_year");
json2blob(jb,"Runtime",buf8k,"runtime");
json2blob(jb,"Director",buf8k,"director");
json2blob(jb,"Actors",buf8k,"actors");
json2blob(jb,"Genre",buf8k,"genres");
json2blob(jb,"plot",buf8k,"overview");
}

int get_tmdb_result_type(const char *buf8k)
{
int tv;
for (tv=0;tv<2;tv++) if (json_contains_data(buf8k, result_type[tv])) return(tv);
m_finish("bad tmno");
return(NOTFND);
}

// Passed 'buf' contains tmdb response to 'basic details' api call citing the IMDB number
// Get TMDB number, and check which of "movie/tv results" exists, setting tmdbTV to 0/1
// Put those 2 fields into a blob and call Add2tbuf to retrieve & add all other fields of interest
// Before return, replace the passed contents of 'buf' with the newly-constructed json doc in the blob.
static void tmdb2omdb(OMZ *oz, char *buf8k)
{
char wrk[128];
JBLOB_READER jb("");								// Create initially empty parser
jb.put("imdbID",strfmt(wrk,"%d",oz->k.imno));

oz->k.tv=get_tmdb_result_type(buf8k);
my_json_get(wrk,buf8k,sizeof(wrk),"id",result_type[oz->k.tv],0);	// get the TMDB id

oz->k.tmno = a2l(wrk,0);
if (oz->k.tmno<1) m_finish("Shite!");
jb.put("tmdbID",wrk);
jb.put("tmdbTV",oz->k.tv?"1":"0");
add2tbuf(&jb, oz);			// UniqCall
strcpy(buf8k,jb.get(NULL));
}

bool tmdb_find_imno(int32_t imno, char *buf8k)
{
char cmd[256];
strfmt(cmd,"curl 'https://api.themoviedb.org/3/find/tt%07d?api_key=%s", imno, tapikey());
strendfmt(cmd,"%s %s","&external_source=imdb_id'", STD_ERRNULL);
int err=exec_cmd(cmd, buf8k, 8192);
return(err==0);
}

void tmdb_all_from_number(OMZ *oz, char *buf8k)
{
tmdb_find_imno(oz->k.imno,buf8k);
tmdb2omdb(oz, buf8k);	// UniqCall RE-fill buf8k with data gotten from tmdb, reformatted to look like omdb
//sjhlog("%s\n",buf8k);
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

bool tmdb_update_rating(bool (*authenticate)(char *session_id),
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
//	sjhlog("Rating %d for TmdbID:%d ImdbID:%d",rating,tmno,imno);	// hkmsrda7
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
