#!/usr/bin/env bash
# Builds every example and runs it: ./build.sh [--save] [example...] (all by default).
#
# Programs are C built for WebAssembly: plain ones with clang --target=wasm32,
# ones that use the C library with a WASI sysroot (wasi-sdk, or WASI_SYSROOT).
# Each one's .wasm is also kept next to its source (program.c, program.wasm):
# without a WebAssembly toolchain the examples run with those. --save writes
# the programs built here over them (CI does on main, with its pinned SDK);
# STRICT=1 fails instead of falling back. Hosts are C built for this computer
# with the interpreter, the devices and the kernel from the wasm-rtos
# submodule.
set -uo pipefail
cd "$(dirname "$0")"

K=wasm-rtos            # the kernel; its vm/ and hw/ are the interpreter and the devices
VM=$K/vm/source
HW=$K/hw
OUT=build
CC="${CC:-cc}"
WASM_CC="${WASM_CC:-}"
WASI_SYSROOT="${WASI_SYSROOT:-}"
# A clang that targets WebAssembly, and a WASI sysroot, where Homebrew puts them.
if [ -z "$WASM_CC" ]; then
    WASM_CC=clang
    [ -x /opt/homebrew/opt/llvm/bin/clang ] && WASM_CC=/opt/homebrew/opt/llvm/bin/clang
fi
[ -z "$WASI_SYSROOT" ] && [ -d /opt/homebrew/opt/wasi-libc/share/wasi-sysroot ] &&
    WASI_SYSROOT=/opt/homebrew/opt/wasi-libc/share/wasi-sysroot
SYSROOT=()
[ -n "$WASI_SYSROOT" ] && SYSROOT=(--sysroot="$WASI_SYSROOT")
# Sanitizers on the hosts where the compiler has them: an example is also a test.
HOST_FLAGS=(-std=gnu11 -O1 -g -Wall -Wextra -Werror -Wno-unused-parameter -fsanitize=address,undefined)

SAVE=0
STRICT="${STRICT:-0}"
[ "${1:-}" = "--save" ] && { SAVE=1; shift; }

