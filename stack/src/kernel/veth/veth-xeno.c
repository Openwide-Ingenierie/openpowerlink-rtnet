/**
********************************************************************************
\file   veth-xeno.c

\brief  Implementation of virtual Ethernet for Xenomai

This file contains the the virtual Ethernet driver for the Linux userspace
implementation. It uses a TUN/TAP device as virtual ethernet driver and the XDDP
protocol for the communication between Xenomai threads and the TAP interface.

\ingroup module_veth
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
#include <kernel/veth.h>
#include <kernel/dllk.h>
#include <kernel/dllkcal.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <netinet/if_ether.h>

#include <rtdm/ipc.h>
//============================================================================//
//            G L O B A L   D E F I N I T I O N S                             //
//============================================================================//

//------------------------------------------------------------------------------
// const defines
//------------------------------------------------------------------------------
#define TUN_DEV_NAME        "/dev/net/tun"

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
#define XDDP_PORT 0

//------------------------------------------------------------------------------
// local types
//------------------------------------------------------------------------------
/**
\brief Structure describing an instance of the Virtual Ethernet driver

This structure describes an instance of the Virtual Ethernet driver in Linux
userspace.
*/
typedef struct
{
    UINT8               macAdrs[6];         ///< MAC address of the VEth interface
    UINT8               tapMacAdrs[6];      ///< MAC address of the TAP device
    int                 fd;                 ///< File descriptor of the tunnel device
    BOOL                fStop;              ///< Flag indicating whether the receive thread shall be stopped
    pthread_t           threadHandle;       ///< Handle of the receive thread
    pthread_t           threadLinux;        ///< Linux thread to handle tap interface 
    int 		sock_fd;            ///< Realtime XDDP endpoint
} tVethInstance;

//------------------------------------------------------------------------------
// local vars
//------------------------------------------------------------------------------
static tVethInstance        vethInstance_l;

//------------------------------------------------------------------------------
// local function prototypes
//------------------------------------------------------------------------------
static void getMacAdrs(UINT8* pMac_p);
static tOplkError veth_receiveFrame(tFrameInfo* pFrameInfo_p,
                                    tEdrvReleaseRxBuffer* pReleaseRxBuffer_p);
static void* vethRecvThread(void* pArg_p);
static void* vethLinuxThread(void* pArg_p);

//------------------------------------------------------------------------------
// local vars
//------------------------------------------------------------------------------

//============================================================================//
//            P U B L I C   F U N C T I O N S                                 //
//============================================================================//

