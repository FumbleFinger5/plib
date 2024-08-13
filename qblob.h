#include <json-c/json.h>


//void tmdb2omdb(const char *buf, char *buf8k);

class JBLOB_READER {	// Manage named strings from api-filled 'blob' (repeated "keyname"="keyvalue" elements)
                // destructor frees constructor param 'buf'
public:			// ONE internal 8192-byte 'blob' buffer & UP TO 4 dynamically-allocated returned strings  
JBLOB_READER(const char *blob);
~JBLOB_READER();
char *get(const char *id);				// Get the named item ("Title", "Type", etc)
void put(const char *name, const char *value);
void del(const char *name);
private:
json_object *parser;
char *secret_buf=NULL;
};

char *jstr4(json_object *parser, const char *id);
char *jarray2list(char *str);
