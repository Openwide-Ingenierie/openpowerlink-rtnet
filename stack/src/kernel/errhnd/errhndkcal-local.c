/**
********************************************************************************
\file   errhndkcal-local.c

\brief  Implementation of kernel CAL module for error handler

This module implements the CAL functions in kernel layer for the error handler.
This implementation uses a static variable which will be referenced from user
and from kernel space. It can be used if the user and kernel part is running
in the same domain and global variables can be shared.

\ingroup module_errhndkcal
*******************************************************************************/

/*------------------------------------------------------------------------------
Copyright (c) 2014, Bernecker+Rainer Industrie-Elektronik Ges.m.b.H. (B&R)
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holders nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDERS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
------------------------------------------------------------------------------*/

//------------------------------------------------------------------------------
// includes
//------------------------------------------------------------------------------
#include <common/oplkinc.h>
#include "errhndkcal.h"

//============================================================================//
//            G L O B A L   D E F I N I T I O N S                             //
//============================================================================//

//------------------------------------------------------------------------------
// const defines
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// module global vars
//------------------------------------------------------------------------------
tErrHndObjects              errhndk_errorObjects_g;

//------------------------------------------------------------------------------
// global function prototypes
//------------------------------------------------------------------------------


//============================================================================//
//            P R I V A T E   D E F I N I T I O N S                           //
//============================================================================//

//------------------------------------------------------------------------------
// const defines
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// local types
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// local vars
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// local function prototypes
//------------------------------------------------------------------------------

//============================================================================//
//            P U B L I C   F U N C T I O N S                                 //
//============================================================================//

//------------------------------------------------------------------------------
/**
\brief    Initialize error handler user CAL module

The function initializes the user layer CAL module of the error handler.

\return     Returns always kErrorOk

\ingroup module_errhndkcal
*/
//------------------------------------------------------------------------------
tOplkError errhndkcal_init(void)
{
    OPLK_MEMSET(&errhndk_errorObjects_g, 0, sizeof(tErrHndObjects));

    return kErrorOk;
}

//------------------------------------------------------------------------------
/**
\brief    shutdown error handler user CAL module

The function is used to deinitialize and shutdown the user layer
CAL module of the error handler.

\ingroup module_errhndkcal
*/
//------------------------------------------------------------------------------
void errhndkcal_exit(void)
{
}

//------------------------------------------------------------------------------
/**
\brief    get pointer to error handler objects

The function returns a pointer to the memory block where the error handler
objects are stored.

\ingroup module_errhndkcal
*/
//------------------------------------------------------------------------------
tErrHndObjects* errhndkcal_getMemPtr(void)
{
    return &errhndk_errorObjects_g;
}

//------------------------------------------------------------------------------
// getters
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
/**
\brief  Get error objects for LossOfSoc error

\param  pCumulativeCnt_p      Pointer to store cumulative counter.
\param  pThresholdCnt_p       Pointer to store threshold counter.
\param  pThreshold_p          Pointer to store threshold.

\ingroup module_errhndkcal
*/
//------------------------------------------------------------------------------
void errhndkcal_getCnLossSocError(UINT32* pCumulativeCnt_p,
                                  UINT32* pThresholdCnt_p,
                                  UINT32* pThreshold_p)
{
    *pCumulativeCnt_p = errhndk_errorObjects_g.cnLossSoc.cumulativeCnt;
    *pThresholdCnt_p = errhndk_errorObjects_g.cnLossSoc.thresholdCnt;
    *pThreshold_p = errhndk_errorObjects_g.cnLossSoc.threshold;
}

//------------------------------------------------------------------------------
/**
\brief  Get error objects for LossOfPreq error

\param  pCumulativeCnt_p      Pointer to store cumulative counter.
\param  pThresholdCnt_p       Pointer to store threshold counter.
\param  pThreshold_p          Pointer to store threshold.

\ingroup module_errhndkcal
*/
//------------------------------------------------------------------------------
void errhndkcal_getCnLossPreqError(UINT32* pCumulativeCnt_p,
                                   UINT32* pThresholdCnt_p,
                                   UINT32* pThreshold_p)
{
    *pCumulativeCnt_p = errhndk_errorObjects_g.cnLossPreq.cumulativeCnt;
    *pThresholdCnt_p = errhndk_errorObjects_g.cnLossPreq.thresholdCnt;
    *pThreshold_p = errhndk_errorObjects_g.cnLossPreq.threshold;
}

