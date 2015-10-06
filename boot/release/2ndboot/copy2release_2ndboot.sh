#!/bin/bash

SRC_NAME=S5P6818_2ndboot_aarch32_DDR3_V036

DST_NAME1=2ndboot_svt
DST_NAME2=2ndboot_drone
DST_NAME3=2ndboot_asb
DST_NAME4=2ndboot_avn_ref

echo "$SECBOOT_NAME"

cp ../../temporary/2ndboot/"$SRC_NAME"_ALL.bin ./"$SRC_NAME"_ALL.bin

#cp ../../temporary/2ndboot/"$SRC_NAME"_SVT_ALL.bin   ./"$DST_NAME1".bin
cp ../../temporary/2ndboot/"$SRC_NAME"_DRONE_ALL.bin ./"$DST_NAME2".bin
#cp ../../temporary/2ndboot/"$SRC_NAME"_ASB_ALL.bin   ./"$DST_NAME3".bin
cp ../../temporary/2ndboot/"$SRC_NAME"_AVN_ALL.bin   ./"$DST_NAME4".bin
