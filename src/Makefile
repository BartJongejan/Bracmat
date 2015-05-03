bracmat: bracmat.c xml.c json.c
	gcc -std=c99 -pedantic -Wall -O2 -static -DNDEBUG -o bracmat bracmat.c xml.c json.c

bracmatsafe: bracmat.c xml.c json.c
	gcc -std=c99 -pedantic -Wall -O2 -static -DNDEBUG -DNO_C_INTERFACE -DNO_FILE_RENAME -DNO_FILE_REMOVE -DNO_SYSTEM_CALL -DNO_LOW_LEVEL_FILE_HANDLING -o bracmatsafe bracmat.c xml.c json.c

profiling:
	gcc -Wall -c -pg -DNDEBUG bracmat.c xml.c json.c
	gcc -Wall -pg bracmat.o xml.o json.o
	cp ../valid.bra .
	cp ../pr-xml-utf-8.xml .
	./a.out "get'\"valid.bra\";!r" 
	gprof a.out > bracmat.prof
	rm a.out

coverage:
	gcc -fprofile-arcs -ftest-coverage -DNDEBUG bracmat.c xml.c json.c
	cp ../valid.bra .
	cp ../pr-xml-utf-8.xml .
	./a.out "get'\"../valid.bra\";!r"
	gcov bracmat.c
	gcov xml.c
	gcov json.c
	rm a.out

all: bracmat bracmatsafe profiling coverage

clean:
	rm bracmat
	rm bracmatsafe
