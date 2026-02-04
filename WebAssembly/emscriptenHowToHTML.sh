# Set WASM=0 or WASM=1 in this script. Then execute it. 

# Only JavaScript, without WebAssembly:
WASM=0
# Output: bracmatJS.html
#    This file works fine by double clicking it in the file manager.

# With WebAssembly
#WASM=1
# Output: bracmatWASM.html and potuC.wasm
#   These two files must be in the same directory under the webserver's document root.
#   Note that you cannot activate Web Assembly if you just double click bracmatWASM.html in the file manager.
#   You can do `python3 -m http.server' and then load http://localhost:8000/bracmatWASM.html in your browser.
#   See https://emscripten.org/docs/getting_started/FAQ.html#how-do-i-run-a-local-webserver-for-testing-why-does-my-program-stall-in-downloading-or-preparing

# emcc -v :
# emcc (Emscripten gcc/clang-like replacement + linker emulating GNU ld) 4.0.4 (273f0216fede04f2445367765eaf2aabeeb60d84)
# clang version 21.0.0git (https:/github.com/llvm/llvm-project 148111fdcf0e807fe74274b18fcf65c4cff45d63)

emcc ../src/*.c --pre-js pre.js -s EXPORTED_RUNTIME_METHODS='["stringToNewUTF8", "cwrap", "intArrayFromString","stackAlloc"]' -s "EXPORTED_FUNCTIONS=['_main','_oneShot','_malloc']" -s "MODULARIZE=0" -O0 -o potuC.html -s "WASM=$WASM"
bracmat "get'\"edit-potuC.html.bra\"" "$WASM"

# last updated: 20260204