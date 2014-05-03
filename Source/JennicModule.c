/****************************************************************************
 *
 * MODULE:             Linux 6LoWPAN Routing daemon
 *
 * COMPONENT:          Interface to module
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
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <sys/stat.h>

#include <libdaemon/daemon.h>

#include "JennicModule.h"
#include "TunDevice.h"
#include "SerialLink.h"

#ifdef USE_ZEROCONF
#include "Zeroconf.h"
#endif /* USE_ZEROCONF */


#define JENNIC_VERSION_MAJOR(a) (a << 16)
#define JENNIC_VERSION_MINOR(a) (a << 8)
#define JENNIC_VERSION_REV(a)   (a << 0)
#define JENNIC_VERSION(a,b,c)   (JENNIC_VERSION_MAJOR(a) | JENNIC_VERSION_MINOR(b) | JENNIC_VERSION_REV(c))

/** Timeout comms after 60 seconds of no data */
#define MODULE_TIMEOUT 60

/** Structure of flags for state machine */
static struct
{
    unsigned    uVersionKnown           : 1;    /**< Version information has been received */
    unsigned    uAddressKnown           : 1;    /**< IPv6 address information has been received */
    unsigned    uConfigKnown            : 1;    /**< Configuration of node is known */
    unsigned    uSupportsPing           : 1;    /**< Node supports the ping message */
} sFlags;


static enum
{
    E_STATE_IDLE,
    E_STATE_DETERMINE_VERSION,
    E_STATE_CONFIGURE_NETWORK,
    E_STATE_CONFIGURE_SECURITY,
    E_STATE_CONFIGURE_PROFILE,
    E_STATE_START_MODULE,
    E_STATE_CONFIGURE_FRONTEND,
    E_STATE_DETERMINE_CONFIGURATION,
    E_STATE_DETERMINE_ADDRESS,
    E_STATE_ACTIVITY_LED,
    E_STATE_RUNNING,
} eModuleState;


/** Structure definition to configure the operating parameters of the network 
 *  This verison of the structure is used for the 1.0.X series border routers
 */
typedef struct
{
    uint8_t     u8Region;
    uint8_t     u8Channel;
    uint16_t    u16PanID;
    uint32_t    u32NetworkID;
    uint32_t    u64NetworkPrefixMSB;
    uint32_t    u64NetworkPrefixLSB;
} __attribute__((__packed__)) tsModule_ConfigV10 ;


/** Structure definition to configure the operating parameters of the network 
 *  This verison of the structure is used for the 1.1.X series border routers
 */
typedef struct
{
    uint8_t     u8Region;
    uint8_t     u8Channel;
    uint16_t    u16PanID;
    uint32_t    u32NetworkID;
    uint32_t    u64NetworkPrefixMSB;
    uint32_t    u64NetworkPrefixLSB;
} __attribute__((__packed__)) tsModule_ConfigV11 ;


/** Structure definition to configure the security parameters of the network */
typedef struct
{
    struct in6_addr  sKey;                      /**< Store key like an IPv6 address. That gets us round the endianness issues */
    
    teAuthScheme eAuthScheme;
    tuAuthSchemeData uAuthSchemeData;
} __attribute__((__packed__)) tsSecurityConfig ;



teModuleMode     eModuleMode        = E_MODE_COORDINATOR;
teRegion         eRegion            = CONFIG_DEFAULT_REGION;
teChannel        eChannel           = CONFIG_DEFAULT_CHANNEL;
uint16_t         u16PanID           = CONFIG_DEFAULT_PAN_ID;
uint32_t         u32UserData        = CONFIG_DEFAULT_NETWORK_ID;
uint64_t         u64NetworkPrefix   = CONFIG_DEFAULT_PREFIX;
uint8_t          u8JenNetProfile    = CONFIG_DEFAULT_PROFILE;

int              iSecureNetwork     = 0;
struct in6_addr  sSecurityKey;
teAuthScheme     eAuthScheme        = SECURITY_CONFIG_DEFAULT_AUTH_SCHEME;
tuAuthSchemeData uAuthSchemeData;
void *(*vprConfigChanged)(void *arg)= NULL;


