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
echo 'source "/home/emsdk/emsdk_env.sh"' >> $HOME/.bash_profile
```
(Adapt the path /home/emsdk/emsdk_env.sh if needed.)
