#!/bin/bash

SECBOOT_NAME=S5P6818_2ndboot_aarch32_DDR3_V036

DST_NAME1=2ndboot_svt
DST_NAME2=2ndboot_dronel
DST_NAME3=2ndboot_drone

echo "$SECBOOT_NAME"

cp ../../temporary/2ndboot/"$SECBOOT_NAME"_SDMMC.bin ./"$DST_NAME1"_sdmmc.bin
cp ../../temporary/2ndboot/"$SECBOOT_NAME"_SDMMC.bin ./"$DST_NAME2"_sdmmc.bin
cp ../../temporary/2ndboot/"$SECBOOT_NAME"_SDMMC.bin ./"$DST_NAME3"_sdmmc.bin

cp ../../temporary/2ndboot/"$SECBOOT_NAME"_USB.bin ./"$DST_NAME1"_usb.bin
cp ../../temporary/2ndboot/"$SECBOOT_NAME"_USB.bin ./"$DST_NAME2"_usb.bin
cp ../../temporary/2ndboot/"$SECBOOT_NAME"_USB.bin ./"$DST_NAME3"_usb.bin
