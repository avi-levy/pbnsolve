
# Settings on Jan's OpenSUSE 10.2 system:
LIB=-lxml2 -lm
CFLAGS= -O2
#CFLAGS= -g

# Settings on Pair.com
# LIB=-lxml2 -lm -L/usr/local/lib
# CFLAGS= -O2 -I/usr/local/include/libxml2 -I/usr/local/include

# Settings on dreamhost - I recommend -O2 on production installations
#LIB=-lxml2 -lm
#CFLAGS= -O2 -I/usr/include/libxml2

OBJ= pbnsolve.o read.o read_xml.o read_bw.o read_grid.o dump.o puzz.o grid.o \
	line_lro.o job.o solve.o probe.o contradict.o gamma.o http.o clue.o \
	merge.o exhaust.o bit.o read_olsak.o line_cache.o score.o

pbnsolve: $(OBJ)
	cc -o pbnsolve $(CFLAGS) $(OBJ) $(LIB)

pbnsolve.o: pbnsolve.c pbnsolve.h read.h bitstring.h config.h
dump.o: dump.c pbnsolve.h bitstring.h config.h
grid.o: grid.c pbnsolve.h bitstring.h config.h
puzz.o: puzz.c pbnsolve.h bitstring.h config.h
line_lro.o: line_lro.c pbnsolve.h bitstring.h config.h
line_cache.o: line_cache.c pbnsolve.h bitstring.h config.h
job.o: job.c pbnsolve.h bitstring.h config.h
solve.o: solve.c pbnsolve.h bitstring.h config.h
score.o: score.c pbnsolve.h bitstring.h config.h
probe.o: probe.c pbnsolve.h bitstring.h config.h
contradict.o: contradict.c pbnsolve.h bitstring.h config.h
exhaust.o: exhaust.c pbnsolve.h bitstring.h config.h
clue.o: clue.c pbnsolve.h bitstring.h config.h
merge.o: merge.c bitstring.h pbnsolve.h config.h
bit.o: bit.c bitstring.h config.h
gamma.o: gamma.c config.h
http.o: http.c pbnsolve.h config.h
read.o: read.c pbnsolve.h read.h bitstring.h config.h
read_xml.o: read_xml.c pbnsolve.h read.h bitstring.h config.h
read_bw.o: read_bw.c pbnsolve.h read.h bitstring.h config.h
read_grid.o: read_grid.c pbnsolve.h read.h bitstring.h config.h
read_olsak.o: read_olsak.c pbnsolve.h read.h bitstring.h config.h

testgamma: testgamma.c gamma.o
	cc -o testgamma $(CFLAGS) testgamma.c gamma.o -lm

testline: testline.c line_lro.o read.o dump.o grid.o merge.o job.o read_xml.o \
	puzz.o clue.o line_cache.o read_bw.o read_grid.o read_olsak.o
	cc -o testline $(CFLAGS) testline.c line_lro.o read.o dump.o grid.o \
	merge.o job.o read_xml.o puzz.o clue.o line_cache.o read_bw.o \
	read_grid.o read_olsak.o $(LIB)

TARBALL= README CHANGELOG Makefile \
	bitstring.h config.h pbnsolve.h read.h read_bw.c read_grid.c \
	pbnsolve.c puzz.c read.c read_xml.c solve.c testgamma.c \
	clue.c dump.c gamma.c grid.c http.c job.c line_lro.c merge.c \
	exhaust.c testline.c probe.c contradict.c bit.c read_olsak.c \
	line_cache.c score.c

pbnsolve.tgz: $(TARBALL)
	tar cvzf pbnsolve.tgz $(TARBALL)

clean:
	rm -f $(OBJ)
