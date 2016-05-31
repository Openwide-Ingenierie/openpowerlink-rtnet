#!/bin/sh
# Script to build openPOWERLINK-RTnet demo app
#

# Path to Xenomai sources
XENO_SRC=
# Path to Xenomai libraries
XENO_LIB=

XENO_CFLAGS="-I${XENO_SRC}/include/ -I${XENO_SRC}/include/cobalt/ -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -Os -D_GNU_SOURCE -D_REENTRANT -D__COBALT__ -D__COBALT_WRAP__"
XENO_LDFLAGS="-Wl,@${XENO_SRC}/lib/cobalt/cobalt.wrappers $XENO_SRC/lib/boilerplate/init/bootstrap.o -Wl,--wrap=main -Wl,--dynamic-list=${XENO_SRC}/scripts/dynlist.ld -L${XENO_LIB} -lpthread -lrt -lcobalt"

#TARGET=Debug
TARGET=Release

cmake -DXENO_CFLAGS="${XENO_CFLAGS}" \
             -DXENO_LDFLAGS="${XENO_LDFLAGS}" \
             -DCMAKE_BUILD_TYPE=${TARGET}  \
             -DRTNET_STACK_INCLUDE=${XENO_SRC}/kernel/drivers/net/stack/include \
             -DCMAKE_SYSTEM_PROCESSOR=arm \
             ../..

if [ $? -eq 0 ]; then
    echo
    echo "You should now type \"make\" then \"make install\""
fi
