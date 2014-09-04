#!/bin/sh
make ARCH=arm clean
make ARCH=arm -j4
cp nx_vpu.ko ../../../../hardware/nexell/pyrope/prebuilt/modules/
