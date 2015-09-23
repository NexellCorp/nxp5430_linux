#!/bin/bash

SECBOOT_SDMMC=S5P6818_NSIH_V03_sdmmc_400_800_16x2_1cs_1GB.txt
SECBOOT_USB=S5P6818_NSIH_V03_usb_400_800_16x2_1cs_1GB.txt

DST_NAME1=nsih_svt
DST_NAME2=nsih_dronel
DST_NAME3=nsih_drone

cp ../../temporary/nsih/"$SECBOOT_SDMMC" ./"$DST_NAME1"_sdmmc.txt
cp ../../temporary/nsih/"$SECBOOT_SDMMC" ./"$DST_NAME2"_sdmmc.txt
cp ../../temporary/nsih/"$SECBOOT_SDMMC" ./"$DST_NAME3"_sdmmc.txt

cp ../../temporary/nsih/"$SECBOOT_USB" ./"$DST_NAME1"_usb.txt
cp ../../temporary/nsih/"$SECBOOT_USB" ./"$DST_NAME2"_usb.txt
cp ../../temporary/nsih/"$SECBOOT_USB" ./"$DST_NAME3"_usb.txt
