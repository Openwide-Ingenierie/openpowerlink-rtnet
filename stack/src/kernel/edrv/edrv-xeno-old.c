/**
********************************************************************************
\file   edrv-linux-xeno.c

\brief  Implementation of Xenomai Ethernet driver

This file contains the implementation of the xenomai Ethernet driver

\ingroup module_edrv
*******************************************************************************/

/*------------------------------------------------------------------------------
Copyright (c) 2014, Bernecker+Rainer Industrie-Elektronik Ges.m.b.H. (B&R)
Copyright (c) 2013, Kalycito Infotech Private Limited
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
#include <common/ftracedebug.h>
#include <kernel/edrv.h>

#include <unistd.h>
#include <string.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/syscall.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>

#include <errno.h>
#include <netpacket/packet.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <rtnet.h>
#include <mqueue.h>
#include <time.h>
//============================================================================//
//            G L O B A L   D E F I N I T I O N S                             //
//============================================================================//

//------------------------------------------------------------------------------
// const defines
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// module global vars
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// global function prototypes
//------------------------------------------------------------------------------

//============================================================================//
//            P R I V A T E   D E F I N I T I O N S                           //
//============================================================================//

//------------------------------------------------------------------------------
// const defines
//------------------------------------------------------------------------------
#define EDRV_MAX_FRAME_SIZE     0x600
#define SNAPLEN			65535
#define MSGQOBJ_NAME    	"/tx-queue"
#define ETH_P_POWERLINK		ETH_P_ALL

//------------------------------------------------------------------------------
// local types
//------------------------------------------------------------------------------
/**
\brief Structure describing an instance of the Edrv

This structure describes an instance of the Ethernet driver.
*/
typedef struct
{
    tEdrvInitParam      initParam;                          ///< Init parameters
    tEdrvTxBuffer*      pTransmittedTxBufferLastEntry;      ///< Pointer to the last entry of the transmitted TX buffer
    tEdrvTxBuffer*      pTransmittedTxBufferFirstEntry;     ///< Pointer to the first entry of the transmitted Tx buffer
    pthread_mutex_t     mutex;                              ///< Mutex for locking of critical sections
    sem_t               syncSem;                            ///< Semaphore for signalling the start of the worker thread
    int 		sockfd;				    ///< Socket descriptor
    int			ifindex;			    ///< Socket index
    u_char*		buffer;				    ///< Buffer to store the incoming packet
    pthread_t           rxThread;                           ///< Handle of the rx thread
    pthread_t		txThread;			    ///< Handle of the tx thread 
    mqd_t 		msgq_id;			    ///< ID of the message queue
} tEdrvInstance;

//------------------------------------------------------------------------------
// local vars
//------------------------------------------------------------------------------
static tEdrvInstance edrvInstance_l;

//------------------------------------------------------------------------------
// local function prototypes
//------------------------------------------------------------------------------
static void* rxThread(void* pArgument_p);
static void* txThread(void* pArgument_p);
static void getMacAdrs(const char* pIfName_p, UINT8* pMacAddr_p);
static tOplkError open_live(tEdrvInstance* pInstance, nanosecs_rel_t timeout);
static int read_packet(tEdrvInstance* pInstance);
static int send_packet(u_char *buf, int size);
static int read_queue(tEdrvInstance* pInstance);

//============================================================================//
//            P U B L I C   F U N C T I O N S                                 //
//============================================================================//

