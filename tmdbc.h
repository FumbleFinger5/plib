//#ifndef IMDB_H
//#define IMDB_H

#define SESSIONFIL  "/home/steve/tmdb_session_id"
#define SESSIONSIZ  40

int rating2tmdb(int rating);

bool get_session(char *session_id);

extern const char *result_type[];       // Initially, 2 "real" MediaResultTypes, plus a nullptr
extern const char *mtyp[];              // MediaType - "movie" or "tv"

bool tmdb_find_imno(int32_t imno, char *buf8k);       // used by both series and usherette
void tmdb_all_from_number(OMZ *oz, char *buf8k);
int get_tmdb_result_type(const char *buf8k);

bool uxlt(char *str);	// Normalise passed string e.g. u"Noël Coward"   to "Noel Coward"  IN SITU

bool tmdb_update_rating(bool (*authenticate)(char *session_id), int32_t tmno, int32_t imno, int tv, int season, int episode, int my_rating);

bool  tmdb_api(const char *cmd, char *buf=0, int bufsz=0);
int   tmdb_get_rating(bool (*authenticate)(char *session_id),int32_t tmno, int32_t imno, int tv, int sea, int epi);


bool session_id_works(char *session_id);
bool get_request_token_url(char *url, char *request_token);
bool authenticate_request_token(char *request_token, char *session_id);
bool save_new_session(char *session_id);
bool session_id_read(char *session_id);


//#endif
