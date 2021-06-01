# XRFC
XRFC is a work-in-progress llvm-based compiler for [XRF](https://esolangs.org/wiki/XRF).
XRF is an esoteric programming language based on manipulating a stack/queue
(a quack?), and having to deal with the illogical flow control, which can jump
to any part of the program at any time, dependent on the top value of the
stack every few instructions that are executed.

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
* Add more tests, especially for the optimization/generation code.
* Make it so the compiler can optionally emit checks for undefined behavior.
  For instance, trying to pop the stack when it's empty, trying to jump to a
  code chunk that doesn't exist, checking for stack overflow.
* Implement compiler warnings where possible. For instance, if the optimizer
  can tell that a chunk will always try to jump to a nonexistent chunk.
