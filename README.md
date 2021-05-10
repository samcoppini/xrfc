# XRFC
XRFC is a work-in-progress llvm-based compiler for [XRF](https://esolangs.org/wiki/XRF).

## How to use
In order to build you will need LLVM >= 10, a C++17 compiler, Meson, and Ninja.

To build, simply do:
```sh
$ meson build --prefix=$(pwd)/dist
$ ninja -C build install
```

This will install the `xrfc` to the `dist/bin` folder. It can then be called
to generate LLVM IR files, which can be compiled into executables using the
LLVM toolchain like so:

```sh
$ ./dist/bin/xrfc ./examples/hello.xrf -o hello.ll
$ llc hello.ll -o hello.s
$ clang hello.s -o hello.exe
$ ./hello.exe
Hello, World!
```

## Todo:
* Implement `D` instruction to randomize the stack.
* Better optimization of generated LLVM code.
* Add more tests, especially for the optimization/generation code.
