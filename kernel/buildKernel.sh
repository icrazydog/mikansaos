#!/bin/sh -ex

CPPFLAGS="-I~/mikansaos/env/x86_64-elf/include -I~/mikansaos/env/x86_64-elf/include/c++/v1 -nostdlibinc -D__ELF__ -D_LDBL_EQ_DBL -D_GNU_SOURCE -D_POSIX_TIMERS"

LDFLAGS="-L~/mikansaos/env/x86_64-elf/lib"

clang++ $CPPFLAGS -O2 --target=x86_64-elf -fno-exceptions -ffreestanding -c main.cpp

ld.lld $LDFLAGS --entry KernelMain -z norelro --image-base 0x100000 --static -o kernel.elf main.o
