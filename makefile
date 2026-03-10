TARGET = libplib.a
COMP = g++

# Use  make r=1  for release version
ifdef r
BUG =
else
BUG = -ggdb -DBUG=YES
endif

CFLAGS = `pkg-config --cflags gtk+-3.0 Qt5Core Qt5Widgets` -I../mylib -fPIC


$(TARGET): str.o memgive.o cal.o flopen.o \
	parm.o dirscan.o drinfo.o db.o \
	csv.o my_json.o qblob.o  tmdbc.o \
	imdb.o imdbf.o xcom1.o \
	log.o omdb1.o exec.o scan.o \
	mvdb.o tvdb.o mvhlc.o
	@ar rcs $@ $^
	@echo $@ linked OK

%.o: %.cpp %.h
	@$(COMP) $(CFLAGS)  $(BUG) -c -o $@ $<  
	@echo $@

clean:
	rm -f *.o *.a $(TARGET)

