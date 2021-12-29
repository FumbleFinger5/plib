class PARM {
public:
PARM(const char *pth);		// Path to *.cfg file
~PARM();
const char    *get(const char *name);
void    set(const char *name, const char *value);
private:
char    *path;
DYNAG   *nam, *val;
int     upd=NO;
};