//------------------------------------------------------------------------------
/**
\brief  Get error objects for CN CRC error

\param  pCumulativeCnt_p      Pointer to store cumulative counter.
\param  pThresholdCnt_p       Pointer to store threshold counter.
\param  pThreshold_p          Pointer to store threshold.

\ingroup module_errhndkcal
*/
//------------------------------------------------------------------------------
void errhndkcal_getCnCrcError(UINT32* pCumulativeCnt_p,
                              UINT32* pThresholdCnt_p,
                              UINT32* pThreshold_p)

{
    *pCumulativeCnt_p = errhndk_errorObjects_g.cnCrcErr.cumulativeCnt;
    *pThresholdCnt_p = errhndk_errorObjects_g.cnCrcErr.thresholdCnt;
    *pThreshold_p = errhndk_errorObjects_g.cnCrcErr.threshold;
}

#ifdef CONFIG_INCLUDE_NMT_MN
//------------------------------------------------------------------------------
/**
\brief  Get error objects for MN CRC error

\param  pCumulativeCnt_p      Pointer to store cumulative counter.
\param  pThresholdCnt_p       Pointer to store threshold counter.
\param  pThreshold_p          Pointer to store threshold.

\ingroup module_errhndkcal
*/
//------------------------------------------------------------------------------
void errhndkcal_getMnCrcError(UINT32* pCumulativeCnt_p,
                              UINT32* pThresholdCnt_p,
                              UINT32* pThreshold_p)
{
    *pCumulativeCnt_p = errhndk_errorObjects_g.mnCrcErr.cumulativeCnt;
    *pThresholdCnt_p = errhndk_errorObjects_g.mnCrcErr.thresholdCnt;
    *pThreshold_p = errhndk_errorObjects_g.mnCrcErr.threshold;
}

//------------------------------------------------------------------------------
/**
\brief  Get error objects for MNCycleTimeExceed error

\param  pCumulativeCnt_p      Pointer to store cumulative counter.
\param  pThresholdCnt_p       Pointer to store threshold counter.
\param  pThreshold_p          Pointer to store threshold.

\ingroup module_errhndkcal
*/
//------------------------------------------------------------------------------
void errhndkcal_getMnCycTimeExceedError(UINT32* pCumulativeCnt_p,
                                        UINT32* pThresholdCnt_p,
                                        UINT32* pThreshold_p)
{
    *pCumulativeCnt_p = errhndk_errorObjects_g.mnCycTimeExceed.cumulativeCnt;
    *pThresholdCnt_p = errhndk_errorObjects_g.mnCycTimeExceed.thresholdCnt;
    *pThreshold_p = errhndk_errorObjects_g.mnCycTimeExceed.threshold;
}

//------------------------------------------------------------------------------
/**
\brief  Get error objects for MNCNLossPres error

\param  nodeIdx_p             Index of node (node ID - 1).
\param  pCumulativeCnt_p      Pointer to store cumulative counter.
\param  pThresholdCnt_p       Pointer to store threshold counter.
\param  pThreshold_p          Pointer to store threshold.

\ingroup module_errhndkcal
*/
//------------------------------------------------------------------------------
void errhndkcal_getMnCnLossPresError(UINT nodeIdx_p,
                                     UINT32* pCumulativeCnt_p,
                                     UINT32* pThresholdCnt_p,
                                     UINT32* pThreshold_p)
{
    *pCumulativeCnt_p = errhndk_errorObjects_g.aMnCnLossPres[nodeIdx_p].cumulativeCnt;
    *pThresholdCnt_p = errhndk_errorObjects_g.aMnCnLossPres[nodeIdx_p].thresholdCnt;
    *pThreshold_p = errhndk_errorObjects_g.aMnCnLossPres[nodeIdx_p].threshold;
}
#endif

//------------------------------------------------------------------------------
/**
\brief  Get threshold counter for LosSoc error

\param  pThresholdCnt_p       Pointer to store threshold counter.

\ingroup module_errhndkcal
*/
//------------------------------------------------------------------------------
void errhndkcal_getLossSocThresholdCnt(UINT32* pThresholdCnt_p)
{
    *pThresholdCnt_p = errhndk_errorObjects_g.cnLossSoc.thresholdCnt;
}
//------------------------------------------------------------------------------
/**
\brief  Get threshold counter for LosPreq error

\param  pThresholdCnt_p       Pointer to store threshold counter.

\ingroup module_errhndkcal
*/
//------------------------------------------------------------------------------
void errhndkcal_getLossPreqThresholdCnt(UINT32* pThresholdCnt_p)
{
    *pThresholdCnt_p = errhndk_errorObjects_g.cnLossPreq.thresholdCnt;
}
//------------------------------------------------------------------------------
/**
\brief  Get threshold counter for CnCrc error

\param  pThresholdCnt_p       Pointer to store threshold counter.

\ingroup module_errhndkcal
*/
//------------------------------------------------------------------------------
void errhndkcal_getCnCrcThresholdCnt(UINT32* pThresholdCnt_p)
{
    *pThresholdCnt_p = errhndk_errorObjects_g.cnCrcErr.thresholdCnt;
}