//------------------------------------------------------------------------------
/**
\brief  Ethernet driver initialization

This function initializes the Ethernet driver.

\param  pEdrvInitParam_p    Edrv initialization parameters

\return The function returns a tOplkError error code.

\ingroup module_edrv
*/
//------------------------------------------------------------------------------
tOplkError edrv_init(tEdrvInitParam* pEdrvInitParam_p)
{
    tOplkError ret = kErrorOk;
    struct sched_param schedParam;
    struct mq_attr attr, *pattr;

    // clear instance structure
    OPLK_MEMSET(&edrvInstance_l, 0, sizeof(edrvInstance_l));

    if (pEdrvInitParam_p->hwParam.pDevName == NULL || strcmp(pEdrvInitParam_p->hwParam.pDevName, "") == 0)
    {
        ret = kErrorEdrvInit;
        goto Exit;
    }

    /* if no MAC address was specified read MAC address of used
     * Ethernet interface
     */
    if ((pEdrvInitParam_p->aMacAddr[0] == 0) &&
        (pEdrvInitParam_p->aMacAddr[1] == 0) &&
        (pEdrvInitParam_p->aMacAddr[2] == 0) &&
        (pEdrvInitParam_p->aMacAddr[3] == 0) &&
        (pEdrvInitParam_p->aMacAddr[4] == 0) &&
        (pEdrvInitParam_p->aMacAddr[5] == 0))
    {   
	// read MAC address from controller
        getMacAdrs(pEdrvInitParam_p->hwParam.pDevName,
                   pEdrvInitParam_p->aMacAddr);
    }

    // save the init data (with updated MAC address)
    edrvInstance_l.initParam = *pEdrvInitParam_p;

    if (pthread_mutex_init(&edrvInstance_l.mutex, NULL) != 0)
    {
        DEBUG_LVL_ERROR_TRACE("%s() couldn't init mutex\n", __func__);
        ret = kErrorEdrvInit;
        goto Exit;
    }

   /*
    * Init Rx packet handler
    */
    if (sem_init(&edrvInstance_l.syncSem, 0, 0) != 0) 
    {
        DEBUG_LVL_ERROR_TRACE("%s() couldn't init semaphore syncSem\n", __func__);
        ret = kErrorEdrvInit;
        goto Exit;
    }

    if (pthread_create(&edrvInstance_l.rxThread, NULL,
                       rxThread,  &edrvInstance_l) != 0)
    {
        DEBUG_LVL_ERROR_TRACE("%s() Couldn't create rxThread!\n", __func__);
        ret = kErrorEdrvInit;
        goto Exit;
    }
 
    schedParam.__sched_priority = CONFIG_THREAD_PRIORITY_MEDIUM;
    if (pthread_setschedparam(edrvInstance_l.rxThread, SCHED_FIFO, &schedParam) != 0)
    {
        DEBUG_LVL_ERROR_TRACE("%s() Couldn't set rxthread scheduling parameters!\n",
                                __func__);
    }

    pthread_setname_np(edrvInstance_l.rxThread, "oplk-rx");

    sem_wait(&edrvInstance_l.syncSem);

   /*
    * Init Tx packet handler
    */
    attr.mq_msgsize = EDRV_MAX_FRAME_SIZE;
    attr.mq_maxmsg = 128;
    pattr = &attr;
    edrvInstance_l.msgq_id = mq_open(MSGQOBJ_NAME, O_RDWR|O_CREAT|O_EXCL|O_NONBLOCK, NULL, pattr);
    if(edrvInstance_l.msgq_id == -1){
        DEBUG_LVL_ERROR_TRACE("%s() Couldn't create tx-queue!\n", __func__);
        ret = kErrorEdrvInit;
        goto Exit;
    }

    if (pthread_create(&edrvInstance_l.txThread, NULL,
                       txThread,  &edrvInstance_l) != 0)
    {
        DEBUG_LVL_ERROR_TRACE("%s() Couldn't create txthread!\n", __func__);
        ret = kErrorEdrvInit;
        goto Exit;
    }

    if (pthread_setschedparam(edrvInstance_l.txThread, SCHED_FIFO, &schedParam) != 0)
    {
        DEBUG_LVL_ERROR_TRACE("%s() couldn't set txthread scheduling parameters!\n",
                                __func__);
    }

    pthread_setname_np(edrvInstance_l.txThread, "oplk-tx");

Exit:
    return ret;
}

//------------------------------------------------------------------------------
/**
\brief  Ethernet driver shutdown

This function shuts down the Ethernet driver.

\return The function returns a tOplkError error code.

\ingroup module_edrv
*/
//------------------------------------------------------------------------------
tOplkError edrv_shutdown(void)
{
    // signal shutdown to the threads
    if(edrvInstance_l.rxThread != 0){
	pthread_cancel(edrvInstance_l.rxThread);
        pthread_join(edrvInstance_l.rxThread, NULL);
    }

    if(edrvInstance_l.txThread != 0){
        pthread_cancel(edrvInstance_l.txThread);
        pthread_join(edrvInstance_l.txThread, NULL);
    }

    if(&edrvInstance_l.mutex != NULL)
        pthread_mutex_destroy(&edrvInstance_l.mutex);

    if(edrvInstance_l.msgq_id != 0)
	mq_close(edrvInstance_l.msgq_id);
	mq_unlink(MSGQOBJ_NAME);
    }

    if(edrvInstance_l.buffer != NULL){
        OPLK_FREE(edrvInstance_l.buffer);
        edrvInstance_l.buffer = NULL;
    }
    // clear instance structure
    OPLK_MEMSET(&edrvInstance_l, 0, sizeof(edrvInstance_l));

    return kErrorOk; //assuming no problems with closing the handle
}

