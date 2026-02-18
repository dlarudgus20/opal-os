#!/usr/bin/bash
: ${CONFIG:=debug}
: ${PLATFORM:=pc-x64}
: ${OUTPATH:="/mnt/d/VirtualBox VMs/opal-os/opal-os.iso"}
make iso CONFIG=$CONFIG PLATFORM=$PLATFORM || exit 1
cp "build/$PLATFORM/$CONFIG/opal-os.iso" "$OUTPATH" || exit 1
echo iso file is copied to $OUTPATH
