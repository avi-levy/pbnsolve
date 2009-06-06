
# Settings on Jan's OpenSUSE 10.2 system:
LIB=-lxml2 -lm
CFLAGS= -g

# Settings on Pair.com
# LIB=-lxml2 -lm -L/usr/local/lib
# CFLAGS= -g -I/usr/local/include/libxml2 -I/usr/local/include

OBJ= pbnsolve.o read.o read_xml.o dump.o puzz.o grid.o line_lro.o job.o \
	solve.o gamma.o http.o clue.o

pbnsolve: $(OBJ)
	cc -o pbnsolve $(CFLAGS) $(OBJ) $(LIB)

pbnsolve.o: pbnsolve.c pbnsolve.h read.h bitstring.h config.h
dump.o: dump.c pbnsolve.h bitstring.h config.h
grid.o: grid.c pbnsolve.h bitstring.h config.h
puzz.o: puzz.c pbnsolve.h bitstring.h config.h
line_lro.o: line_lro.c pbnsolve.h bitstring.h config.h
job.o: job.c pbnsolve.h bitstring.h config.h
solve.o: solve.c pbnsolve.h bitstring.h config.h
read.o: read.c pbnsolve.h read.h bitstring.h config.h
clue.o: clue.c pbnsolve.h bitstring.h config.h
gamma.o: gamma.c config.h
http.o: http.c pbnsolve.h config.h
read_xml.o: read_xml.c pbnsolve.h read.h bitstring.h config.h

testgamma: testgamma.c gamma.o
	cc -o testgamma $(CFLAGS) testgamma.c gamma.o -lm

TARBALL= README pbnsolve.html pbn_fmt.html Makefile \
	bitstring.h config.h pbnsolve.h read.h \
	pbnsolve.c puzz.c read.c read_xml.c solve.c testgamma.c \
	clue.c dump.c gamma.c grid.c http.c job.c line_lro.c

pbnsolve.tgz: $(TARBALL)
	tar cvzf pbnsolve.tgz $(TARBALL)

clean:
	rm -f $(OBJ)
