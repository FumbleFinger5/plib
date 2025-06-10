#ifndef PARM_H
#define PARM_H

#include "memgive.h"

class PARM {
public:
PARM(void);
~PARM();
const char    *get(const char *name);
DYNAG *allKeys(void) {return(nam);};
void    set(const char *name, const char *value);
private:
char    *path;
DYNAG   *nam, *val;
int     upd=NO;
};

class RECENT
{
public:
RECENT();
~RECENT() {delete tbl;};
int32_t top(void) {return(tbl->ct ? *(int32_t*)tbl->get(0) : 0);};
void put(int32_t imno);
int pos(int32_t imno);
private:
char fn[256];
DYNAG *tbl;
};




#include <gtk/gtk.h>
#include <gdk/gdk.h>

struct GW_GEOMETRY {int x, y, w, h, m;};

class GET_SET_GEOMETRY
{
public:
GET_SET_GEOMETRY(const char *app_name, GtkWidget *window);
void save(void);
private:
const char *appName;
GtkWindow *gtkWindow;
GW_GEOMETRY geom;
};
   
      
char *parm_str(const char *setting, char *buf, const char *deflt);   // buf must be >= 128 bytes

const char *apikey();
extern int tapikey_throttle_force;
const char *tapikey(int throttle=NO);
char *get_conf_path(char *fn, const char *base_filename);

#endif // PARM_H
