TARGET = libplib.a
COMP = g++
#COMP = clang++

# Use  make r=1  for release version
ifdef r
BUG =
else
BUG = -ggdb -DBUG=YES
endif

CFLAGS = -I../mylib  -isystem /usr/include/x86_64-linux-gnu/qt5 \
   -isystem /usr/include/x86_64-linux-gnu/qt5/QtCore \
   -isystem /usr/include/x86_64-linux-gnu/qt5/QStandardItemModel \
   -isystem /usr/include/x86_64-linux-gnu/qt5/QSettings \
   -isystem  /usr/include/x86_64-linux-gnu/qt5/QtWidgets  -fPIC 

# target: prerequisites - the rule head
# $@  means the target
# $^  means all prerequisites
# $<  means just the first prerequisite
# ar  Linux archive tool, with options...
#     r - replace files existing inside the archive
#     c - create a archive if not already existent
#     s - create an object-file index into the archive


$(TARGET): str.o memgive.o \
        cal.o flopen.o \
        parm.o dirscan.o \
        drinfo.o db.o \
        csv.o my_json.o qblob.o tmdbc.o \
        smdb.o imdb.o imdbf.o xcom1.o \
        log.o omdb1.o exec.o scan.o serdbf.o
	@ar rcs $@ $^
	@echo $@ linked OK

%.o: %.cpp %.h
	@$(COMP) $(CFLAGS)  $(BUG) -c -o $@ $<  
	@echo $@

clean:
	rm -f *.o *.a $(TARGET)

