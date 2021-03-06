#!/bin/bash
set -e

git clone --depth=1 -b qemu-powernv-for-skiboot-1 git://github.com/open-power/qemu.git
cd qemu
git submodule update --init dtc
./configure --target-list=ppc64-softmmu --disable-werror
make -j `grep -c processor /proc/cpuinfo`
