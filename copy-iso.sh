#!/usr/bin/bash
: ${CONFIG:=debug}
: ${PLATFORM:=pc-x64}
: ${OUTPATH:="/mnt/d/VirtualBox VMs/opal-os/opal-os.iso"}
if [ "$UNIT_TEST" = "1" ]; then
    BUILD_DIR=build/unit-test
else
    BUILD_DIR=build
fi
make iso CONFIG=$CONFIG PLATFORM=$PLATFORM || exit 1
cp "$BUILD_DIR/$PLATFORM/$CONFIG/opal-os.iso" "$OUTPATH" || exit 1
echo iso file is copied to $OUTPATH
