#include <stdarg.h>
#include <cstring>
#include <json-c/json.h>
#include <cstdint>  // C++

#include "pdef.h"
#include "memgive.h"
#include "str.h"
#include "log.h"
#include "qblob.h"
#include "my_json.h"

/// IF i/p is a json array of comma-separated quotated strings,
/// strip json-array-enclosing square brackets and quote marks
/// WON'T WORK IF escaped quotes (backslash+") OR commas in strings
char *jarray2list(char *str)
{
int i,j,ln=strlen(str);
if (SAME3BYTES(str,"[ \"") && ln>4 && str[ln-1]==']')
    {
    bool qt=false;
    char c;
    for (i=j=0; (c=str[i])!=0; i++)
        {
        if (c==QTDOUBLE) qt=!qt;
        else if (qt || (!qt && c==COMMA)) str[j++]=c;     
        }
    str[j]=0;
   }
return(str);
}

JSS4::JSS4()
{for (n=0;n<4;n++) aptr[n]=(char*)memgive(alen[n]=1);}
JSS4::~JSS4()
{for (n=0;n<4;n++) memtake(aptr[n]);}
const char* JSS4::get(json_object *parser, const char *name)
{
n=(n+1)&3;
json_object *jo_fld;
const char *str;
if (!json_object_object_get_ex(parser, name, &jo_fld) || (str=json_object_get_string(jo_fld))==NULL)
   str="";
int len=strlen(str);
if ((len+1)>alen[n]) aptr[n]=(char*)memrealloc(aptr[n],alen[n]=(len+1));
if (len) memmove(aptr[n],str,len);
aptr[n][len]=0;
return(jarray2list(aptr[n]));
}
   
JBLOB_READER::JBLOB_READER(const char *blob)
{
if (!*blob) blob="{}";
parser = json_tokener_parse(blob);
if (!parser) m_finish("Failed to parse Json");
jss4=new JSS4;
}

JBLOB_READER::~JBLOB_READER()
{
delete jss4;
memtake(secret_buf);
json_object_put(parser);
}

const char* JBLOB_READER::get(const char *id)	// Get the named item ("Title", "Type", etc) OR EMPTY STRING
{
if (id==NULL)   // If no specific object name passed, return the whole json object AS A NULL-TERMINATED STRING
    {
    const char *p = json_object_to_json_string(parser);
    Scrap(secret_buf);
    return(secret_buf=stradup(p));
    }
return(jss4->get(parser, id));
}

DYNAG* JBLOB_READER::get(void)           /// Populate passed DYNAG with all key names 
{
DYNAG *keys=new DYNAG(0);
// macro implicitly declares  char *key; json_object *val;   (we're not using "val" here)
json_object_object_foreach(parser, key, val)
   {
   keys->put(key);
   }
return(keys);
}

void JBLOB_READER::del(const char *name)
{
json_object_object_del(parser,name);
}

void JBLOB_READER::put(const char *name, const char *value) // Add or Update 'name' with 'value' (DEL if name==NULL)
{
json_object* jo_str = json_object_new_string(value);
json_object_object_add(parser, name, jo_str);
}