teActivityLED    eActivityLED       = E_ACTIVITY_LED_NONE;


teRadioFrontEnd  eRadioFrontEnd     = E_FRONTEND_STANDARD_POWER;

int              iAntennaDiversity  = 0;

/** Firmware version of the connected device */
static uint32_t u32JennicDeviceVersion = 0;

/** Time of last successful communications */
time_t  sLastSuccessfulComms = 0;

extern int verbosity;

static teModuleStatus eJennicModuleWriteConfig(void)
{
    if ((sFlags.uVersionKnown == 0) || (u32JennicDeviceVersion <= JENNIC_VERSION(1,0,255)))
    {
        tsModule_ConfigV10 sConfig;
        
        /* Setting new configuration */
        sConfig.u8Region            = eRegion;
        sConfig.u8Channel           = eChannel;
        sConfig.u16PanID            = htons(u16PanID);
        sConfig.u32NetworkID        = htonl(u32UserData);
        sConfig.u64NetworkPrefixMSB = htonl((u64NetworkPrefix >> 32) & 0xFFFFFFFF);
        sConfig.u64NetworkPrefixLSB = htonl((u64NetworkPrefix >>  0) & 0xFFFFFFFF);
        
        daemon_log(LOG_INFO, "Writing configuration to Module");
        daemon_log(LOG_INFO, "Config 15.4 Region    : %d", sConfig.u8Region);
        daemon_log(LOG_INFO, "Config 15.4 Channel   : %d", sConfig.u8Channel);
        daemon_log(LOG_INFO, "Config 15.4 PAN ID    : 0x%x", ntohs(sConfig.u16PanID));
        daemon_log(LOG_INFO, "Config JenNet ID      : 0x%x", (unsigned int)ntohl(sConfig.u32NetworkID));
        daemon_log(LOG_INFO, "Config 6LoWPAN Prefix : 0x%08x%08x", (unsigned int)ntohl(sConfig.u64NetworkPrefixMSB), (unsigned int)ntohl(sConfig.u64NetworkPrefixLSB));
        
        if (verbosity >= LOG_DEBUG) 
        {
            daemon_log(LOG_DEBUG, "Writing Module: Network Config");
        }
        
        /* Send the module's configuration data */
        vSL_WriteMessage(E_SL_MSG_CONFIG, sizeof(tsModule_ConfigV10), (uint8_t*)&sConfig);
    }
    else if ((sFlags.uVersionKnown == 1) && (u32JennicDeviceVersion >= JENNIC_VERSION(1,1,0)))
    {
        tsModule_ConfigV11 sConfig;
        
        /* Setting new configuration */
        sConfig.u8Region            = eRegion;
        sConfig.u8Channel           = eChannel;
        sConfig.u16PanID            = htons(u16PanID);
        sConfig.u32NetworkID        = htonl(u32UserData);
        sConfig.u64NetworkPrefixMSB = htonl((u64NetworkPrefix >> 32) & 0xFFFFFFFF);
        sConfig.u64NetworkPrefixLSB = htonl((u64NetworkPrefix >>  0) & 0xFFFFFFFF);

        daemon_log(LOG_INFO, "Writing configuration to Module");
        daemon_log(LOG_INFO, "Config 15.4 Region    : %d", sConfig.u8Region);
        daemon_log(LOG_INFO, "Config 15.4 Channel   : %d", sConfig.u8Channel);
        daemon_log(LOG_INFO, "Config 15.4 PAN ID    : 0x%x", ntohs(sConfig.u16PanID));
        daemon_log(LOG_INFO, "Config JenNet ID      : 0x%x", (unsigned int)ntohl(sConfig.u32NetworkID));
        daemon_log(LOG_INFO, "Config 6LoWPAN Prefix : 0x%08x%08x", (unsigned int)ntohl(sConfig.u64NetworkPrefixMSB), (unsigned int)ntohl(sConfig.u64NetworkPrefixLSB));
        
        if (verbosity >= LOG_DEBUG)
        {
            daemon_log(LOG_DEBUG, "Writing Module: Network Config");
        }
        
        /* Send the module's configuration data */
        vSL_WriteMessage(E_SL_MSG_CONFIG, sizeof(tsModule_ConfigV11), (uint8_t*)&sConfig);
    }
    else
    {
        daemon_log(LOG_ERR, "Cannot configure border router node version V%d.%d.%d", 
                   (u32JennicDeviceVersion >> 16) & 0x0F, 
                   (u32JennicDeviceVersion >>  8) & 0x0F, 
                   (u32JennicDeviceVersion >>  0) & 0x0F);
        return E_MODULE_ERROR;
    }
    
    return E_MODULE_OK;
}


