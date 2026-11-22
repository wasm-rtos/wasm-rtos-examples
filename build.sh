#!/usr/bin/env bash
# Builds every example and runs it: ./build.sh [example...] (all by default).
#
# Programs are C built for WebAssembly: plain ones with clang --target=wasm32,
# ones that use the C library with a WASI sysroot (wasi-sdk, or WASI_SYSROOT).
# Hosts are C built for this computer with the interpreter, the devices and
# the kernel from the wasm-rtos submodule.
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

plain() { "$WASM_CC" --target=wasm32 -nostdlib -O2 -Wl,--no-entry -Wl,--export-dynamic "$@"; }
wasi() { "$WASM_CC" --target=wasm32-wasip1 "${SYSROOT[@]}" -O2 -I$K/include "$@"; }
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
    plain vm/hello/program.c -o $OUT/vm-hello.wasm &&
        host vm/hello/main.c $VM/*.c -o $OUT/vm-hello &&
        run vm/hello $OUT/vm-hello $OUT/vm-hello.wasm
}

example_vm_host_call()
{
    plain -Wl,--allow-undefined vm/host-call/program.c -o $OUT/vm-host-call.wasm &&
        host vm/host-call/main.c $VM/*.c -o $OUT/vm-host-call &&
        run vm/host-call $OUT/vm-host-call $OUT/vm-host-call.wasm
}

example_vm_slices()
{
    plain -Wl,--allow-undefined vm/slices/program.c -o $OUT/vm-slices.wasm &&
        host vm/slices/main.c $VM/*.c -o $OUT/vm-slices &&
        run vm/slices $OUT/vm-slices $OUT/vm-slices.wasm
}

example_vm_wasi()
{
    wasi vm/wasi/program.c -o $OUT/vm-wasi.wasm &&
        host vm/wasi/main.c $VM/*.c -o $OUT/vm-wasi &&
        run vm/wasi $OUT/vm-wasi $OUT/vm-wasi.wasm
}

example_mini_os()
{
    "$WASM_CC" --target=wasm32 -nostdlib -O2 -I$HW/include -Wl,--no-entry -Wl,--export=app_main \
        -Wl,-z,stack-size=256 -Wl,--page-size=1 mini-os/program.c -o $OUT/mini-os.wasm &&
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
    wasi rtos/tasks/program.c -o $OUT/rtos-tasks.wasm &&
        kernel_host rtos/tasks/main.c -o $OUT/rtos-tasks &&
        run rtos/tasks $OUT/rtos-tasks $OUT/rtos-tasks.wasm
}

example_rtos_devices()
{
    wasi rtos/devices/blinker.c -o $OUT/rtos-blinker.wasm &&
        wasi rtos/devices/button.c -o $OUT/rtos-button.wasm &&
        kernel_host rtos/devices/main.c -o $OUT/rtos-devices &&
        run rtos/devices $OUT/rtos-devices $OUT/rtos-blinker.wasm $OUT/rtos-button.wasm
}

mkdir -p $OUT
all="vm_hello vm_host_call vm_slices vm_wasi mini_os mini_os_avr rtos_tasks rtos_devices"
for e in ${@:-$all}; do
    e=${e//\//_}
    e=${e//-/_}
    "example_$e" || { echo "FAIL  $e (build)"; fail=1; }
done
exit $fail