//------------------------------------------------------------------------------
/**
\brief  Add virtual Ethernet instance

The function adds a virtual Ethernet instance.

\param  aSrcMac_p       MAC address to set for virtual Ethernet interface.

\return The function returns a tOplkError error code.

\ingroup module_veth
*/
//------------------------------------------------------------------------------
tOplkError veth_addInstance(const UINT8 aSrcMac_p[6])
{
    tOplkError          ret = kErrorOk;
    struct ifreq        ifr;
    int                 err;
    struct sockaddr_ipc saddr;
    size_t 		poolsz;
    struct sched_param  param = { .sched_priority = 20 };
    pthread_attr_t 	attr;
    struct timeval 	timeout;

    vethInstance_l.threadHandle = 0;
    vethInstance_l.threadLinux = 0;

    if ((vethInstance_l.fd = open(TUN_DEV_NAME, O_RDWR)) < 0)
    {
        DEBUG_LVL_VETH_TRACE("Error opening %s\n", TUN_DEV_NAME);
        return kErrorNoFreeInstance;
    }

    OPLK_MEMSET(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    strncpy(ifr.ifr_name, PLK_VETH_NAME, IFNAMSIZ);

    if ((err = ioctl(vethInstance_l.fd, TUNSETIFF, (void*)&ifr)) < 0)
    {
        DEBUG_LVL_VETH_TRACE("Error setting TUN IFF options\n");
        close(vethInstance_l.fd);
        return err;
    }

    // save MAC address of TAP device and Ethernet device to be able to
    // exchange them
    OPLK_MEMCPY(vethInstance_l.macAdrs, aSrcMac_p, 6);
    getMacAdrs(vethInstance_l.tapMacAdrs);

    // set up RT XDDP endpoint
    // get a datagram socket to bind to the RT endpoint
    vethInstance_l.sock_fd = socket(AF_RTIPC, SOCK_DGRAM, IPCPROTO_XDDP);
    if (vethInstance_l.sock_fd < 0) {
        DEBUG_LVL_ERROR_TRACE("Error creating socket: %s, maybe you forgot CONFIG_XENO_DRIVERS_RTIPC in the kernel configuration\n", strerror(errno));
        return vethInstance_l.sock_fd;
    }

    // set a local 16k pool for the RT endpoint. Memory needed to convey 
    // datagrams will be pulled from this pool, instead of Xenomai's system pool
    poolsz = 16384; /* bytes */
    err = setsockopt(vethInstance_l.sock_fd, SOL_XDDP, XDDP_POOLSZ,
                         &poolsz, sizeof(poolsz));
    if (err){
        DEBUG_LVL_ERROR_TRACE("setsockopt error: %s\n", strerror(errno));
    }

    // set a read timeout
    timeout.tv_sec = 0;
    timeout.tv_usec = 400000; //400ms
    err = setsockopt (vethInstance_l.sock_fd, SOL_SOCKET, SO_RCVTIMEO, 
			(char *)&timeout, sizeof(timeout));
    if (err){
        DEBUG_LVL_ERROR_TRACE("setsockopt error: %s\n", strerror(errno));
    }

    // bind the socket to the port, to setup a proxy to channel
    // traffic to/from the Linux domain.
     memset(&saddr, 0, sizeof(saddr));
     saddr.sipc_family = AF_RTIPC;
     saddr.sipc_port = XDDP_PORT;
     err = bind(vethInstance_l.sock_fd, (struct sockaddr *)&saddr, sizeof(saddr));
     if (err) {
        DEBUG_LVL_ERROR_TRACE("bind error: %s\n", strerror(errno));
        return err;
    }

    // start tap receive thread
    vethInstance_l.fStop = FALSE;
    if (pthread_create(&vethInstance_l.threadHandle, NULL, vethRecvThread, (void*)&vethInstance_l) != 0)
        return kErrorNoFreeInstance;

    pthread_setname_np(vethInstance_l.threadHandle, "oplk-veth");

    // register callback function in DLL
    ret = dllk_regAsyncHandler(veth_receiveFrame);

    //Linux thread to handle tap interface
    pthread_attr_init(&attr);
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    pthread_attr_setschedparam(&attr, &param);
    if(__STD(pthread_create(&vethInstance_l.threadLinux, &attr, vethLinuxThread, (void*)&vethInstance_l)) != 0)
	return kErrorNoFreeInstance;

    return ret;
}

//------------------------------------------------------------------------------
/**
\brief  Delete virtual Ethernet instance

The function deletes a virtual ethernet instance.

\return The function returns a tOplkError error code.

\ingroup module_veth
*/
//------------------------------------------------------------------------------
tOplkError veth_delInstance(void)
{
    // stop receive thread by setting its stop flag
    vethInstance_l.fStop = TRUE;
    if(vethInstance_l.threadHandle != 0)
	pthread_join(vethInstance_l.threadHandle, NULL);
    if(vethInstance_l.threadLinux != 0)
        pthread_join(vethInstance_l.threadLinux, NULL);
    close (vethInstance_l.fd);
    close (vethInstance_l.sock_fd);
    return kErrorOk;
}


//============================================================================//
//            P R I V A T E   F U N C T I O N S                               //
//============================================================================//
/// \name Private Functions
/// \{

//------------------------------------------------------------------------------
/**
\brief  Get MAC address of veth interface

The function reads the MAC address of the virtual Ethernet interface.

\param  pMac_p        Pointer to store the MAC address
*/
//------------------------------------------------------------------------------
static void getMacAdrs(UINT8* pMac_p)
{
    struct ifreq    ifr;
    int             sock;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        DEBUG_LVL_VETH_TRACE("Cannot open udp socket for MAC reading: %s\n",
                              strerror(errno));
        return;
    }

    OPLK_MEMSET(&ifr, 0, sizeof(struct ifreq));
    strncpy(ifr.ifr_name, "plk", IFNAMSIZ);

    if (ioctl(sock, SIOCGIFHWADDR, &ifr) < 0)
    {
        DEBUG_LVL_VETH_TRACE("Cannot get MAC adress: '%s' (%d)\n", strerror(errno), errno);
    }

    DEBUG_LVL_VETH_TRACE ("Get Mac addr %02x:%02x:%02x:%02x:%02x:%02x\n",
           ifr.ifr_hwaddr.sa_data[0], ifr.ifr_hwaddr.sa_data[1],
           ifr.ifr_hwaddr.sa_data[2], ifr.ifr_hwaddr.sa_data[3],
           ifr.ifr_hwaddr.sa_data[4], ifr.ifr_hwaddr.sa_data[5]);

    close(sock);

    OPLK_MEMCPY(pMac_p, &ifr.ifr_hwaddr.sa_data[0], ETHER_ADDR_LEN);
}

