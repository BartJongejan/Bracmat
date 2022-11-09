SRC = binding.c \
      branch.c \
      builtinmethod.c \
      canonization.c \
      calculation.c \
      copy.c \
      encoding.c \
      equal.c \
      evaluate.c \
      filestatus.c \
      filewrite.c \
      functions.c \
      globals.c \
      hash.c \
      head.c \
      input.c \
      json.c \
      lambda.c \
      macro.c \
      memory.c \
      nodeutil.c \
      numbercheck.c \
      object.c \
      objectdef.c \
      opt.c \
      position.c \
      potu.c \
      quote.c \
      rational.c \
      real.c \
      result.c \
      simil.c \
      stringmatch.c \
      treematch.c \
      u2l-l2u.c \
      variables.c \
      wipecopy.c \
      writeerr.c \
      xml.c

potu: $(SRC) 
	gcc -std=c99 -pedantic -Wall -O2 -static -DNDEBUG -o potu $(SRC) -lm


potu1: potu.c 
	gcc -std=c99 -pedantic -Wall -O2 -static -DNDEBUG -DSINGLEOBJECT -o potu1 potu.c -lm

potusafe: $(SRC)
	gcc -std=c99 -pedantic -Wall -O2 -static -DNDEBUG -DNO_C_INTERFACE -DNO_FILE_RENAME -DNO_FILE_REMOVE -DNO_SYSTEM_CALL -DNO_LOW_LEVEL_FILE_HANDLING -o potusafe $(SRC) -lm

potu1safe: potu.c
	gcc -std=c99 -pedantic -Wall -O2 -static -DNDEBUG -DNO_C_INTERFACE -DNO_FILE_RENAME -DNO_FILE_REMOVE -DNO_SYSTEM_CALL -DNO_LOW_LEVEL_FILE_HANDLING -DSINGLEOBJECT -o potu1safe potu.c -lm

profiling:
	gcc -Wall -c -pg -DNDEBUG -DSINGLEOBJECT potu.c -lm
	gcc -Wall -pg potu.o
	cp ../valid.bra .
	cp ../pr-xml-utf-8.xml .
	./a.out "get'\"valid.bra\";!r" 
	gprof a.out > potu1.prof
	rm a.out

coverage:
	gcc -fprofile-arcs -ftest-coverage -DNDEBUG -DSINGLEOBJECT potu.c -lm
	cp ../valid.bra .
	cp ../pr-xml-utf-8.xml .
	./a.out "get'\"../valid.bra\";!r"
	gcov potu.c
	rm a.out

all: potu potu1 potusafe potu1safe profiling coverage

clean:
	rm potu
	rm potu1
	rm potusafe
	rm potu1safe
	rm *.gcov
	rm *.o