//------------------------------------------------------------------------------
/**
\brief  Get MAC address

This function returns the MAC address of the Ethernet controller

\return The function returns a pointer to the MAC address.

\ingroup module_edrv
*/
//------------------------------------------------------------------------------
UINT8* edrv_getMacAddr(void)
{
    return edrvInstance_l.initParam.aMacAddr;
}

//------------------------------------------------------------------------------
/**
\brief  Send Tx buffer

This function sends the Tx buffer. As we can't capture outgoing packets, these are
put in a message queue which is further read by the tx thread.

\param  pBuffer_p           Tx buffer descriptor

\return The function returns a tOplkError error code.

\ingroup module_edrv
*/
//------------------------------------------------------------------------------
tOplkError edrv_sendTxBuffer(tEdrvTxBuffer* pBuffer_p)
{
    tOplkError  ret = kErrorOk;
    int err;

    FTRACE_MARKER("%s", __func__);

    if (pBuffer_p->txBufferNumber.pArg != NULL)
    {
        ret = kErrorInvalidOperation;
        goto Exit;
    }

    pthread_mutex_lock(&edrvInstance_l.mutex);
    if (edrvInstance_l.pTransmittedTxBufferLastEntry == NULL)
    {
        edrvInstance_l.pTransmittedTxBufferLastEntry =
           edrvInstance_l.pTransmittedTxBufferFirstEntry = pBuffer_p;
    }
    else
    {
       edrvInstance_l.pTransmittedTxBufferLastEntry->txBufferNumber.pArg = pBuffer_p;
       edrvInstance_l.pTransmittedTxBufferLastEntry = pBuffer_p;
    }
    pthread_mutex_unlock(&edrvInstance_l.mutex);

    err = send_packet(pBuffer_p->pBuffer, (INT)pBuffer_p->txFrameSize);
    if(err != (INT)pBuffer_p->txFrameSize)
    {
        if(err == 1)
                DEBUG_LVL_EDRV_TRACE("%s() error send_packet, link is down\n",  __func__);
        else
                DEBUG_LVL_EDRV_TRACE("%s() error send_packet: %d\n", __func__, err);

        ret = kErrorInvalidOperation;
        goto Exit;

    }

    if (mq_send(edrvInstance_l.msgq_id, (const char*)pBuffer_p->pBuffer, (INT)pBuffer_p->txFrameSize, 0) == -1){
	DEBUG_LVL_EDRV_TRACE("%s() error mq_send\n", __func__);
    }

Exit:
    return ret;
}

//------------------------------------------------------------------------------
/**
\brief  Allocate Tx buffer

This function allocates a Tx buffer.

\param  pBuffer_p           Tx buffer descriptor

\return The function returns a tOplkError error code.

\ingroup module_edrv
*/
//------------------------------------------------------------------------------
tOplkError edrv_allocTxBuffer(tEdrvTxBuffer* pBuffer_p)
{
    tOplkError ret = kErrorOk;

    if (pBuffer_p->maxBufferSize > EDRV_MAX_FRAME_SIZE)
    {
        ret = kErrorEdrvNoFreeBufEntry;
        goto Exit;
    }

    // allocate buffer with malloc
    pBuffer_p->pBuffer = (UINT8*)OPLK_MALLOC(pBuffer_p->maxBufferSize);
    if (pBuffer_p->pBuffer == NULL)
    {
        ret = kErrorEdrvNoFreeBufEntry;
        goto Exit;
    }

    pBuffer_p->txBufferNumber.pArg = NULL;

Exit:
    return ret;
}

//------------------------------------------------------------------------------
/**
\brief  Free Tx buffer

This function releases the Tx buffer.

\param  pBuffer_p           Tx buffer descriptor

\return The function returns a tOplkError error code.

\ingroup module_edrv
*/
//------------------------------------------------------------------------------
tOplkError edrv_freeTxBuffer(tEdrvTxBuffer* pBuffer_p)
{
    UINT8* pBuffer = pBuffer_p->pBuffer;

    // mark buffer as free, before actually freeing it
    pBuffer_p->pBuffer = NULL;

    OPLK_FREE(pBuffer);

    return kErrorOk;
}

