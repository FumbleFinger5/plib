TARGET = libplib.a
#COMP = g++
COMP = clang
#CFLAGS = -I../mylib -ggdb
CFLAGS = -I../mylib -ggdb  -fPIE 

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
        csv.o log.o
	ar rcs $@ $^

%.o: %.cpp %.h
	$(COMP) $(CFLAGS) -c -o $@ $<  

clean:
	rm -f *.o *.a $(TARGET)

