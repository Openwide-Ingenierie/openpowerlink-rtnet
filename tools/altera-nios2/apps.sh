#!/bin/bash
################################################################################
#
# Apps generator script for Altera Nios II
#
# Copyright (c) 2014, Bernecker+Rainer Industrie-Elektronik Ges.m.b.H. (B&R)
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of the copyright holders nor the
#       names of its contributors may be used to endorse or promote products
#       derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDERS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
################################################################################

APP_PATH=$1
BOARD_PATH=$2
CFLAGS="-Wall -Wextra -pedantic -std=c99"

if [ -z "${OPLK_BASE_DIR}" ];
then
    # Find oplk root path
    SCRIPTDIR=$(dirname $(readlink -f "${0}"))
    OPLK_BASE_DIR=$(readlink -f "${SCRIPTDIR}/../..")
fi

# process arguments
DEBUG=
DEBUG_ARG=
OUT_PATH=.
while [ $# -gt 0 ]
do
    case "$1" in
        --debug)
            DEBUG=1
            DEBUG_ARG=$1
            ;;
        --out)
            shift
            OUT_PATH=$1
            ;;
        --help)
            echo "$ apps.sh [APP] [BOARD] [OPTIONS]"
            echo "APP       ... Path to application project (apps.settings)"
            echo "BOARD     ... Path to hardware project (board.settings)"
            echo "OPTIONS   ... :"
            echo "              --debug ... Lib is generated with O0"
            echo "              --out   ... Path to output directory"
            exit 1
            ;;
        *)
            ;;
    esac
    shift
done

if [ -z "${APP_PATH}" ];
then
    echo "ERROR: No application path is given!"
fi

if [ -z "${BOARD_PATH}" ];
then
    echo "ERROR: No board path is given!"
fi

# Let's source the board.settings (null.settings before)
BOARD_SETTINGS_FILE=${BOARD_PATH}/board.settings
CFG_APP_CPU_NAME=
CFG_APP_EPCS=
CFG_JTAG_CABLE=
if [ -f ${BOARD_SETTINGS_FILE} ]; then
    source ${BOARD_SETTINGS_FILE}
else
    echo "ERROR: ${BOARD_SETTINGS_FILE} not found!"
    exit 1
fi

if [ -z "${CFG_APP_CPU_NAME}" ]; then
    echo "ERROR: The board has no CPU processing app!"
    exit 1
fi

BSP_PATH=${OUT_PATH}/bsp-${CFG_APP_CPU_NAME}

BSP_GEN_ARGS="${CFG_APP_BSP_TYPE} ${BSP_PATH} ${BOARD_PATH}/quartus \
--set hal.enable_c_plus_plus false \
--set hal.linker.enable_alt_load_copy_exceptions false \
--set hal.enable_clean_exit false \
--set hal.enable_exit false \
--cpu-name ${CFG_APP_CPU_NAME} \
--set hal.sys_clk_timer ${CFG_APP_SYS_TIMER_NAME} \
--cmd add_section_mapping .tc_i_mem ${CFG_APP_TCI_MEM_NAME} \
"

if [ -z "${DEBUG}" ];
then
    BSP_GEN_ARGS+="\
    --set hal.stdout none --set hal.stdin none --set hal.stderr none \
    --set hal.make.bsp_cflags_optimization ${CFG_APP_BSP_OPT_LEVEL} \
    "
else
    BSP_GEN_ARGS+="--set hal.make.bsp_cflags_optimization -O0"
    echo "INFO: Prepare BSP for debug."
fi

nios2-bsp ${BSP_GEN_ARGS}
RET=$?

if [ ${RET} -ne 0 ]; then
    echo "ERROR: BSP generation returned with error ${RET}!"
    exit ${RET}
fi

# Now fix the generated linker.x file...
chmod +x ${OPLK_BASE_DIR}/tools/altera-nios2/fix-linkerx
${OPLK_BASE_DIR}/tools/altera-nios2/fix-linkerx ${BSP_PATH}

# Generate the library fitting to the board
chmod +x ${OPLK_BASE_DIR}/tools/altera-nios2/stack.sh
${OPLK_BASE_DIR}/tools/altera-nios2/stack.sh ${BSP_PATH} --out ${OUT_PATH} ${DEBUG_ARG}
RET=$?

if [ ${RET} -ne 0 ]; then
    echo "ERROR: Library generation returned with error ${RET}!"
    exit ${RET}
fi

# Let's source the apps.settings
CFG_APP_ARGS=
APP_SETTINGS_FILE=${APP_PATH}/apps.settings
if [ -f ${APP_SETTINGS_FILE} ]; then
    source ${APP_SETTINGS_FILE}
else
    echo "ERROR: ${APP_SETTINGS_FILE} not found!"
    exit 1
fi

if [ -n "${DEBUG}" ];
then
    APP_OPT_LEVEL=-O0
    DEBUG_MODE=_DEBUG
else
    DEBUG_MODE=NDEBUG
fi

APP_GEN_ARGS="\
--app-dir ${OUT_PATH} \
--bsp-dir ${BSP_PATH} \
--elf-name ${APP_NAME}.elf \
--src-files ${APP_SOURCES} \
--set APP_CFLAGS_OPTIMIZATION ${APP_OPT_LEVEL} \
--set CFLAGS=${CFLAGS} -D${DEBUG_MODE} \
--set OBJDUMP_INCLUDE_SOURCE 1 \
--set CREATE_OBJDUMP 0 \
--set QSYS_SUB_CPU ${CFG_APP_PROC_NAME} \
--set QUARTUS_PROJECT_DIR=${BOARD_PATH}/quartus \
--set OPLK_BASE_DIR=${OPLK_BASE_DIR} \
${CFG_APP_ARGS} \
"

# Get path to board includes
BOARD_INCLUDE_PATH=$(readlink -f "${BOARD_PATH}/include")

if [ ! -d "${BOARD_INCLUDE_PATH}" ];
then
    echo "ERROR: Path to board include does not exist (${BOARD_INCLUDE_PATH})!"
    exit 1
fi

# Add board includes to app includes
APP_INCLUDES+=" ${BOARD_INCLUDE_PATH}"

# Add includes
for i in ${APP_INCLUDES}
do
    APP_GEN_ARGS+="--inc-dir ${i} "
done

# Add JTAG cable from board.settings
if [ -n "${CFG_JTAG_CABLE}" ];
then
    APP_GEN_ARGS+="--set DOWNLOAD_CABLE=${CFG_JTAG_CABLE} "
    echo "INFO: Set JTAG Cable to ${CFG_JTAG_CABLE}."
fi

# And add stack library
LIB_STACK_DIR=$(find ${OUT_PATH} -type d -name "liboplk*")

APP_GEN_ARGS+="--use-lib-dir ${LIB_STACK_DIR} "

nios2-app-generate-makefile ${APP_GEN_ARGS}
RET=$?

if [ ${RET} -ne 0 ]; then
    echo "ERROR: Application generation returned with error ${RET}!"
    exit ${RET}
fi

chmod +x ${OPLK_BASE_DIR}/tools/altera-nios2/fix-app-makefile
${OPLK_BASE_DIR}/tools/altera-nios2/fix-app-makefile ${OUT_PATH}/Makefile

# Add EPCS flash makefile rules
if [ -n "${CFG_APP_EPCS}" ]; then
    chmod +x ${OPLK_BASE_DIR}/tools/altera-nios2/add-app-makefile-epcs
    ${OPLK_BASE_DIR}/tools/altera-nios2/add-app-makefile-epcs ${OUT_PATH}/Makefile
fi

exit 0
