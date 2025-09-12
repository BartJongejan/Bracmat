#emcc (Emscripten gcc/clang-like replacement + linker emulating GNU ld) 3.1.9
# Without WebAssembly:
##emcc ../src/*.c --pre-js pre.js -s EXPORTED_RUNTIME_METHODS='["cwrap", "intArrayFromString"]' -s "EXPORTED_FUNCTIONS=['_main','_oneShot','_malloc']" -s "MODULARIZE=0" -O0 -o potuC.html -s "WASM=0"
# emcc ../src/*.c --pre-js pre.js -s EXPORTED_RUNTIME_METHODS='["stringToNewUTF8", "cwrap", "intArrayFromString","stackAlloc"]' -s "EXPORTED_FUNCTIONS=['_main','_oneShot','_malloc']" -s "MODULARIZE=0" -O0 -o potuC.html -s "WASM=0"
# With WebAssembly
# Note that you probably need to run from a web server. You can do `python3 -m http.server' and then load http://localhost:8000/bracmatJS.html in your browser.
# See https://emscripten.org/docs/getting_started/FAQ.html#how-do-i-run-a-local-webserver-for-testing-why-does-my-program-stall-in-downloading-or-preparing
emcc ../src/*.c --pre-js pre.js -s EXPORTED_RUNTIME_METHODS='["stringToNewUTF8", "cwrap", "intArrayFromString","stackAlloc"]' -s "EXPORTED_FUNCTIONS=['_main','_oneShot','_malloc']" -s "MODULARIZE=0" -O0 -o potuC.html
bracmat "get'\"edit-potuC.html.bra\""
