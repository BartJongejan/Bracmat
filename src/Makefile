bracmat: bracmat.c xml.c
	gcc -std=c99 -pedantic -Wall -O3 -static -DNDEBUG -o bracmat bracmat.c xml.c

bracmatsafe: bracmat.c xml.c
	gcc -std=c99 -pedantic -Wall -O3 -static -DNDEBUG -DNO_C_INTERFACE -DNO_FILE_RENAME -DNO_FILE_REMOVE -DNO_SYSTEM_CALL -DNO_LOW_LEVEL_FILE_HANDLING -o bracmatsafe bracmat.c xml.c

all: bracmat bracmatsafe

clean:
	rm bracmat
	rm bracmatsafe
