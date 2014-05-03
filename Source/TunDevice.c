/****************************************************************************
 *
 * MODULE:             Linux 6LoWPAN Routing daemon
 *
 * COMPONENT:          Interface to tun device
 *
 * REVISION:           $Revision: 37647 $
 *
 * DATED:              $Date: 2011-12-02 11:16:28 +0000 (Fri, 02 Dec 2011) $
 *
 * AUTHOR:             Matt Redfearn
 *
 ****************************************************************************
 *
 * This software is owned by NXP B.V. and/or its supplier and is protected
 * under applicable copyright laws. All rights are reserved. We grant You,
 * and any third parties, a license to use this software solely and
 * exclusively on NXP products [NXP Microcontrollers such as JN5148, JN5142, JN5139]. 
 * You, and any third parties must reproduce the copyright and warranty notice
 * and any other legend of ownership on each copy or partial copy of the 
 * software.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.

 * Copyright NXP B.V. 2012. All rights reserved
 *
 ***************************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <errno.h>

#include <libdaemon/daemon.h>

#include "TunDevice.h"
#include "JennicModule.h"

/** File descriptor for tun device */
int tun_fd = 0;

/** Name of the tun device to create **/
char *cpTunDevice = "tun0";


teTunStatus eTunDeviceOpen(const char *dev)
{
    struct ifreq ifr;
    int fd, err;

    if((fd = open("/dev/net/tun", O_RDWR)) < 0)
    {
        daemon_log(LOG_ERR, "Open /dev/net/tun failed (%s)", strerror(errno));
        return E_TUN_ERROR;
    }

    memset(&ifr, 0, sizeof(ifr));

    /* Flags: IFF_TUN   - TUN device (no Ethernet headers)
            *        IFF_TAP   - TAP device
            *
            *        IFF_NO_PI - Do not provide packet information
    */
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
    if(*dev)
    {
        strncpy(ifr.ifr_name, dev, IFNAMSIZ);
    }

    err = ioctl(fd, TUNSETIFF, (void *) &ifr);
    if(err < 0)
    {
        daemon_log(LOG_ERR, "Couldn't set options on tun device (%s)", strerror(errno));
        close(fd);
        return E_TUN_ERROR;
    }

    daemon_log(LOG_DEBUG, "Opened tun device: %s", ifr.ifr_name);

    tun_fd = fd;
    return E_TUN_OK;
}


teTunStatus eTunDeviceReadPacket(void)
{
    unsigned char buf[2048];
    int len;
    len = read(tun_fd, buf, sizeof(buf));
    if (len > 0)
    {
        // If there's data waiting for us on the TUN device, write it to the Jennic chip.
        //printf("Data from TUN: %d bytes\n", len);
        
        //for (i = 0; i < len; i++)
        //    printf("%x ", buf[i] & 0x000000FF);
        //printf("\n");
        
        // Send data to Jennic chip
        if (eJennicModuleWriteIPv6(len, buf) != E_MODULE_OK)
        {
            daemon_log(LOG_ERR, "Error writing packet to module");
            return E_TUN_ERROR;
        }
    }
    return E_TUN_OK;
}


teTunStatus eTunDeviceWritePacket(uint32_t u32Length, uint8_t *pu8Data)
{
    int len;

    len = write(tun_fd, pu8Data, u32Length);
    if (len == u32Length)
    {
        //printf("Data to TUN: %d bytes (%d)\n", len, psMsg->u16Length);
        return E_TUN_OK;
    }
    return E_TUN_ERROR;
}



