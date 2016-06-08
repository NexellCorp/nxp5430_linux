#!/bin/bash

SRC_NAME=S5P6818_2ndboot_aarch32_DDR3_V036

DST_NAME1=2ndboot_avn_ref
DST_NAME2=2ndboot_drone

echo "$SECBOOT_NAME"

cp ../../temporary/2ndboot/"$SRC_NAME"_ALL.bin ./"$SRC_NAME"_ALL.bin

cp ../../temporary/2ndboot/"$SRC_NAME"_AVN_ALL.bin   ./"$DST_NAME1".bin
cp ../../temporary/2ndboot/"$SRC_NAME"_AVN_ALL.bin   ./"$DST_NAME1"_sdmmc.bin

cp ../../temporary/2ndboot/"$SRC_NAME"_DRONE_ALL.bin ./"$DST_NAME2".bin
cp ../../temporary/2ndboot/"$SRC_NAME"_DRONE_ALL.bin ./"$DST_NAME2"_sdmmc.bin
