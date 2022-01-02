#!/bin/bash

if [ "$1" = "b" ]
then
  shift
  cd ~/mikansaos/env/edk2
  source edksetup.sh
  build
  cd -
fi
cp ~/mikansaos/env/edk2/Build/MikanLoaderX64/DEBUG_CLANG38/X64/Loader.efi .
cp kernel/kernel.elf .
