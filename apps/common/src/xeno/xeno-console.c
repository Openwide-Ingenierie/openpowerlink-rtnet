/**
********************************************************************************
\file   xeno-console.c

\brief  Prompt the available real time interfaces

\ingroup module_app_common
*******************************************************************************/

/*------------------------------------------------------------------------------
Copyright (c) 2014, Bernecker+Rainer Industrie-Elektronik Ges.m.b.H. (B&R)
Copyright (c) 2013, SYSTEC electronic GmbH
Copyright (c) 2013, Kalycito Infotech Private Ltd.All rights reserved.
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
#include <stdio.h>
#include <string.h>
#include "xeno-console.h"

#include <netdb.h>
#include <linux/if.h>
#include <rtnet.h>
//============================================================================//
//            G L O B A L   D E F I N I T I O N S                             //
//============================================================================//

//------------------------------------------------------------------------------
// const defines
//------------------------------------------------------------------------------
#define MAX_RT_DEVICES 8
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
\brief  Select PCAP device

With this function the PCAP device to be used for openPOWERLINK is selected
from a list of devices.

\param  pDevName_p              Pointer to store the device name which should be
                                used.

\return The function returns 0 if a device could be selected, otherwise -1.

*/
//------------------------------------------------------------------------------
int selectDevice(char* pDevName_p)
{
    int inum, sockfd, j, ret;
    struct ifreq ifr_buf[MAX_RT_DEVICES];
    int devices = 0;
    struct ifconf ifc;

    printf("--------------------------------------------------\n");
    printf("List of Ethernet cards found in this system: \n");
    printf("--------------------------------------------------\n");

    sockfd = socket(PF_PACKET, SOCK_RAW, 0);
    if (sockfd < 0) {

        printf("%s, Error opening socket: %d\n", __func__, sockfd);
        return -1;
    }

    ifc.ifc_len = sizeof(ifr_buf);
    ifc.ifc_req = ifr_buf;

    ret = ioctl(sockfd, SIOCGIFCONF, &ifc);
    if (ret < 0) {
        close(sockfd);
        printf("%s, Error retrieving device list: %d\n", __func__, ret);
        return -1;
    }

    while (ifc.ifc_len >= (int)sizeof(struct ifreq)) {
        struct ifreq ifr;

        memcpy(ifr.ifr_name, ifc.ifc_req[devices].ifr_name, IFNAMSIZ);
        
        ifc.ifc_len -= sizeof(struct ifreq);
        devices++;
    }

    close(sockfd);

    if (devices == 0)
    {
        printf("No interface found!\n");
        return -1;
    }

    for (j = 0; j < devices; j++) {
        printf("%d. %s\n", j, ifr_buf[j].ifr_name);
    }

    printf("--------------------------------------------------\n");
    /*printf("Select the interface to be used for POWERLINK (0-%d):", (devices - 1));
    if (scanf("%d", &inum) == EOF)
    {
        return -1;
    }*/

    inum = 0; // Force to select device 0, which should be rteth0

    printf("--------------------------------------------------\n");
    if ((inum < 0) || (inum > devices -1))
    {
        printf("\nInterface number out of range.\n");
        return -1;
    }

    strncpy(pDevName_p, ifr_buf[inum].ifr_name, 127);

    return 0;
}

//============================================================================//
//            P R I V A T E   F U N C T I O N S                               //
//============================================================================//
/// \name Private Functions
/// \{


///\}
