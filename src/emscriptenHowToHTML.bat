REM ..\..\emscripten\emcc json.c xml.c bracmat.c -o bracmat.html -s EXPORTED_FUNCTIONS="['_startProc','_Eval','_endProc','_oneShot']" -s IGNORED_FUNCTIONS="['_myputc']"
ECHO starting script to generate bracmat as HTML page.
call ..\..\emscripten\emcc json.c xml.c bracmat.c -o bracmatJS.html -s EXPORTED_FUNCTIONS="['_startProc','_Eval','_endProc','_oneShot']"
ECHO now bracmat
REM edit the next line so a natively compiled version of bracmat on your system is found.
D:\projects\Bracmat\bin\bracmat.exe "get'\"editbracmatjs.bra\""
ECHO bracmat done