#ifdef CONFIG_INCLUDE_NMT_MN
//------------------------------------------------------------------------------
/**
\brief  Get threshold counter for MnCrc error

\param  pThresholdCnt_p       Pointer to store threshold counter.

\ingroup module_errhndkcal
*/
//------------------------------------------------------------------------------
void errhndkcal_getMnCrcThresholdCnt(UINT32* pThresholdCnt_p)
{
    *pThresholdCnt_p = errhndk_errorObjects_g.mnCrcErr.thresholdCnt;
}
//------------------------------------------------------------------------------
/**
\brief  Get threshold counter for MnCycTimeExceed error

\param  pThresholdCnt_p       Pointer to store threshold counter.

\ingroup module_errhndkcal
*/
//------------------------------------------------------------------------------
void errhndkcal_getMnCycTimeExceedThresholdCnt(UINT32* pThresholdCnt_p)
{
    *pThresholdCnt_p = errhndk_errorObjects_g.mnCycTimeExceed.thresholdCnt;
}

//------------------------------------------------------------------------------
/**
\brief  Get threshold counter for MnCnLossPres error

\param  nodeIdx_p             Index of node (node ID - 1).
\param  pThresholdCnt_p       Pointer to store threshold counter.

\ingroup module_errhndkcal
*/
//------------------------------------------------------------------------------
void errhndkcal_getMnCnLossPresThresholdCnt(UINT nodeIdx_p,
                                            UINT32* pThresholdCnt_p)
{
    *pThresholdCnt_p = errhndk_errorObjects_g.aMnCnLossPres[nodeIdx_p].thresholdCnt;
}
#endif

//------------------------------------------------------------------------------
// setters
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
/**
\brief  Set error counters of LossSoc error

\param  cumulativeCnt_p       Cumulative counter to set.
\param  thresholdCnt_p        Threshold counter to set.

\ingroup module_errhndkcal
*/
//------------------------------------------------------------------------------
void errhndkcal_setCnLossSocCounters(UINT32 cumulativeCnt_p,
                                     UINT32 thresholdCnt_p)
{
    errhndk_errorObjects_g.cnLossSoc.cumulativeCnt = cumulativeCnt_p;
    errhndk_errorObjects_g.cnLossSoc.thresholdCnt = thresholdCnt_p;
}

//------------------------------------------------------------------------------
/**
\brief  Set error counters of LossPreq error

\param  cumulativeCnt_p       Cumulative counter to set.
\param  thresholdCnt_p        Threshold counter to set.

\ingroup module_errhndkcal
*/
//------------------------------------------------------------------------------
void errhndkcal_setCnLossPreqCounters(UINT32 cumulativeCnt_p,
                                      UINT32 thresholdCnt_p)
{
    errhndk_errorObjects_g.cnLossPreq.cumulativeCnt = cumulativeCnt_p;
    errhndk_errorObjects_g.cnLossPreq.thresholdCnt = thresholdCnt_p;
}

//------------------------------------------------------------------------------
/**
\brief  Set error counters of CnCrc error

\param  cumulativeCnt_p       Cumulative counter to set.
\param  thresholdCnt_p        Threshold counter to set.

\ingroup module_errhndkcal
*/
//------------------------------------------------------------------------------
void errhndkcal_setCnCrcCounters(UINT32 cumulativeCnt_p,
                                 UINT32 thresholdCnt_p)
{
    errhndk_errorObjects_g.cnCrcErr.cumulativeCnt = cumulativeCnt_p;
    errhndk_errorObjects_g.cnCrcErr.thresholdCnt = thresholdCnt_p;
}

#ifdef CONFIG_INCLUDE_NMT_MN
//------------------------------------------------------------------------------
/**
\brief  Set error counters of MnCrc error

\param  cumulativeCnt_p       Cumulative counter to set.
\param  thresholdCnt_p        Threshold counter to set.

\ingroup module_errhndkcal
*/
//------------------------------------------------------------------------------
void errhndkcal_setMnCrcCounters(UINT32 cumulativeCnt_p,
                                 UINT32 thresholdCnt_p)
{
    errhndk_errorObjects_g.mnCrcErr.cumulativeCnt = cumulativeCnt_p;
    errhndk_errorObjects_g.mnCrcErr.thresholdCnt = thresholdCnt_p;
}

