# wasm-rtos-examples

**Small, complete programs that show how the parts of wasm-rtos fit
together**, from the interpreter alone up to the operating system with
devices. Each example is one host (`main.c`, for your computer) and the
WebAssembly program it runs (`program.c`, built as `program.wasm` beside
it), and each runs in CI, so what you read here works.

```
 programs (.wasm)        any language that compiles to WebAssembly
 ─────────────────────────────────────────────────────────────────────
 wasm-rtos               tasks, priorities, queues, timers, files, sleep
 (the kernel)            ── optional: a host of your own can stand here
 ─────────────────────────────────────────────────────────────────────
 wasm-rtos-hw            pins, buses, displays, serial ports … by name,
 (the devices)           one owner at a time        ── optional
 ─────────────────────────────────────────────────────────────────────
 wasm-rtos-vm            loads and runs WebAssembly: fuel, stops,
 (the interpreter)       snapshots, paged memory, WASI without files
 ─────────────────────────────────────────────────────────────────────
 your board              a clock, an output, the devices' registers
```

Each layer only needs the one below it. Take the interpreter alone for a
scripting engine, add the devices and a simple host for a mini OS on a
small board, or take all three.

## The examples

| Example | Layers | What it shows |
|---|---|---|
| [`vm/hello`](vm/hello) | vm | Load a module, call what it exports: four calls |
| [`vm/host-call`](vm/host-call) | vm | A program calls functions of the host: how anything reaches the outside |
| [`vm/slices`](vm/slices) | vm | Two programs at once with no operating system: fuel and `wm_yield` make a scheduler of a few lines |
| [`vm/wasi`](vm/wasi) | vm | An ordinary C program with `printf`, built for WASI, run by the interpreter alone |
| [`mini-os`](mini-os) | vm + hw | A mini OS of the interpreter, the devices and the simple host: a program blinks a pin and waits for a button; also built for an AVR |
| [`rtos/tasks`](rtos/tasks) | vm + hw + kernel | Two tasks of one program fed through a queue, their output marked with who wrote it |
| [`rtos/devices`](rtos/devices) | vm + hw + kernel | A board with a pin and a button shared by two programs: one blinks on a timer, the other sleeps until the button's interrupt |

What they print, from CI:

```
vm/slices
    0 ms  program 1: round 1 done
    0 ms  program 2: round 1 done
    5 ms  program 1: round 2 done
   10 ms  program 1: round 3 done
   10 ms  program 2: round 2 done
   15 ms  program 1: ended with 169 after 214 slices
   20 ms  program 2: round 3 done
   30 ms  program 2: ended with 40 after 424 slices

rtos/devices
    0 ms  [button] "led" is in use by another program
   10 ms  [blinker] led on
   20 ms  [blinker] led off
   26 ms  [button] pressed (1)
   30 ms  [blinker] led on
   ...
```

## Running them

```sh
git clone --recursive https://github.com/wasm-rtos/wasm-rtos-examples
cd wasm-rtos-examples
./build.sh                  # every example
./build.sh vm/slices        # one
```

Each program's `.wasm` is kept next to its source, so a C compiler for your
computer is all you need: without a WebAssembly toolchain `build.sh` runs
those. To build the programs too, you need a clang that targets
WebAssembly with a WASI sysroot: the
[WASI SDK](https://github.com/WebAssembly/wasi-sdk) (set `WASM_CC` to its
`clang` and `WASI_SYSROOT` to its `share/wasi-sysroot`), or on macOS
`brew install llvm wasi-libc`, which `build.sh` finds by itself. The AVR
build of the mini OS needs `avr-gcc` and is skipped without it.

The hosts are built with AddressSanitizer and UBSan: an example is also a
test.

## Where to go next

- [wasm-rtos-vm](https://github.com/wasm-rtos/wasm-rtos-vm): the
  interpreter's API (`wm.h`, `wm_wasi.h`) and its options for small chips.
- [wasm-rtos-hw](https://github.com/wasm-rtos/wasm-rtos-hw): every device,
  and how to write the port of a board.
- [wasm-rtos](https://github.com/wasm-rtos/wasm-rtos): the kernel, the ABI
  programs use (`include/wasm-rtos.h`, `docs/abi.md`) and porting it.

## License

Public domain, as the rest of wasm-rtos.
