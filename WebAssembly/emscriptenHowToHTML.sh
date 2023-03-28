#emcc (Emscripten gcc/clang-like replacement + linker emulating GNU ld) 3.1.9
emcc ../potu/*.c --pre-js pre.js -s EXPORTED_RUNTIME_METHODS='["cwrap", "allocateUTF8", "intArrayFromString"]' -s "EXPORTED_FUNCTIONS=['_main','_oneShot','_malloc']" -s "MODULARIZE=0"  -s "WASM=0" -O0 -o potuC.html
bracmat "get'\"edit-potuC.html.bra\""