static teModuleStatus eJennicModuleWriteSecurityConfig(void)
{
    tsSecurityConfig sSecurityConfig;
    
    sSecurityConfig.sKey            = sSecurityKey;
    sSecurityConfig.eAuthScheme     = htonl(eAuthScheme);
    sSecurityConfig.uAuthSchemeData = uAuthSchemeData;

    /* Print config */
    {
        char buffer[INET6_ADDRSTRLEN] = "Could not determine Security Key";
        inet_ntop(AF_INET6, &sSecurityConfig.sKey, buffer, INET6_ADDRSTRLEN);

        daemon_log(LOG_INFO, "Enabling network security:");
        daemon_log(LOG_INFO, "Network Key           : %s", buffer);
        
        switch (ntohl(sSecurityConfig.eAuthScheme))
        {
            case(E_AUTH_SCHEME_NONE):
                daemon_log(LOG_INFO, "Authorisation Scheme  : None");
                break;
                
            case(E_AUTH_SCHEME_RADIUS_PAP):
            {
                char buffer[INET6_ADDRSTRLEN] = "Could not determine Security Key";
                inet_ntop(AF_INET6, &sSecurityConfig.uAuthSchemeData.sRadiusPAP.sAuthServerIP, buffer, INET6_ADDRSTRLEN);
                daemon_log(LOG_INFO, "Authorisation Scheme  : RADIUS server at %s using PAP", buffer);
                break;
            }
            
            default:
                break;
        }
    }
    
    if (verbosity >= LOG_DEBUG)
    {
        daemon_log(LOG_DEBUG, "Writing Module: Security Config");
    }
    /* Send security configuration data */
    vSL_WriteMessage(E_SL_MSG_SECURITY, sizeof(tsSecurityConfig), (uint8_t*)&sSecurityConfig);
    
    return E_MODULE_OK;
}


static teModuleStatus eJennicModuleWriteActivityLED(void)
{
    if (eActivityLED != E_ACTIVITY_LED_NONE)
    {
        uint8_t u8ActivityLED = (uint8_t)eActivityLED;
        if (verbosity >= LOG_DEBUG)
        {
            daemon_log(LOG_DEBUG, "Writing Module: Activity LED: %d", u8ActivityLED);
        }
        vSL_WriteMessage(E_SL_MSG_ACTIVITY_LED, sizeof(uint8_t), &u8ActivityLED);
    }
    return E_MODULE_OK;
}


teModuleStatus eJennicModuleWriteProfile(void)
{
    if (u32JennicDeviceVersion >= JENNIC_VERSION(1,1,0))
    {
        /* Version 1.1 up supports profiles */
        if (verbosity >= LOG_DEBUG)
        {
            daemon_log(LOG_DEBUG, "Writing Module: Set JenNet Profile (%d)", u8JenNetProfile & 0xff);
        }
        vSL_WriteMessage(E_SL_MSG_PROFILE, sizeof(uint8_t), &u8JenNetProfile);
    }
    return E_MODULE_OK;
}


