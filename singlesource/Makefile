bracmat: bracmat.c 
	gcc -std=c99 -pedantic -Wall -O2 -DNDEBUG -o bracmat bracmat.c -lm

bracmat.c: ../src/bracmat
	../src/bracmat "get'\"one.bra\""

../src/bracmat:
	cd ../src && make && cd ../singlesource

bracmatsafe: bracmat.c
	gcc -std=c99 -pedantic -Wall -O2 -static -DNDEBUG -DNO_C_INTERFACE -DNO_FILE_RENAME -DNO_FILE_REMOVE -DNO_SYSTEM_CALL -DNO_LOW_LEVEL_FILE_HANDLING -o bracmatsafe bracmat.c -lm

profiling:
	gcc -Wall -c -pg -DNDEBUG bracmat.c -lm
	gcc -Wall -pg bracmat.o -lm
	cp ../valid.bra .
	cp ../pr-xml-utf-8.xml .
	./a.out "get'\"valid.bra\";!r" 
	gprof a.out > bracmat.prof
	rm a.out

coverage:
	gcc -fprofile-arcs -ftest-coverage -DNDEBUG bracmat.c -lm
	cp ../valid.bra .
	cp ../pr-xml-utf-8.xml .
	./a.out "get'\"../valid.bra\";!r"
	gcov bracmat.c
	rm a.out

all: bracmat bracmatsafe profiling coverage

clean:
	rm bracmat.c
	rm bracmat
	rm bracmatsafe
