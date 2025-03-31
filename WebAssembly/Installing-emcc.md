# Install emcc

```
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source "emsdk_env.sh"
```
Configure emsdk in your shell startup scripts by running:
```
echo 'source "$HOME/emsdk/emsdk_env.sh"' >> $HOME/.bash_profile
```
(Adapt the path $HOME/emsdk/emsdk_env.sh if needed.)
