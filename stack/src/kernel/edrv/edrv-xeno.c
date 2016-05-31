/**
********************************************************************************
\file   edrv-xeno.c

\brief  Implementation of Xenomai Ethernet driver

This file contains the implementation of the Xenomai Ethernet driver

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
#define MSGQOBJ_NAME    	"/edrv-queue"
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
    u_char*		buffer;				    ///< Buffer to store incoming packets
    pthread_t           hThread;                            ///< Handle of the packetHandler
    mqd_t 		msgq_id;			    ///< ID of the message queue
} tEdrvInstance;

//------------------------------------------------------------------------------
// local vars
//------------------------------------------------------------------------------
static tEdrvInstance edrvInstance_l;

//------------------------------------------------------------------------------
// local function prototypes
//------------------------------------------------------------------------------
static void* packetHandler(void* pArgument_p);
static void getMacAdrs(const char* pIfName_p, UINT8* pMacAddr_p);
static tOplkError open_live(tEdrvInstance* pInstance);
static int read_packet(tEdrvInstance* pInstance);
static int send_packet(u_char *buf, int size);

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
    tOplkError 		ret = kErrorOk;
    struct sched_param 	schedParam;
    struct mq_attr 	attr, *pattr;

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
    * Init packet handler
    */
    if (sem_init(&edrvInstance_l.syncSem, 0, 0) != 0)
    {
        DEBUG_LVL_ERROR_TRACE("%s() couldn't init semaphore syncSem\n", __func__);
        ret = kErrorEdrvInit;
        goto Exit;
    }

    // message queue to store outcoming messages
    attr.mq_msgsize = EDRV_MAX_FRAME_SIZE;
    attr.mq_maxmsg = 128;
    pattr = &attr;
    edrvInstance_l.msgq_id = mq_open(MSGQOBJ_NAME, O_RDWR|O_CREAT|O_EXCL|O_NONBLOCK, NULL, pattr);
    if(edrvInstance_l.msgq_id == -1){
        DEBUG_LVL_ERROR_TRACE("%s() Couldn't create EDRV message queue\n", __func__);
        ret = kErrorEdrvInit;
        goto Exit;
    }

    if (pthread_create(&edrvInstance_l.hThread, NULL,
                       packetHandler,  &edrvInstance_l) != 0)
    {
        DEBUG_LVL_ERROR_TRACE("%s() Couldn't create packetHandler\n", __func__);
        ret = kErrorEdrvInit;
        goto Exit;
    }

    schedParam.__sched_priority = CONFIG_THREAD_PRIORITY_MEDIUM;
    if (pthread_setschedparam(edrvInstance_l.hThread, SCHED_FIFO, &schedParam) != 0)
    {
        DEBUG_LVL_ERROR_TRACE("%s() couldn't set packetHandler scheduling parameters\n",
                                __func__);
    }

    pthread_setname_np(edrvInstance_l.hThread, "oplk-edrv");

    sem_wait(&edrvInstance_l.syncSem);

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
    if(edrvInstance_l.hThread != 0){
	pthread_cancel(edrvInstance_l.hThread);
        pthread_join(edrvInstance_l.hThread, NULL);
    }

    if(&edrvInstance_l.mutex != NULL)
        pthread_mutex_destroy(&edrvInstance_l.mutex);

    if(edrvInstance_l.msgq_id != 0){
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

This function sends the Tx buffer. Outgoing packets are put in a message queue
which is read by the packetHandler.

\param  pBuffer_p           Tx buffer descriptor

\return The function returns a tOplkError error code.

\ingroup module_edrv
*/
//------------------------------------------------------------------------------
tOplkError edrv_sendTxBuffer(tEdrvTxBuffer* pBuffer_p)
{
    tOplkError 		ret = kErrorOk;
    int 		err;

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
    tOplkError 		ret = kErrorOk;

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
    UINT8* 		pBuffer = pBuffer_p->pBuffer;

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
\brief  Edrv packet Handler

This function keeps reading packets (socket and message queue) until an error occurs.

\param  pArgument_p     User specific pointer pointing to the instance structure
*/
//------------------------------------------------------------------------------
static void* packetHandler(void* pArgument_p)
{
    tEdrvInstance* 	pInstance = (tEdrvInstance*)pArgument_p;
    tOplkError 		ret;
    register int 	n;

    ret = open_live(pInstance);
    if (ret != kErrorOk) {
	DEBUG_LVL_EDRV_TRACE("%s(), error open_live\n",  __func__);
        return NULL;
    }

    /* signal that thread is successfully started */
    sem_post(&pInstance->syncSem);

    /* start main loop */
    do {
	n = read_packet(pInstance);
    } while (n == 0);

    DEBUG_LVL_EDRV_TRACE("%s() ended because of an error\n", __func__);
    return NULL;
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

This function opens a new socket, and binds it to the desired interface. It also
allocates memory to store incoming packets.

\param  pInstance  User specific pointer pointing to the instance structure

\return The function returns an error code.
*/
//------------------------------------------------------------------------------
static tOplkError open_live(tEdrvInstance* pInstance)
{
    struct sockaddr_ll 	sa;
    struct ifreq 	ifr;

    pInstance->sockfd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_POWERLINK));
    if (pInstance->sockfd < 0) {
        DEBUG_LVL_ERROR_TRACE("%s() error creating socket, maybe you forgot CONFIG_XENO_DRIVERS_NET_ETH_P_ALL in the kernel configuration\n", __func__);
        return kErrorEdrvInit;
    }

    // Bind the socket to the sdesired interface
    strncpy(ifr.ifr_name, pInstance->initParam.hwParam.pDevName, IFNAMSIZ);
    if (ioctl(pInstance->sockfd, SIOCGIFINDEX, &ifr) < 0) {
    	DEBUG_LVL_ERROR_TRACE("%s() error ioctl, can't find interface\n", __func__);
	close(pInstance->sockfd);
        return kErrorEdrvInit;
    }

    OPLK_MEMSET(&sa, 0x00, sizeof(sa));
    sa.sll_family = AF_PACKET;
    sa.sll_protocol = htons(ETH_P_POWERLINK);
    sa.sll_ifindex = ifr.ifr_ifindex;
    if (bind(pInstance->sockfd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
	DEBUG_LVL_ERROR_TRACE("%s() error bind, can't bind to interface\n", __func__);
        close(pInstance->sockfd);
        return kErrorEdrvInit;
    }

    // Allocate memory to store incoming packets
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
\brief  Read packets

This function reads incoming packets and messages from the queue, and forwards
them to the dllk.

\param  pInstance  User specific pointer pointing to the instance structure

\retval 0    	Keep going
\retval errno	An error occurs
*/
//------------------------------------------------------------------------------
static int read_packet(tEdrvInstance* pInstance)
{
    struct sockaddr_ll	from;
    int                 packet_len;
    struct iovec        iov;
    struct msghdr       msg;
    tEdrvRxBuffer   	rxBuffer;
    fd_set		readFds;
    int			max_fd;
    struct timeval      timeout;
    char 		buff[EDRV_MAX_FRAME_SIZE];

    iov.iov_len = SNAPLEN;
    iov.iov_base = pInstance->buffer;

    OPLK_MEMSET(&msg, 0, sizeof(msg));
    msg.msg_name = &from;
    msg.msg_namelen = sizeof(from);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    FD_ZERO(&readFds);
    FD_SET(pInstance->sockfd, &readFds);
    FD_SET(pInstance->msgq_id, &readFds);

    max_fd = pInstance->sockfd > pInstance->msgq_id ? pInstance->sockfd : pInstance->msgq_id;

    timeout.tv_sec = 0;
    timeout.tv_usec = 1000;

    packet_len = select(max_fd + 1, &readFds, NULL, NULL, &timeout);

    if (packet_len <= 0) {
	if ((packet_len == 0) || (errno == EINTR))//timeout or interrupted syscall
            return 0;
	else { //error
            DEBUG_LVL_EDRV_TRACE("select error: %s\n", strerror(errno));
            return errno;
	}
    }

    if (FD_ISSET(pInstance->sockfd, &readFds)){
	packet_len = recvmsg(pInstance->sockfd, &msg, 0);

        rxBuffer.bufferInFrame = kEdrvBufferLastInFrame;
        rxBuffer.rxFrameSize = packet_len - 4; //-4 is to substract the CRC bytes (Beaglebone only)
        rxBuffer.pBuffer = (UINT8*)pInstance->buffer;

        pInstance->initParam.pfnRxHandler(&rxBuffer);
    }

    if (FD_ISSET(pInstance->msgq_id, &readFds)){
	packet_len = mq_receive(pInstance->msgq_id, buff, EDRV_MAX_FRAME_SIZE, NULL);

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
    } // if (FD_ISSET(pInstance->msgq_id, &readFds))

    return 0;
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
    struct msghdr 	msg;
    struct iovec 	iov;

    iov.iov_base = buf;
    iov.iov_len = size;

    OPLK_MEMSET(&msg, 0, sizeof(msg));
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    return sendmsg(edrvInstance_l.sockfd, &msg, 0);
}
///\}