teModuleStatus eJennicModuleWriteFrontEndConfig(void)
{
    if (u32JennicDeviceVersion >= JENNIC_VERSION(1,4,0))
    {
        /* Version 1.4 up support configuring radio frontend and antenna diversity*/
        if (verbosity >= LOG_DEBUG)
        {
            daemon_log(LOG_DEBUG, "Writing Module: Set Frontend (%d)", eRadioFrontEnd);
        }
        vSL_WriteMessage(E_SL_MSG_SET_RADIO_FRONTEND, sizeof(uint8_t), &eRadioFrontEnd);
        
        if (iAntennaDiversity)
        {
            if (verbosity >= LOG_DEBUG)
            {
                daemon_log(LOG_DEBUG, "Writing Module: Enabling Antenna Diversity");
            }
            vSL_WriteMessage(E_SL_MSG_ENABLE_DIVERSITY, 0, NULL);
        }
    }
    return E_MODULE_OK;
}


teModuleStatus eJennicModuleRun(void)
{
    if (eModuleMode == E_MODE_COORDINATOR)
    {
        if (verbosity >= LOG_DEBUG)
        {
            daemon_log(LOG_DEBUG, "Writing Module: Run Coordinator");
        }
        vSL_WriteMessage(E_SL_MSG_RUN_COORDINATOR, 0, NULL);
    }
    else if (eModuleMode == E_MODE_ROUTER)
    {
        if (verbosity >= LOG_DEBUG)
        {
            daemon_log(LOG_DEBUG, "Writing Module: Run Router");
        }
        vSL_WriteMessage(E_SL_MSG_RUN_ROUTER, 0, NULL);
    }
    else if (eModuleMode == E_MODE_COMMISSIONING)
    {
        if (verbosity >= LOG_DEBUG)
        {
            daemon_log(LOG_DEBUG, "Writing Module: Run Commisioning");
        }
        vSL_WriteMessage(E_SL_MSG_RUN_COMMISIONING, 0, NULL);
    }
    else
    {
        daemon_log(LOG_ERR, "Unknown module mode: %d", eModuleMode);
    }
    return E_MODULE_OK;
}


teModuleStatus eJennicModuleReset(void)
{
    if (verbosity >= LOG_DEBUG)
    {
        daemon_log(LOG_DEBUG, "Writing Module: Reset");
    }
    vSL_WriteMessage(E_SL_MSG_RESET, 0, NULL);
    return E_MODULE_OK;
}


teModuleStatus JennicModuleGetIPv6Address(void)
{
    if (verbosity >= LOG_DEBUG)
    {
        daemon_log(LOG_DEBUG, "Writing Module: Get Address");
    }
    sFlags.uAddressKnown = 0;
    vSL_WriteMessage(E_SL_MSG_ADDR, 0, NULL);
    return E_MODULE_OK;
}


teModuleStatus eJennicModuleWriteIPv6(uint32_t u32Length, uint8_t *pu8Data)
{
    vSL_WriteMessage(E_SL_MSG_IPV6, u32Length, pu8Data);
    return E_MODULE_OK;
}


teModuleStatus eJennicModuleWritePing(void)
{
    if (verbosity >= LOG_DEBUG)
    {
        daemon_log(LOG_DEBUG, "Writing Module: Ping");
    }
    vSL_WriteMessage(E_SL_MSG_PING, 0, NULL);
    return E_MODULE_OK;
}


teModuleStatus eJennicModuleWriteVersionRequest(void)
{
    if (verbosity >= LOG_DEBUG)
    {
        daemon_log(LOG_DEBUG, "Writing Module: Get Version");
    }
    vSL_WriteMessage(E_SL_MSG_VERSION_REQUEST, 0, NULL);
    return E_MODULE_OK;
}


teModuleStatus eJennicModuleWriteConfigRequest(void)
{
    if (verbosity >= LOG_DEBUG)
    {
        daemon_log(LOG_DEBUG, "Writing Module: Get Config");
    }
    vSL_WriteMessage(E_SL_MSG_CONFIG_REQUEST, 0, NULL);
    return E_MODULE_OK;
}


/** Detect communication failures with the border router by
 *  sending a regular ping message.
 *  Process incoming ping messages from the module.
 *  \param  u32Length   Length of received packet
 *  \param  pu8Data     Pointer to message. If NULL, this is called from state machine.
 *  \return E_MODULE_OK or E_MODULE_COMMS_FAILED on error
 */
