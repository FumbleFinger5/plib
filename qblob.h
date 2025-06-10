#ifndef QBLOB_H
#define QBLOB_H

#include <json-c/json.h>


//void tmdb2omdb(const char *buf, char *buf8k);

class JSS4 {	// manage a pool of 4 ALLOCATED string buffers
public:			// ONE internal 8192-byte 'blob' buffer & UP TO 4 dynamically-allocated returned strings  
JSS4();
~JSS4();
const char *get(json_object *parser, const char *name);	/// Return ptr->value of item ("Title", "Type",...)
private:
char  *aptr[4];                  // 4 ALOCATED ptrs so return values aren't immediately overwritten
int   n, alen[4];
};

class JBLOB_READER {	// Manage named strings from api-filled 'blob' (repeated "keyname"="keyvalue" elements)
   // destructor frees constructor param 'buf'
public:			// ONE internal 8192-byte 'blob' buffer & UP TO 4 dynamically-allocated returned strings  
JBLOB_READER(const char *blob);
~JBLOB_READER();
const char *get(const char *id);	/// Return ptr->value of item ("Title", "Type",...  OR whole buffer if id==NULL)
DYNAG *get(void);           /// Return ptr to newly-created DYNAG with all key names 
void put(const char *name, const char *value);
void del(const char *name);
private:
json_object *parser;
char *secret_buf=NULL;
JSS4 *jss4;
};

//char *jstr4(json_object *parser, const char *id);
char *jarray2list(char *str);

#endif // QBLOB_H
