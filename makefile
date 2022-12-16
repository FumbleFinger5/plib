TARGET = libplib.a
#COMP = gcc
COMP = clang++
#BUG = -ggdb
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
        smdb.o xcom1.o \
        csv.o log.o omdb.o exec.o
	ar rcs $@ $^

%.o: %.cpp %.h
	$(COMP) $(CFLAGS) $(BUG) -c -o $@ $<  

clean:
	rm -f *.o *.a $(TARGET)