static teModuleStatus eJennicModulePing(uint32_t u32Length, uint8_t *pu8Data)
{
#define PING_INTERVAL   (10) /* Seconds between pings */
    static time_t   sLastPing = 0;              /* Time last ping was sent */

    if (sFlags.uSupportsPing == 1)
    {
        /* Connected border router supports ping */

        if (pu8Data)
        {
            /* Ping received: time of last successful comms will be updated */
            if (verbosity >= LOG_DEBUG)
            {
                daemon_log(LOG_DEBUG, "Pong");
            }
        }
        else
        {
            if (difftime(time(NULL), sLastPing) > PING_INTERVAL)
            {
                if (verbosity >= LOG_DEBUG)
                {
                    daemon_log(LOG_DEBUG, "Ping");
                }
                vSL_WriteMessage(E_SL_MSG_PING, 0, NULL);
                sLastPing = time(NULL);
            }
            else
            {
                /* Do nothing */
            }
        }
    }
    return E_MODULE_OK;
}


teModuleStatus eJennicModuleStateMachine(uint8_t bTimeout)
{
    static uint32_t u32Retries = 0;
#define MAX_VERSION_RETRIES 3
#define MAX_ADDRESS_RETRIES 6
    
    switch (eModuleState)
    {
        case (E_STATE_DETERMINE_VERSION):
            if (sFlags.uVersionKnown == 0)
            {
                if (u32Retries)
                {
                    if (verbosity >= LOG_DEBUG)
                    {
                        daemon_log(LOG_DEBUG, "Timeout waiting for version");
                    }
                }
                if (++u32Retries < MAX_VERSION_RETRIES)
                {
                    if (verbosity >= LOG_DEBUG)
                    {
                        daemon_log(LOG_DEBUG, "Requesting version");
                    }
                    eJennicModuleWriteVersionRequest();
                }
                else
                {
                    u32Retries = 0;
                    eModuleState = E_STATE_CONFIGURE_NETWORK;
                }
                break;
            }
            else
            {
                u32Retries = 0;
                eModuleState = E_STATE_CONFIGURE_NETWORK;
            }
            /* Fall through to next state if we know the version of border router node */

        case (E_STATE_CONFIGURE_NETWORK):
            eJennicModuleWriteConfig();
            eModuleState = E_STATE_CONFIGURE_SECURITY;
            break;
        
        case (E_STATE_CONFIGURE_SECURITY):
            if (iSecureNetwork)
            {
                eJennicModuleWriteSecurityConfig();
            }
            
            if (u32JennicDeviceVersion >= JENNIC_VERSION(1,1,0))
            {
                /* Border router 1.1.0 and above support profiles */
                eModuleState = E_STATE_CONFIGURE_PROFILE;
            }
            else
            {
                eModuleState = E_STATE_START_MODULE;
            }
            break;
        
        case (E_STATE_CONFIGURE_PROFILE):
            eJennicModuleWriteProfile();
            eModuleState = E_STATE_START_MODULE;
            break;
            
        case (E_STATE_START_MODULE):
            eJennicModuleRun();
            
            if (u32JennicDeviceVersion >= JENNIC_VERSION(1,4,0))
            {
                /* Border router 1.4.0 and above support configuring radio frontend */
                eModuleState = E_STATE_CONFIGURE_FRONTEND;
            }
            else if (u32JennicDeviceVersion >= JENNIC_VERSION(1,1,0))
            {
                /* Version From version 1.1 on we can request the configuration from the node */
                /* It will ignore these requests until it's network is up. */
                eModuleState = E_STATE_DETERMINE_CONFIGURATION;
            }
            else
            {
                sFlags.uAddressKnown = 0;
                eModuleState = E_STATE_DETERMINE_ADDRESS;
            }
            break;

        case (E_STATE_CONFIGURE_FRONTEND):
            eJennicModuleWriteFrontEndConfig();
            eModuleState = E_STATE_DETERMINE_CONFIGURATION;
            break; 
            
        case (E_STATE_DETERMINE_CONFIGURATION):
            if (sFlags.uConfigKnown == 0)
            {
                /* Keep requesting configuration until the module responds */
                if (verbosity >= LOG_DEBUG)
                {
                    daemon_log(LOG_DEBUG, "Requesting configuration");
                }
                eJennicModuleWriteConfigRequest();
                break;
            }
            else
            {
                /* Got configuration, now get the address */
                eModuleState = E_STATE_DETERMINE_ADDRESS;
                sFlags.uAddressKnown = 0;
            }
            /* Fall through to next state if we know the configuration of border router node */
            
        case (E_STATE_DETERMINE_ADDRESS):
            if (sFlags.uAddressKnown == 0)
            {
                if (bTimeout)
                {
                    /* Wait for a timeout */
                    if (u32Retries && (verbosity >= LOG_DEBUG))
                    {
                        daemon_log(LOG_DEBUG, "Timeout waiting for address");
                    }
                    if (++u32Retries < MAX_ADDRESS_RETRIES)
                    {
                        if (verbosity >= LOG_DEBUG)
                        {
                            daemon_log(LOG_DEBUG, "Requesting module address");
                        }
                        JennicModuleGetIPv6Address();
                    }
                    else
                    {
                        daemon_log(LOG_ERR, "Cannot determine module address");
                        eModuleState    = E_STATE_DETERMINE_VERSION;
                        eJennicModuleReset();
                    }
                }
                break;
            }
            else
            {
                u32Retries = 0;
                eModuleState = E_STATE_ACTIVITY_LED;
            }
            /* Fall through to next state */
            
        case (E_STATE_ACTIVITY_LED):
            if (u32JennicDeviceVersion >= JENNIC_VERSION(1,3,0))
            {
                /* Border router 1.3.0 and above support Activity LED */
                eJennicModuleWriteActivityLED();
            }
            eModuleState = E_STATE_RUNNING;
            break;
            
        case (E_STATE_RUNNING):
            if (eJennicModulePing(0, NULL) != E_MODULE_OK)
            {
                return E_MODULE_ERROR;
            }
            
            break;
    
            
        default:
            break;
        
    }
    
    if ((sFlags.uVersionKnown) && (sFlags.uSupportsPing))
    {
        if (difftime(time(NULL), sLastSuccessfulComms) > MODULE_TIMEOUT)
        {
            daemon_log(LOG_ERR, "Node not responding (last comms %d seconds ago)", (int)difftime(time(NULL), sLastSuccessfulComms));
            return E_MODULE_COMMS_FAILED;
        }
    }
    return E_MODULE_OK;
}