//------------------------------------------------------------------------------
/**
\brief  Set error counters of CycTimeExceed error

\param  cumulativeCnt_p       Cumulative counter to set.
\param  thresholdCnt_p        Threshold counter to set.

\ingroup module_errhndkcal
*/
//------------------------------------------------------------------------------
void errhndkcal_setMnCycTimeExceedCounters(UINT32 cumulativeCnt_p,
                                           UINT32 thresholdCnt_p)
{
    errhndk_errorObjects_g.mnCycTimeExceed.cumulativeCnt = cumulativeCnt_p;
    errhndk_errorObjects_g.mnCycTimeExceed.thresholdCnt = thresholdCnt_p;
}

//------------------------------------------------------------------------------
/**
\brief  Set error counters of MnCnLossPres error

\param  nodeIdx_p               Index of node (node ID - 1).
\param  cumulativeCnt_p       Cumulative counter to set.
\param  thresholdCnt_p        Threshold counter to set.

\ingroup module_errhndkcal
*/
//------------------------------------------------------------------------------
void errhndkcal_setMnCnLossPresCounters(UINT nodeIdx_p,
                                        UINT32 cumulativeCnt_p,
                                        UINT32 thresholdCnt_p)
{
    errhndk_errorObjects_g.aMnCnLossPres[nodeIdx_p].cumulativeCnt = cumulativeCnt_p;
    errhndk_errorObjects_g.aMnCnLossPres[nodeIdx_p].thresholdCnt = thresholdCnt_p;
}
#endif

//------------------------------------------------------------------------------
/**
\brief  Set threshold counter of LossSoc error

\param  thresholdCnt_p        Threshold counter to set.

\ingroup module_errhndkcal
*/
//------------------------------------------------------------------------------
void errhndkcal_setLossSocThresholdCnt(UINT32 thresholdCnt_p)
{
    errhndk_errorObjects_g.cnLossSoc.thresholdCnt = thresholdCnt_p;
}

//------------------------------------------------------------------------------
/**
\brief  Set threshold counter of LossPreq error

\param  thresholdCnt_p        Threshold counter to set.

\ingroup module_errhndkcal
*/
//------------------------------------------------------------------------------
void errhndkcal_setLossPreqThresholdCnt(UINT32 thresholdCnt_p)
{
    errhndk_errorObjects_g.cnLossPreq.thresholdCnt = thresholdCnt_p;
}

//------------------------------------------------------------------------------
/**
\brief  Set threshold counter of CnCrc error

\param  thresholdCnt_p        Threshold counter to set.

\ingroup module_errhndkcal
*/
//------------------------------------------------------------------------------
void errhndkcal_setCnCrcThresholdCnt(UINT32 thresholdCnt_p)
{
    errhndk_errorObjects_g.cnCrcErr.thresholdCnt = thresholdCnt_p;
}

#ifdef CONFIG_INCLUDE_NMT_MN
//------------------------------------------------------------------------------
/**
\brief  Set threshold counter of MnCrc error

\param  thresholdCnt_p        Threshold counter to set.

\ingroup module_errhndkcal
*/
//------------------------------------------------------------------------------
void errhndkcal_setMnCrcThresholdCnt(UINT32 thresholdCnt_p)
{
    errhndk_errorObjects_g.mnCrcErr.thresholdCnt = thresholdCnt_p;
}

//------------------------------------------------------------------------------
/**
\brief  Set threshold counter of MnCycTimeExceed error

\param  thresholdCnt_p        Threshold counter to set.

\ingroup module_errhndkcal
*/
//------------------------------------------------------------------------------
void errhndkcal_setMnCycTimeExceedThresholdCnt(UINT32 thresholdCnt_p)
{
    errhndk_errorObjects_g.mnCycTimeExceed.thresholdCnt = thresholdCnt_p;
}

//------------------------------------------------------------------------------
/**
\brief  Set threshold counter of MnCnLossPres error

\param  nodeIdx_p             Index of node (node ID - 1).
\param  thresholdCnt_p      Threshold counter to set.

\ingroup module_errhndkcal
*/
//------------------------------------------------------------------------------
void errhndkcal_setMnCnLossPresThresholdCnt(UINT nodeIdx_p,
                                            UINT32 thresholdCnt_p)
{
    errhndk_errorObjects_g.aMnCnLossPres[nodeIdx_p].thresholdCnt = thresholdCnt_p;
}
#endif

//============================================================================//
//            P R I V A T E   F U N C T I O N S                               //
//============================================================================//
/// \name Private Functions
/// \{

///\}
