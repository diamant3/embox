#!/bin/bash

PYTHON=python3
OPENOCD=~/git/openocd/src/openocd
MIK32_UPLOADER=~/mikron/mik32-uploader

#EMBOX_DIR=`pwd`

riscv64-unknown-elf-objcopy -O ihex build/base/bin/embox $MIK32_UPLOADER/embox.hex

#sudo $OPENOCD -f $MIK32_UPLOADER/openocd-scripts/interface/ftdi/mikron-link.cfg -f $MIK32_UPLOADER/openocd-scripts/target/mik32.cfg -c "init;halt"
pushd $MIK32_UPLOADER
sudo $PYTHON mik32_upload.py embox.hex --run-openocd --openocd-exec=$OPENOCD --openocd-interface=openocd-scripts/interface/ftdi/mikron-link.cfg --openocd-target=openocd-scripts/target/mik32.cfg --adapter-speed=4000
popd