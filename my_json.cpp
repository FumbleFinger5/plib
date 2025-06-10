#include <iostream>
//#include <string>
//#include <curl/curl.h>
//#include <jsoncpp/json/json.h>

#include <string.h>
#include <stdarg.h>

#include "pdef.h"
//#include "flopen.h"
#include "str.h"
#include "log.h"
//#include "memgive.h"
//#include "cal.h"
//#include "exec.h"

#include <json-c/json.h>

bool my_json_empty(const char *jdoc)
{
struct json_object *jobj = json_tokener_parse(jdoc);

struct json_object *response_obj;

    // Check if "Response" key exists and its value is "False"
if (json_object_object_get_ex(jobj, "Response", &response_obj) &&
        json_object_is_type(response_obj, json_type_string) &&
        strcmp(json_object_get_string(response_obj), "False") == 0)
   {
   json_object_put(jobj);  // Free memory
   return true;
   }

bool empty=true;
json_object_object_foreach(jobj, key, val)
    if (val != NULL && !json_object_is_type(val, json_type_null)) {empty=false; break;}
json_object_put(jobj);
return(empty);
}

const char *my_json_get(char *op_buf, const char *ip_buf, int bufsz, const char *fld,...) // fldnam THEN section
{
va_list args;
va_start(args, fld);
const char *str = NULL, *sect = va_arg(args, const char *);
if (sect != nullptr && va_arg(args, const char *) != nullptr)
    m_finish("too many params!");
va_end(args);

*op_buf = 0;
json_object *parser, *jo_fld, *jo_sect;
parser = json_tokener_parse(ip_buf);
if (!parser) sjhlog("\n%s\n",ip_buf);
if (!parser) m_finish("Failed to parse JSON");
if (sect != nullptr && json_object_object_get_ex(parser, sect, &jo_sect))
	{
   if (json_object_get_type(jo_sect) == json_type_array && json_object_array_length(jo_sect) > 0)
		{
      parser = json_object_array_get_idx(jo_sect, 0); // get the first object in the array
      }
	else
		{
		if (json_object_get_type(jo_sect) != json_type_object)
         m_finish("json section '%s' parsing error", sect);
      parser = jo_sect; // use the object directly
      }
	}
if (json_object_object_get_ex(parser, fld, &jo_fld) && (str=json_object_get_string(jo_fld))!=nullptr)
   strancpy(op_buf, str, bufsz);
json_object_put(parser); // Clean up
return(op_buf);
}

bool sect_exists(const char* ip_buf, const char *section_name) {
    json_object *parsed_json, *section;
    bool result = false;

    parsed_json = json_tokener_parse(ip_buf);
    if (!parsed_json) return false; // Failed to parse JSON

    if (json_object_object_get_ex(parsed_json, section_name, &section)) {
        result = json_object_object_length(section) > 0;
    }

    json_object_put(parsed_json); // Clean up
    return result;
}

bool json_contains_data(const char* ip_buf, const char *section_name)
{
struct json_object *parsed_json, *section;
bool result = false;
parsed_json = json_tokener_parse(ip_buf);
if (!parsed_json) return false; // Failed to parse JSON
if (json_object_object_get_ex(parsed_json, section_name, &section))
	{
   enum json_type type = json_object_get_type(section);
   if (type == json_type_array)
		{result = json_object_array_length(section) > 0;}
	else if (type == json_type_object) {result = json_object_object_length(section) > 0;}
        // Add more type checks as needed for other types
   }
json_object_put(parsed_json); // Clean up
return result;
}

/*bool get_numbers_json(const char *buf8k, char *number)    // NOT YET USED: Load all FilmsNN values from smove.json 
{
int ct = 0, n, prv = NOTFND;
json_object *root;
if (buf8k == nullptr || number == nullptr || (root = json_tokener_parse(buf8k)) == nullptr) return false;
json_object *films_obj = root; // Corrected line
if (films_obj != nullptr && json_object_get_type(films_obj) == json_type_object)
    json_object_object_foreach(films_obj, key, val)
        {
        if (strncmp(key, "Films", 5) != 0 || strlen(key) != 7) {ct=0; break;}
        n = a2i(&key[5], 2); // convert 2 bytes from c-str number to int (global a2err_char = first non-digit found)
        if (n < 1 || n > 99 || n <= prv || a2err_char != 0) {ct=0; break;} // (a2err_char should be an EOS byte)
        number[ct++] = prv = n;
        }
number[ct] = 0;
json_object_put(root);
return (ct > 0);
}*/