//------------------------------------------------------------------------------
/**
\brief  Change Rx filter setup

This function changes the Rx filter setup. The parameter entryChanged_p
selects the Rx filter entry that shall be changed and \p changeFlags_p determines
the property.
If \p entryChanged_p is equal or larger count_p all Rx filters shall be changed.

\note Rx filters are not supported by this driver!

\param  pFilter_p           Base pointer of Rx filter array
\param  count_p             Number of Rx filter array entries
\param  entryChanged_p      Index of Rx filter entry that shall be changed
\param  changeFlags_p       Bit mask that selects the changing Rx filter property

\return The function returns a tOplkError error code.

\ingroup module_edrv
*/
//------------------------------------------------------------------------------
tOplkError edrv_changeRxFilter(tEdrvFilter* pFilter_p, UINT count_p,
                               UINT entryChanged_p, UINT changeFlags_p)
{
    UNUSED_PARAMETER(pFilter_p);
    UNUSED_PARAMETER(count_p);
    UNUSED_PARAMETER(entryChanged_p);
    UNUSED_PARAMETER(changeFlags_p);

    return kErrorOk;
}

//------------------------------------------------------------------------------
/**
\brief  Clear multicast address entry

This function removes the multicast entry from the Ethernet controller.

\param  pMacAddr_p  Multicast address

\return The function returns a tOplkError error code.

\ingroup module_edrv
*/
//------------------------------------------------------------------------------
tOplkError edrv_clearRxMulticastMacAddr(UINT8* pMacAddr_p)
{
    UNUSED_PARAMETER(pMacAddr_p);

    return kErrorOk;
}

//------------------------------------------------------------------------------
/**
\brief  Set multicast address entry

This function sets a multicast entry into the Ethernet controller.

\param  pMacAddr_p  Multicast address

\return The function returns a tOplkError error code.

\ingroup module_edrv
*/
//------------------------------------------------------------------------------
tOplkError edrv_setRxMulticastMacAddr(UINT8* pMacAddr_p)
{
    UNUSED_PARAMETER(pMacAddr_p);

    return kErrorOk;
}

//============================================================================//
//            P R I V A T E   F U N C T I O N S                               //
//============================================================================//
/// \name Private Functions
/// \{
//------------------------------------------------------------------------------
/**
\brief  Edrv rx thread

This function keeps reading incoming packets until an error occurs

\param  pArgument_p     User specific pointer pointing to the instance structure

\return The function returns NULL
*/
//------------------------------------------------------------------------------
static void* rxThread(void* pArgument_p)
{
    tEdrvInstance* pInstance = (tEdrvInstance*)pArgument_p;
    int ret;
    register int n;

    ret = open_live(pInstance, 1000000); //nanosecond read timeout: 1ms
    if (ret != kErrorOk) {
	DEBUG_LVL_ERROR_TRACE("%s() error open_live: %d\n",  __func__, ret);
        return NULL;
    }

    /* signal that thread is successfully started */
    sem_post(&pInstance->syncSem);

    for (;;) {
        do {
            n = read_packet(pInstance);
        } while (n == 0);

        if (n < 0){
	    DEBUG_LVL_ERROR_TRACE("%s() ended because of an error\n", __func__);
            return NULL;
	}
    }
}

//------------------------------------------------------------------------------
/**
\brief  Edrv tx thread

This function keeps reading the message queue until an error occurs

\param  pArgument_p     User specific pointer pointing to the instance structure

\return The function returns NULL
*/
//------------------------------------------------------------------------------
static void* txThread(void* pArgument_p)
{
    tEdrvInstance* pInstance = (tEdrvInstance*)pArgument_p;
    register int n;

    for (;;) {
        do {
            n = read_queue(pInstance);
        } while (n == 0);

        if (n < 0){
	    DEBUG_LVL_ERROR_TRACE("%s() ended because of an error\n", __func__);
            return NULL;
	}
    }
}

