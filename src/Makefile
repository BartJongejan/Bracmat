EXECUTABLE = bracmat

SRC = binding.c \
      branch.c \
      builtinmethod.c \
      canonization.c \
      calculation.c \
      charput.c \
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
      result.c \
      simil.c \
      stringmatch.c \
      treematch.c \
      unicaseconv.c \
      unichartypes.c \
      variables.c \
      wipecopy.c \
      writeerr.c \
      xml.c

potu: $(SRC) 
	gcc -std=c99 -pedantic -Wall -O2 -static -DNDEBUG -o $(EXECUTABLE) $(SRC) -lm


potu1: potu.c 
	gcc -std=c99 -pedantic -Wall -O2 -static -DNDEBUG -DSINGLESOURCE -o $(EXECUTABLE)1 potu.c -lm

potusafe: $(SRC)
	gcc -std=c99 -pedantic -Wall -O2 -static -DNDEBUG -DNO_C_INTERFACE -DNO_FILE_RENAME -DNO_FILE_REMOVE -DNO_SYSTEM_CALL -DNO_LOW_LEVEL_FILE_HANDLING -o $(EXECUTABLE)safe $(SRC) -lm

potu1safe: potu.c
	gcc -std=c99 -pedantic -Wall -O2 -static -DNDEBUG -DNO_C_INTERFACE -DNO_FILE_RENAME -DNO_FILE_REMOVE -DNO_SYSTEM_CALL -DNO_LOW_LEVEL_FILE_HANDLING -DSINGLESOURCE -o $(EXECUTABLE)1safe potu.c -lm

profiling:
	gcc -std=c99 -pedantic -Wall -O2 -static -c -pg -DNDEBUG -DSINGLESOURCE potu.c -lm
	gcc -std=c99 -pedantic -Wall -O2 -static -pg potu.o -lm
	cp ../valid.bra .
	cp ../pr-xml-utf-8.xml .
	./a.out "get'\"valid.bra\";!r" 
	gprof a.out > $(EXECUTABLE)1.prof
	rm a.out

coverage:
	gcc -fprofile-arcs -ftest-coverage -DNDEBUG -DSINGLESOURCE potu.c -lm
	cp ../valid.bra .
	cp ../pr-xml-utf-8.xml .
	./a.out "get'\"../valid.bra\";!r"
	gcov potu.c
	rm a.out

all: potu potu1 potusafe potu1safe profiling coverage

clean:
	rm $(EXECUTABLE)
	rm $(EXECUTABLE)1
	rm $(EXECUTABLE)safe
	rm $(EXECUTABLE)1safe
	rm *.gcov
	rm *.o