//------------------------------------------------------------------------------
/**
\brief  Receive frame from virtual Ethernet interface

The function receives a non POWERLINK frame from the network. It fowards it 
to the vethLinuxThread in order to be write to the virtual Ethernet interface.

\param  pFrameInfo_p        Pointer to frame information of received frame.
\param  pReleaseRxBuffer_p  Pointer to buffer release flag. The function must
                            set this flag to determine if the RxBuffer could be
                            released immediately.

\return The function returns a tOplkError error code.
*/
//------------------------------------------------------------------------------
static tOplkError veth_receiveFrame(tFrameInfo* pFrameInfo_p,
                                    tEdrvReleaseRxBuffer* pReleaseRxBuffer_p)
{
    UINT            nwrite;

    // replace the MAC address of the POWERLINK Ethernet interface with virtual
    // Ethernet MAC address before forwarding it into the virtual Ethernet interface
    if (OPLK_MEMCMP(pFrameInfo_p->pFrame->aDstMac, vethInstance_l.macAdrs, ETHER_ADDR_LEN) == 0)
    {
        OPLK_MEMCPY(pFrameInfo_p->pFrame->aDstMac, vethInstance_l.tapMacAdrs, ETHER_ADDR_LEN);
    }
    
    nwrite = sendto(vethInstance_l.sock_fd, pFrameInfo_p->pFrame, pFrameInfo_p->frameSize, 0, NULL, 0);
    if (nwrite != pFrameInfo_p->frameSize)
    {
        DEBUG_LVL_VETH_TRACE("Error forwarding data to virtual Ethernet interface!\n");
    }

    *pReleaseRxBuffer_p = kEdrvReleaseRxBufferImmediately;

    return kErrorOk;
}

//------------------------------------------------------------------------------
/**
\brief  Receive frame from virtual Ethernet interface

The function receives a frame from the virtual Ethernet interface. It is implemented
to be used as a thread which does a blocking read in a while loop.

\param  pArg_p        Thread argument. Pointer to virtual ethernet instance.

\return The function returns a tOplkError error code.
*/
//------------------------------------------------------------------------------
static void* vethRecvThread(void *pArg_p)
{
    tVethInstance*      pInstance = (tVethInstance*)pArg_p;
    UINT8               buffer[ETHERMTU];
    tFrameInfo          frameInfo;
    int                 nread;
    tOplkError          ret = kErrorOk;

    while (!pInstance->fStop)
    {
        nread = recvfrom(pInstance->sock_fd, buffer, sizeof(buffer), 0, NULL, 0);
	// Check if an error occured
     	if (nread == -1) {
            switch (errno) {
                case ETIMEDOUT:
                    continue; // no packet there

                default:
                    DEBUG_LVL_VETH_TRACE("recvfrom error: %s\n", strerror(errno));
                    continue;
            }
        }

        frameInfo.pFrame = (tPlkFrame *)buffer;
        frameInfo.frameSize = nread;
        ret = dllkcal_sendAsyncFrame(&frameInfo, kDllAsyncReqPrioGeneric);
        if (ret != kErrorOk) {
            DEBUG_LVL_VETH_TRACE("veth_xmit: dllkcal_sendAsyncFrame returned 0x%02X\n", ret);
        }
    }
    
    pthread_exit(NULL);
}