//------------------------------------------------------------------------------
/**
\brief  Read the message queue

This function is in charge of reading the message queue which contains messages sent on the
network, and forwarding them to the dllk.

\param  pInstance     User specific pointer pointing to the instance structure

\return code indicates if the message was forwarded
\retval 0    No message was there -> continue
\retval 1    A message was successfully forwarded to the dllk
\retval -errno   An error occurs
*/
//------------------------------------------------------------------------------
static int read_queue(tEdrvInstance* pInstance)
{
    int packet_len;
    struct timespec tm;
    char buff[EDRV_MAX_FRAME_SIZE];

    do {
	clock_gettime(CLOCK_REALTIME, &tm);
	tm.tv_nsec += 1000000;
	if(tm.tv_nsec >= 1000000000)
	{
		tm.tv_sec++;
		tm.tv_nsec -= 1000000000;
	}
	
        packet_len = mq_timedreceive(pInstance->msgq_id, buff, EDRV_MAX_FRAME_SIZE, NULL, &tm);
    } while (packet_len == -1 && errno == EINTR);

    // Check if an error occured
    if (packet_len == -1) {
        switch (errno) {
            case ETIMEDOUT:
                return 0; // no packet there

            default:
		DEBUG_LVL_ERROR_TRACE("%s(): Error mq_timedreceive\n", __func__);
                return -errno;
        }
    }

    if (pInstance->pTransmittedTxBufferFirstEntry != NULL)
    {
        tEdrvTxBuffer* pTxBuffer = pInstance->pTransmittedTxBufferFirstEntry;

        if (pTxBuffer->pBuffer != NULL)
        {
            if (OPLK_MEMCMP(buff, pTxBuffer->pBuffer, 6) == 0)
            {
                pthread_mutex_lock(&pInstance->mutex);
                pInstance->pTransmittedTxBufferFirstEntry =
                    pInstance->pTransmittedTxBufferFirstEntry->txBufferNumber.pArg;

                if (pInstance->pTransmittedTxBufferFirstEntry == NULL)
                {
                    pInstance->pTransmittedTxBufferLastEntry = NULL;
                }
                pthread_mutex_unlock(&pInstance->mutex);

                pTxBuffer->txBufferNumber.pArg = NULL;

                if (pTxBuffer->pfnTxHandler != NULL)
                {
                    pTxBuffer->pfnTxHandler(pTxBuffer);
                }
            }
	    else
            {
                    TRACE("%s: no matching TxB: DstMAC=%02X%02X%02X%02X%02X%02X\n",
                        __func__,
                        (UINT)buff[0],
                        (UINT)buff[1],
                        (UINT)buff[2],
                        (UINT)buff[3],
                        (UINT)buff[4],
                        (UINT)buff[5]);
                    TRACE("   current TxB %p: DstMAC=%02X%02X%02X%02X%02X%02X\n",
                        (void*)pTxBuffer,
                        (UINT)pTxBuffer->pBuffer[0],
                        (UINT)pTxBuffer->pBuffer[1],
                        (UINT)pTxBuffer->pBuffer[2],
                        (UINT)pTxBuffer->pBuffer[3],
                        (UINT)pTxBuffer->pBuffer[4],
                        (UINT)pTxBuffer->pBuffer[5]);
             }
        }
    }

    return 1;
}

//------------------------------------------------------------------------------
/**
\brief  Get Edrv MAC address

This function gets the interface's MAC address.

\param  pIfName_p   Ethernet interface device name
\param  pMacAddr_p  Pointer to store MAC address
*/
//------------------------------------------------------------------------------
static void getMacAdrs(const char* pIfName_p, UINT8* pMacAddr_p)
{
    INT             fd;
    struct ifreq    ifr;

    fd = socket(AF_INET, SOCK_DGRAM, 0);

    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name, pIfName_p, IFNAMSIZ - 1);

    ioctl(fd, SIOCGIFHWADDR, &ifr);

    close(fd);

    OPLK_MEMCPY(pMacAddr_p, ifr.ifr_hwaddr.sa_data, 6);
}


