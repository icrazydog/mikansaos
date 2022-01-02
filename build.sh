#!/bin/sh -eu

CPPFLAGS="-I~/mikansaos/env/x86_64-elf/include/c++/v1 -I~/mikansaos/env/x86_64-elf/include -nostdlibinc -D__ELF__ -D_LDBL_EQ_DBL -D_GNU_SOURCE -D_POSIX_TIMERS"
LDFLAGS="-L~/mikansaos/env/x86_64-elf/lib"
export CPPFLAGS
export LDFLAGS

make -C kernel kernel.elf

for MK in $(ls apps/*/Makefile)
do
  APP_DIR=$(dirname $MK)
  APP=$(basename $APP_DIR)
  make -C $APP_DIR $APP
done

if [ "${1:-}" = "mloader" ]
then
  ./cpLoaderAndKernel.sh b
fi

if [ "${1:-}" = "run" ]
then
  ./cpLoaderAndKernel.sh
  ./run.sh Loader.efi kernel.elf
fi
