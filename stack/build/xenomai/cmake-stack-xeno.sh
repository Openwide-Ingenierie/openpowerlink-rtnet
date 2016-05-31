#!/bin/sh
# Script to build openPOWERLINK-RTnet stack
#

# Path to Xenomai sources
XENO_SRC=

XENO_CFLAGS="-I${XENO_SRC}/include/ -I${XENO_SRC}/include/cobalt/ -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -Os -D_GNU_SOURCE -D_REENTRANT -D__COBALT__ -D__COBALT_WRAP__"
XENO_LDFLAGS="-Wl,@${XENO_SRC}/lib/cobalt/cobalt.wrappers -Wl,--wrap=main -Wl,--dynamic-list=${XENO_SRC}/scripts/dynlist.ld"

# Release target
cmake -DXENO_CFLAGS="${XENO_CFLAGS}" \
             -DXENO_LDFLAGS="${XENO_LDFLAGS}" \
             -DCMAKE_BUILD_TYPE=Release \
             -DCFG_COMPILE_LIB_MN=ON \
             -DCFG_COMPILE_LIB_CN=ON \
             -DRTNET_STACK_INCLUDE=${XENO_SRC}/kernel/drivers/net/stack/include \
             -DCMAKE_SYSTEM_PROCESSOR=arm \
             ../..

make
make install

# Debug target
cmake -DXENO_CFLAGS="${XENO_CFLAGS}" \
             -DXENO_LDFLAGS="${XENO_LDFLAGS}" \
             -DCMAKE_BUILD_TYPE=Debug \
             -DCFG_COMPILE_LIB_MN=ON \
             -DCFG_COMPILE_LIB_CN=ON  \
             -DRTNET_STACK_INCLUDE=${XENO_SRC}/kernel/drivers/net/stack/include \
             -DCMAKE_SYSTEM_PROCESSOR=arm \
              ../..

make
make install
