emcc json.c xml.c bracmat.c -o bracmatJS.html -s EXPORTED_FUNCTIONS="['_startProc','_eval','_endProc','_oneShot']"
bracmat "get'\"editbracmatjs.bra\""