teModuleStatus eJennicModuleStart(void)
{
    if (verbosity >= LOG_DEBUG)
    {
        daemon_log(LOG_DEBUG, "Starting module");
    }
    eModuleState    = E_STATE_DETERMINE_VERSION;
    memset(&sFlags, 0, sizeof(sFlags));
    return eJennicModuleStateMachine(0);
}


static teModuleStatus eJennicModuleProcessMessageIPv6(uint32_t u32Length, uint8_t *pu8Data)
{
    // Write the packet into the TUN device and let the kernel do it's stuff
    if (eTunDeviceWritePacket(u32Length, pu8Data) != E_TUN_OK)
    {
        daemon_log(LOG_ERR, "Error writing to tun device");
        return E_MODULE_ERROR;
    }
    return E_MODULE_OK;
}


static teModuleStatus eJennicModuleProcessMessageVersion(uint32_t u32Length, uint8_t *pu8Data)
{
    u32JennicDeviceVersion = 0;
    u32JennicDeviceVersion |= JENNIC_VERSION_MAJOR (pu8Data[0]);
    u32JennicDeviceVersion |= JENNIC_VERSION_MINOR (pu8Data[1]);
    u32JennicDeviceVersion |= JENNIC_VERSION_REV   (pu8Data[2]);

    daemon_log(LOG_INFO, "Connected to Border router V%d.%d.%d", pu8Data[0], pu8Data[1], pu8Data[2]);

    sFlags.uVersionKnown = 1;
    
    if (u32JennicDeviceVersion >= JENNIC_VERSION(1,1,0))
    {
        /* Version 1.1.0 and greater of the border router support ping */ 
        sFlags.uSupportsPing = 1;
    }
    
    return E_MODULE_OK;
}


