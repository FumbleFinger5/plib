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

