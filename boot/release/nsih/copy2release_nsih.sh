#!/bin/bash

CS1_16x2_1GB_SDMMC=S5P6818_NSIH_V03_sdmmc_400_800_16x2_1cs_1GB.txt
CS1_16x2_1GB_USB=S5P6818_NSIH_V03_usb_400_800_16x2_1cs_1GB.txt
CS1_16x2_1GB_SPI=S5P6818_NSIH_V03_spi_400_800_16x2_1cs_1GB.txt

CS1_8x4_2GB_SDMMC=S5P6818_NSIH_V03_sdmmc_400_800_8x4_1cs_2GB.txt
CS1_8x4_2GB_USB=S5P6818_NSIH_V03_usb_400_800_8x4_1cs_2GB.txt
CS1_8x4_2GB_SPI=S5P6818_NSIH_V03_spi_400_800_8x4_1cs_2GB.txt

DST_NAME1=nsih_svt
DST_NAME2=nsih_drone
DST_NAME3=nsih_avn_ref

cp ../../temporary/nsih/"$CS1_16x2_1GB_SDMMC" ./"$DST_NAME1"_sdmmc.txt
cp ../../temporary/nsih/"$CS1_16x2_1GB_SDMMC" ./"$DST_NAME2"_sdmmc.txt

cp ../../temporary/nsih/"$CS1_16x2_1GB_USB" ./"$DST_NAME1"_usb.txt
cp ../../temporary/nsih/"$CS1_16x2_1GB_USB" ./"$DST_NAME2"_usb.txt

cp ../../temporary/nsih/"$CS1_16x2_1GB_SPI" ./"$DST_NAME1"_spi.txt
cp ../../temporary/nsih/"$CS1_16x2_1GB_SPI" ./"$DST_NAME2"_spi.txt

cp ../../temporary/nsih/"$CS1_8x4_2GB_SDMMC" ./"$DST_NAME3"_sdmmc.txt
cp ../../temporary/nsih/"$CS1_8x4_2GB_USB"   ./"$DST_NAME3"_usb.txt
cp ../../temporary/nsih/"$CS1_8x4_2GB_SPI"   ./"$DST_NAME3"_spi.txt