//------------------------------------------------------------------------------
/**
\brief  Open a socket

This function opens a new socket, and bind it to the desired interface. It also 
allocates memory to store incoming packets.

\param  pInstance  User specific pointer pointing to the instance structure
\param timeout	   Read timeout

\return The function returns an error code.
*/
//------------------------------------------------------------------------------
static tOplkError open_live(tEdrvInstance* pInstance, nanosecs_rel_t timeout)
{
    struct sockaddr_ll sa;
    struct ifreq ifr;

    pInstance->sockfd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_POWERLINK));

    strncpy(ifr.ifr_name, pInstance->initParam.hwParam.pDevName, IFNAMSIZ);

    if (ioctl(pInstance->sockfd, SIOCGIFINDEX, &ifr) < 0) {
    	DEBUG_LVL_ERROR_TRACE("%s() error ioctl, can't find interface\n", __func__);
	close(pInstance->sockfd);
        return kErrorEdrvInit;
    }
    pInstance->ifindex = ifr.ifr_ifindex;

    OPLK_MEMSET(&sa, 0x00, sizeof(sa));
    sa.sll_family = AF_PACKET;
    sa.sll_protocol = htons(ETH_P_POWERLINK);
    sa.sll_ifindex = ifr.ifr_ifindex;
    if (bind(pInstance->sockfd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
	DEBUG_LVL_ERROR_TRACE("%s() error bind, can't bind to local interface\n", __func__);
        close(pInstance->sockfd);
        return kErrorEdrvInit;
    }

    if (ioctl(pInstance->sockfd, RTNET_RTIOC_TIMEOUT, &timeout) < 0) {
	DEBUG_LVL_ERROR_TRACE("%s() error ioctl, can't set timeout\n", __func__);
        close(pInstance->sockfd);
        return kErrorEdrvInit;
    }

    //Allocate memory to store incoming packets
    pInstance->buffer = OPLK_MALLOC(SNAPLEN);
    if(pInstance->buffer == NULL) {
        DEBUG_LVL_ERROR_TRACE("%s() error malloc, can't allocate buffer\n", __func__);
        close(pInstance->sockfd);
        return kErrorEdrvInit;
    }

    return kErrorOk;
}

//------------------------------------------------------------------------------
/**
\brief  Read incoming packets

The function reads a packet from the socket and forwards it the to dllk.

\param  pInstance  User specific pointer pointing to the instance structure

\return	code indicates if the packet was forwarded
\retval 0    No packet was there or wrong interface index -> continue
\retval 1    A packet was successfully forwarded to the dllk
\retval -errno	An error occurs
*/
//------------------------------------------------------------------------------
static int read_packet(tEdrvInstance* pInstance)
{
    struct sockaddr_ll      from;
    int                     packet_len;
    struct iovec            iov;
    struct msghdr           msg;
    tEdrvRxBuffer   	    rxBuffer;

    iov.iov_len             = SNAPLEN;
    iov.iov_base            = pInstance->buffer;

    OPLK_MEMSET(&msg, 0, sizeof(msg));
    msg.msg_name            = &from;
    msg.msg_namelen         = sizeof(from);
    msg.msg_iov             = &iov;
    msg.msg_iovlen          = 1;

    do {
        packet_len = recvmsg(pInstance->sockfd, &msg, 0);
     } while (packet_len == -1 && errno == EINTR);

     // Check if an error occured
     if (packet_len == -1) {
          switch (errno) {
		case ETIMEDOUT:
		    return 0; // no packet there

		default:
	  	    DEBUG_LVL_ERROR_TRACE("%s(): Error recvfrom\n", __func__);
                    return -errno;
         }
    }

    // Check the interface index
    if (from.sll_ifindex != pInstance->ifindex)
    	return 0;

    if (packet_len > SNAPLEN)
        packet_len = SNAPLEN;

    rxBuffer.bufferInFrame = kEdrvBufferLastInFrame;
    rxBuffer.rxFrameSize = packet_len;
    rxBuffer.pBuffer = (UINT8*)pInstance->buffer;

    FTRACE_MARKER("%s RX", __func__);
    pInstance->initParam.pfnRxHandler(&rxBuffer);

    return 1;
} 

//------------------------------------------------------------------------------
/**
\brief  Send a packet

The function sends a packet.

\param  buf  Pointer to the data to be sent
\param  size Size of the data

\return the error code of the sendmsg function
*/
//------------------------------------------------------------------------------
static int send_packet(u_char *buf, int size)
{
    struct msghdr msg;
    struct iovec iov;

    iov.iov_base = buf;
    iov.iov_len = size;

    OPLK_MEMSET(&msg, 0, sizeof(msg));
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    
    return sendmsg(edrvInstance_l.sockfd, &msg, 0);
}
///\}
