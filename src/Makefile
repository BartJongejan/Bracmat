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

CC = gcc 
CFLAGS = -std=c99 -pedantic -Wall -O2 
UNAME_S := $(shell uname -s) 
ifeq ($(UNAME_S),Linux)
    STATIC = -static 
else 
#    ifeq ($(UNAME_S),Darwin) 
    STATIC =
#    endif 
endif

# To use GNU readline() on Linux, you need to install the readline-dev library
# sudo apt-get install libreadline-dev
# sudo yum install readline-devel
potu: $(SRC) 
	$(CC) $(CFLAGS)           -DNDEBUG -o $(EXECUTABLE) $(SRC) -lm -DREADLINE -lreadline
#	gcc -std=c99 -pedantic -Wall -O2 $(STATIC) -DNDEBUG -o $(EXECUTABLE) $(SRC) -lm

potu1: potu.c 
	$(CC) $(CFLAGS) $(STATIC) -DNDEBUG -DSINGLESOURCE -o $(EXECUTABLE)1 potu.c -lm

potusafe: $(SRC)
	$(CC) $(CFLAGS) $(STATIC) -DNDEBUG -DNO_C_INTERFACE -DNO_FILE_RENAME -DNO_FILE_REMOVE -DNO_SYSTEM_CALL -DNO_LOW_LEVEL_FILE_HANDLING -o $(EXECUTABLE)safe $(SRC) -lm

potu1safe: potu.c
	$(CC) $(CFLAGS) $(STATIC) -DNDEBUG -DNO_C_INTERFACE -DNO_FILE_RENAME -DNO_FILE_REMOVE -DNO_SYSTEM_CALL -DNO_LOW_LEVEL_FILE_HANDLING -DSINGLESOURCE -o $(EXECUTABLE)1safe potu.c -lm

profiling:
	$(CC) $(CFLAGS) $(STATIC) -c -pg -DNDEBUG -DSINGLESOURCE potu.c -lm
	$(CC) $(CFLAGS) $(STATIC) -pg potu.o -lm
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
	gcov a-potu.c
	rm a.out

clean:
	rm -f $(EXECUTABLE)
	rm -f $(EXECUTABLE)1
	rm -f $(EXECUTABLE)safe
	rm -f $(EXECUTABLE)1safe
	rm -f *.gcov
	rm -f *.gcda
	rm -f *.gcno
	rm -f *.o

all: clean potu potu1 potusafe potu1safe profiling coverage
