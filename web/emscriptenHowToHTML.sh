#emcc (Emscripten gcc/clang-like replacement) 1.38.0
emcc ../src/json.c ../src/xml.c ../src/bracmat.c -o bracmatJS.html -s EXPORTED_FUNCTIONS='["_main","_oneShot"]'
bracmat "get'\"editbracmatjs.bra\""