# program <kind> <source.c> <out.wasm> [flags]: built into out, or the committed copy next to the source.
program()
{
    local kind=$1 source=$2 out=$3 kept=${2%.c}.wasm
    shift 3
    case $kind in
    plain) "$WASM_CC" --target=wasm32 -nostdlib -O2 -Wl,--no-entry -Wl,--export-dynamic "$@" "$source" -o "$out" ;;
    wasi) "$WASM_CC" --target=wasm32-wasip1 "${SYSROOT[@]}" -O2 -I$K/include "$@" "$source" -o "$out" ;;
    esac 2>"$out.log" && { [ $SAVE = 1 ] && cp "$out" "$kept"; return 0; }
    if [ "$STRICT" != 1 ] && [ -s "$kept" ]; then
        echo "note  $source: not built here (no WebAssembly toolchain?), running the committed $kept"
        cp "$kept" "$out"
        return 0
    fi
    cat "$out.log"
    return 1
}
# dylink <library.c> <program.c> <out.so> <out.wasm>: a -shared library and a -pie program that needs it,
# or the committed copies next to the sources (library.so, program.wasm).
dylink()
{
    local lib=$1 app=$2 so=$3 wasm=$4 ld="${WASM_LD:-wasm-ld}" pic=()
    "$ld" --help 2>&1 | grep -- '--experimental-pic' >/dev/null && pic=(--experimental-pic)
    {
        "$WASM_CC" --target=wasm32 -nostdlib -fno-builtin -fPIC -fvisibility=default -O2 -c "$lib" -o "$so.o" &&
            "$ld" -m wasm32 ${pic[@]+"${pic[@]}"} --shared --no-entry --export-dynamic "$so.o" -o "$so" &&
            "$WASM_CC" --target=wasm32 -nostdlib -fno-builtin -fPIC -O2 -c "$app" -o "$wasm.o" &&
            "$ld" -m wasm32 ${pic[@]+"${pic[@]}"} --pie --no-entry --export=_start "$wasm.o" "$so" -o "$wasm"
    } 2>"$wasm.log" && { [ $SAVE = 1 ] && cp "$so" "${lib%.c}.so" && cp "$wasm" "${app%.c}.wasm"; return 0; }
    if [ "$STRICT" != 1 ] && [ -s "${lib%.c}.so" ] && [ -s "${app%.c}.wasm" ]; then
        echo "note  $app: not built here (no WebAssembly toolchain?), running the committed copies"
        cp "${lib%.c}.so" "$so" && cp "${app%.c}.wasm" "$wasm"
        return 0
    fi
    cat "$wasm.log"
    return 1
}
host() { "$CC" "${HOST_FLAGS[@]}" -I$VM "$@" -lm; }
kernel_host() { host -I$K -I$HW "$@" $K/*.c $HW/*.c $VM/*.c; }

fail=0
run() # name, then the command that runs it
{
    local name=$1 log="$OUT/${1//\//-}.log"
    shift
    if "$@" >"$log" 2>&1; then
        echo "ok    $name"
        sed 's/^/      /' "$log"
    else
        echo "FAIL  $name"
        sed 's/^/      /' "$log"
        fail=1
    fi
}

example_vm_hello()
{
    program plain vm/hello/program.c $OUT/vm-hello.wasm &&
        host vm/hello/main.c $VM/*.c -o $OUT/vm-hello &&
        run vm/hello $OUT/vm-hello $OUT/vm-hello.wasm
}

example_vm_host_call()
{
    program plain vm/host-call/program.c $OUT/vm-host-call.wasm -Wl,--allow-undefined &&
        host vm/host-call/main.c $VM/*.c -o $OUT/vm-host-call &&
        run vm/host-call $OUT/vm-host-call $OUT/vm-host-call.wasm
}

example_vm_slices()
{
    program plain vm/slices/program.c $OUT/vm-slices.wasm -Wl,--allow-undefined &&
        host vm/slices/main.c $VM/*.c -o $OUT/vm-slices &&
        run vm/slices $OUT/vm-slices $OUT/vm-slices.wasm
}

example_vm_wasi()
{
    program wasi vm/wasi/program.c $OUT/vm-wasi.wasm &&
        host vm/wasi/main.c $VM/*.c -o $OUT/vm-wasi &&
        run vm/wasi $OUT/vm-wasi $OUT/vm-wasi.wasm
}

example_mini_os()
{
    # Stack of 256 bytes and pages of one byte: the program fits a board with 8 KB of RAM.
    program plain mini-os/program.c $OUT/mini-os.wasm -I$HW/include -Wl,--export=app_main \
        -Wl,-z,stack-size=256 -Wl,--page-size=1 &&
        host -DHW_HAS_SYSTEM=0 -I$HW mini-os/main.c $HW/*.c $HW/single/*.c $VM/*.c -o $OUT/mini-os &&
        run mini-os $OUT/mini-os $OUT/mini-os.wasm
}

# The same mini OS for an AVR, its size only (an ATmega2560: it does not fit an Uno's 32 KB).
example_mini_os_avr()
{
    command -v avr-gcc >/dev/null || { echo "skip  mini-os on AVR (no avr-gcc)"; return 0; }
    (cd $OUT && cp mini-os.wasm program.wasm && xxd -i program.wasm |
        sed 's/^unsigned char program_wasm\[\] = {/static const unsigned char program_wasm[] PROGMEM = {/; s/^unsigned int /static const unsigned int /' \
        > program.wasm.h) &&
        avr-gcc -mmcu=atmega2560 -Os -std=gnu99 -ffunction-sections -fdata-sections -DHW_HAS_SYSTEM=0 \
            -DWM_HAS_VALIDATION=0 -DWM_HAS_I64=0 -DWM_HAS_REF_TYPES=0 -DWM_PROGRAM_IN_FLASH -I$VM -I$HW -I$OUT \
            -Wl,--gc-sections mini-os/avr_main.c $HW/*.c $HW/single/*.c $VM/wm_*.c -o $OUT/mini-os.elf &&
        run "mini-os on AVR" avr-size $OUT/mini-os.elf
}

example_rtos_tasks()
{
    program wasi rtos/tasks/program.c $OUT/rtos-tasks.wasm &&
        kernel_host rtos/tasks/main.c -o $OUT/rtos-tasks &&
        run rtos/tasks $OUT/rtos-tasks $OUT/rtos-tasks.wasm
}

# dylink.0: the loader of wasm-rtos-dylink joins the kernel (os_dylink_add).
example_rtos_dylink()
{
    dylink rtos/dylink/libtext.c rtos/dylink/program.c $OUT/libtext.so $OUT/rtos-dylink.wasm &&
        kernel_host -Iwasm-rtos-dylink rtos/dylink/main.c wasm-rtos-dylink/dylink.c wasm-rtos-dylink/os_dylink.c \
            -o $OUT/rtos-dylink &&
        run rtos/dylink $OUT/rtos-dylink $OUT/rtos-dylink.wasm $OUT/libtext.so
}

example_rtos_devices()
{
    program wasi rtos/devices/blinker.c $OUT/rtos-blinker.wasm &&
        program wasi rtos/devices/button.c $OUT/rtos-button.wasm &&
        kernel_host rtos/devices/main.c -o $OUT/rtos-devices &&
        run rtos/devices $OUT/rtos-devices $OUT/rtos-blinker.wasm $OUT/rtos-button.wasm
}

mkdir -p $OUT
all="vm_hello vm_host_call vm_slices vm_wasi mini_os mini_os_avr rtos_tasks rtos_devices rtos_dylink"
for e in ${@:-$all}; do
    e=${e//\//_}
    e=${e//-/_}
    "example_$e" || { echo "FAIL  $e (build)"; fail=1; }
done
exit $fail
