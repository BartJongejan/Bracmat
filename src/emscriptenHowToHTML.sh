#emcc (Emscripten gcc/clang-like replacement) 1.38.0
emcc json.c xml.c bracmat.c -o bracmatJS.html -s EXPORTED_FUNCTIONS='["_main","_oneShot"]'
bracmat "get'\"editbracmatjs.bra\""
