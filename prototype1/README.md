First prototype

WASM demo: <https://rsms.me/etc/xsys-demo/>

Build:

```sh
./build.sh -watch  # skip -watch for a simple build
```

Result is in `./out/`

Your llvm installation needs do have the WASM backend enabled.
If you're using llvm from Xcode you probably don't have it and
need to install a vanilla version of LLVM and add it to PATH:

```sh
PATH=/path/to/llvm-with-wasm-enabled/bin:$PATH ./build.sh -watch
```