//------------------------------------------------------------------------------
/**
\brief  Receive frame from virtual Ethernet interface

This thread is a Linux one. It acts as an intermediary between Xenomai threads 
and the virtual Ethernet interface.
It handles Ethernet frames sent by user space programs and non POWERLINK frames received
from the network.
It is implemented to be used as a thread which does a blocking read in a while loop.

\param  pArg_p        Thread argument. Pointer to virtual ethernet instance.

\return The function returns a tOplkError error code.
*/
//------------------------------------------------------------------------------
static void* vethLinuxThread(void* pArg_p)
{
    UINT8               buffer[ETHERMTU];
    UINT                nread, nwrite;
    tVethInstance*      pInstance = (tVethInstance*)pArg_p;
    fd_set              readFds;
    int                 result;
    struct timeval      timeout;
    char		*devname;
    int 		fd, max_fd;

    // set up NRT XDDP endpoint
    if (asprintf(&devname, "/dev/rtp%d", XDDP_PORT) < 0) {
	DEBUG_LVL_VETH_TRACE("asprintf error: %s\n", strerror(errno));
	pthread_exit(NULL);
    }

    fd = open(devname, O_RDWR);
    free(devname);
    if (fd < 0) {
	DEBUG_LVL_VETH_TRACE("open error: %s\n", strerror(errno));
	close(fd);
	pthread_exit(NULL);
    }

    max_fd = pInstance->fd > fd ? pInstance->fd : fd;

    while (!pInstance->fStop)
    {
        FD_ZERO(&readFds);
        FD_SET(pInstance->fd, &readFds);
	FD_SET(fd, &readFds);
	
	timeout.tv_sec = 0;
        timeout.tv_usec = 400000;

	//  both descriptors are Linux ones
        result = __STD(select(max_fd + 1, &readFds, NULL, NULL, &timeout));

	if (result == 0) //timeout
	    continue;

	if (result == -1) { //error
	    DEBUG_LVL_VETH_TRACE("select error: %s\n", strerror(errno));
	    continue;
	}	

	// we have just received an Ethernet packet from a user space program
	if (FD_ISSET(pInstance->fd, &readFds)){
	    nread = read(pInstance->fd, buffer, ETHERMTU);
	    if (nread > 0) {
                DEBUG_LVL_VETH_TRACE("VETH:Read %d bytes from the tap interface\n", nread);
                DEBUG_LVL_VETH_TRACE("SRC MAC: %02X:%02X:%02x:%02X:%02X:%02x\n",
                                      buffer[6], buffer[7], buffer[8], buffer[9], buffer[10], buffer[11]);
                DEBUG_LVL_VETH_TRACE("DST MAC: %02X:%02X:%02x:%02X:%02X:%02x\n",
                                      buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);
               
		// replace src MAC address with MAC address of virtual Ethernet interface
                OPLK_MEMCPY(&buffer[6], pInstance->macAdrs, ETHER_ADDR_LEN);

		nwrite = write(fd, buffer, nread);
                if (nwrite != nread) {
                    DEBUG_LVL_VETH_TRACE("write error: %s\n", strerror(errno));
		}
            }
	}

	// we have just received an Ethernet packet from the network
	if (FD_ISSET(fd, &readFds)){
	    nread = read(fd, buffer, ETHERMTU);
            if (nread > 0){
            	nwrite = write(pInstance->fd, buffer, nread);
    		if (nwrite != nread) {
        	    DEBUG_LVL_VETH_TRACE("Error writing data to virtual Ethernet interface!\n");
    		}
	    }
	}
    }

    close(fd);
    pthread_exit(NULL);
}

///\}