static teModuleStatus eJennicModuleProcessMessageConfig(uint32_t u32Length, uint8_t *pu8Data)
{
    int iConfigChanged = 0;
    if (u32Length == 3)
    {
        /* This is actually a version packet in response to the config message */
        return eJennicModuleProcessMessageVersion(u32Length, pu8Data);
    }
    
    /* This is a configuration packet */
    
    if (u32JennicDeviceVersion >= JENNIC_VERSION(1,1,0))
    {
        /* Configuration packet from 1.1 series border router */
        tsModule_ConfigV11 *psConfig = (tsModule_ConfigV11 *)pu8Data;
        uint64_t u64NewPrefix;
        
        /* We MUST remember the configuration so that we can restart with the same network parameters.
         * Otherwise we might not be able to talk to devices again once they have joined to these parameters.
         */
        
        u64NewPrefix = (((uint64_t)htonl(psConfig->u64NetworkPrefixMSB)) << 32) | ((uint64_t)htonl(psConfig->u64NetworkPrefixLSB));
        
        if ((eRegion        != psConfig->u8Region) ||
            (eChannel       != psConfig->u8Channel) ||
            (u16PanID       != ntohs(psConfig->u16PanID)) ||
            (u32UserData    != ntohl(psConfig->u32NetworkID)) ||
            (u64NewPrefix   != u64NetworkPrefix))
        {
            iConfigChanged = 1;
        }
            
        eRegion             = psConfig->u8Region;
        eChannel            = psConfig->u8Channel;
        u16PanID            = ntohs(psConfig->u16PanID);
        u32UserData         = ntohl(psConfig->u32NetworkID);
        u64NetworkPrefix    = u64NewPrefix;

        daemon_log(LOG_INFO, "Received configuration from Module");
        daemon_log(LOG_INFO, "Config 15.4 Region    : %d", eRegion);
        daemon_log(LOG_INFO, "Config 15.4 Channel   : %d", eChannel);
        daemon_log(LOG_INFO, "Config 15.4 PAN ID    : 0x%x", u16PanID);
        daemon_log(LOG_INFO, "Config JenNet ID      : 0x%x", u32UserData);
        daemon_log(LOG_INFO, "Config 6LoWPAN Prefix : 0x%016llx", u64NetworkPrefix);
        
        if ((vprConfigChanged) && iConfigChanged)
        {
            pthread_t sThread;
            /* Start the network changed function in a new thread */
            
            if (pthread_create(&sThread, NULL, vprConfigChanged, NULL) != 0)
            {
                daemon_log(LOG_ERR, "Error starting configuration changed notification thread\n");
            }
        }
        
        sFlags.uConfigKnown = 1;
    }

    return E_MODULE_OK;
}


static teModuleStatus eJennicModuleProcessMessageSecurity(uint32_t u32Length, uint8_t *pu8Data)
{
    /* This is a security configuration packet */
    
    if (u32JennicDeviceVersion >= JENNIC_VERSION(1,1,0))
    {
        /* Configuration packet from 1.1 series border router */
        tsSecurityConfig *psSecurity = (tsSecurityConfig *)pu8Data;
        
        /* We MUST remember the configuration so that we can restart with the same network parameters.
         * Otherwise we might not be able to talk to devices again once they have joined to these parameters.
         */
        
        char buffer[INET6_ADDRSTRLEN] = "Could not determine address\n";
        
        /* Running securely */
        iSecureNetwork = 1;
        memcpy(&sSecurityKey, &psSecurity->sKey, sizeof(struct in6_addr));

        inet_ntop(AF_INET6, &sSecurityKey, buffer, INET6_ADDRSTRLEN);
        
        daemon_log(LOG_INFO, "Received security configuration from Module");
        daemon_log(LOG_INFO, "Security key: %s", buffer);
    }

    return E_MODULE_OK;
}


static teModuleStatus eJennicModuleProcessMessageConfigRequest(uint32_t u32Length, uint8_t *pu8Data)
{
    daemon_log(LOG_INFO, "Configuration request from module");

    /* Resend the configuration */
    eModuleState = E_STATE_CONFIGURE_NETWORK;
    
    return E_MODULE_OK;
}


static teModuleStatus eJennicModuleProcessMessageIPv6Address(uint32_t u32Length, uint8_t *pu8Data)
{
    char buffer[INET6_ADDRSTRLEN] = "Could not determine address";
    inet_ntop(AF_INET6, pu8Data, buffer, INET6_ADDRSTRLEN);
    
    daemon_log(LOG_INFO, "Module address: %s", buffer);
    
#ifdef USE_ZEROCONF
    {
        char acHostname[255];
        sprintf(acHostname, "BR_%s", cpTunDevice);
        ZC_RegisterService("JIP Border Router", acHostname, buffer);
    }
#endif /* USE_ZEROCONF */
    
    {
        char acFileName[255];
        int fd;
        
        sprintf(acFileName, "/tmp/6LoWPANd.%s", cpTunDevice);
        
        fd = open(acFileName, O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
        if (fd < 0)
        {
            daemon_log(LOG_ERR, "Error storing Module address: open (%s)", strerror(errno));
            return E_MODULE_ERROR;
        }
        
        if (write(fd, buffer, strlen(buffer)) < 0)
        {
            daemon_log(LOG_ERR, "Error storing Module address: write (%s)", strerror(errno));
            close(fd);
            return E_MODULE_ERROR;
        }
        
        if (write(fd, "\n", 1) < 0)
        {
            daemon_log(LOG_ERR, "Error storing Module address: write (%s)", strerror(errno));
            close(fd);
            return E_MODULE_ERROR;
        }
        
        close(fd);
    }
    
    sFlags.uAddressKnown = 1;
    return E_MODULE_OK;
}


static teModuleStatus eJennicModuleProcessMessageLog(uint32_t u32Length, uint8_t *pu8Data)
{
    uint8_t u8Priority = pu8Data[0];
    
    if (u8Priority > LOG_DEBUG)
    {
        u8Priority = LOG_DEBUG;
    }
    
    /* Ensure the message is NULL terminated */
    pu8Data[u32Length] = '\0';
    
    /* Log the message */
    daemon_log(u8Priority, "Module: %s", &pu8Data[1]);
    
    return E_MODULE_OK;
}


teModuleStatus eJennicModuleProcessMessage(uint8_t u8Message, uint32_t u32Length, uint8_t *pu8Data)
{
    teModuleStatus eStatus = E_MODULE_ERROR;

    switch(u8Message)
    {
        // Handle each packet type appropriately
#define TEST(X) case (X): /*daemon_log(LOG_DEBUG, #X)*/
        TEST(E_SL_MSG_IPV6);                eStatus = eJennicModuleProcessMessageIPv6(u32Length, pu8Data);          break;
        TEST(E_SL_MSG_CONFIG);              eStatus = eJennicModuleProcessMessageConfig(u32Length, pu8Data);        break;
        TEST(E_SL_MSG_SECURITY);            eStatus = eJennicModuleProcessMessageSecurity(u32Length, pu8Data);      break;
        TEST(E_SL_MSG_ADDR);                eStatus = eJennicModuleProcessMessageIPv6Address(u32Length, pu8Data);   break;
        TEST(E_SL_MSG_CONFIG_REQUEST);      eStatus = eJennicModuleProcessMessageConfigRequest(u32Length, pu8Data); break;
        TEST(E_SL_MSG_LOG);                 eStatus = eJennicModuleProcessMessageLog(u32Length, pu8Data);           break;
        TEST(E_SL_MSG_VERSION);             eStatus = eJennicModuleProcessMessageVersion(u32Length, pu8Data);       break;
        TEST(E_SL_MSG_PING);                eStatus = eJennicModulePing(u32Length, pu8Data);                        break;
        default:                            break;
#undef TEST
    }
    
    // Update the time of the last successful comms with the border router
    sLastSuccessfulComms = time(NULL);
    
    if (eStatus == E_MODULE_OK)
    {
        eStatus = eJennicModuleStateMachine(0);
    }
    
    return eStatus;
}



