/****************************************************************************
 *
 * MODULE:             Linux 6LoWPAN Routing daemon
 *
 * COMPONENT:          Interface to module
 *
 * REVISION:           $Revision: 43420 $
 *
 * DATED:              $Date: 2012-06-18 15:13:17 +0100 (Mon, 18 Jun 2012) $
 *
 * AUTHOR:             Lee Mitchell
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

#ifndef  MODULECONFIG_H_INCLUDED
#define  MODULECONFIG_H_INCLUDED

#include <stdint.h>
#include <netinet/in.h>

#if defined __cplusplus
extern "C" {
#endif

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/

/* Default network configuration */
#define CONFIG_DEFAULT_CHANNEL                          0
#define CONFIG_DEFAULT_PAN_ID                           0xFFFF
#define CONFIG_DEFAULT_NETWORK_ID                       0x11121112
#define CONFIG_DEFAULT_PREFIX                           0xfd040bd380e80002LL
#define CONFIG_DEFAULT_REGION                           E_REGION_EUROPE
#define CONFIG_DEFAULT_PROFILE                          0

/* Default security configuration */
#define SECURITY_CONFIG_DEFAULT_NETWORK_KEY_H_MSB       0x00000000
#define SECURITY_CONFIG_DEFAULT_NETWORK_KEY_H_LSB       0x00000000
#define SECURITY_CONFIG_DEFAULT_NETWORK_KEY_L_MSB       0x00000000
#define SECURITY_CONFIG_DEFAULT_NETWORK_KEY_L_LSB       0x00000000
#define SECURITY_CONFIG_DEFAULT_AUTH_SCHEME             E_AUTH_SCHEME_NONE
#define SECURITY_CONFIG_DEFAULT_SCHEME_RADIUS_PAP_IPV6  "::"

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/


/** Enumerated type of statuses */
typedef enum
{
    E_MODULE_OK,
    E_MODULE_ERROR,
    E_MODULE_COMMS_FAILED,
} teModuleStatus;


/** Enumerated type of module modes */
typedef enum
{
    E_MODE_COORDINATOR      = 0,        /**< Start module as a coordinator */
    E_MODE_ROUTER           = 1,        /**< Start module as a router */
    E_MODE_COMMISSIONING    = 2,        /**< Start module in commissioning mode */
} teModuleMode;


/** Enumerated type of allowable certification regions */
typedef enum
{
    E_REGION_EUROPE,
    E_REGION_USA,
    E_REGION_JAPAN,
    
    E_REGION_MAX
} teRegion;


/** Enumerated type of allowable channels */
typedef enum
{
    E_CHANNEL_AUTOMATIC     = 0,
    E_CHANNEL_MINIMUM       = 11,
    E_CHANNEL_MAXIMUM       = 26
} teChannel;


/** Enumerated type of supported authorisation schemes */
typedef enum
{
    E_AUTH_SCHEME_NONE,
    E_AUTH_SCHEME_RADIUS_PAP,
} teAuthScheme;


/** Per authorisation scheme union of required configuration data */
typedef union
{
    struct
    {
        struct in6_addr sAuthServerIP;
    } sRadiusPAP;
} tuAuthSchemeData;


/** Enumerated type of Activity LED DIOs */
typedef enum
{
    E_ACTIVITY_LED_NONE = 0xFFFFFFFF,
} teActivityLED;


/** Enumerated type of supported radio front ends */
typedef enum
{
    E_FRONTEND_STANDARD_POWER,          /**< No frontend - just a standard power device */
    E_FRONTEND_HIGH_POWER,              /**< High power module - enable PA and LNA */
    E_FRONTEND_ETSI,                    /**< Enable ETSI compliant mode */
} __attribute__((__packed__)) teRadioFrontEnd;


/** Function to call when the network configuration changes */
extern void *(*vprConfigChanged)(void *arg);


/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/


/** Module mode */
extern teModuleMode     eModuleMode;


/** Operating certification region */
extern teRegion         eRegion;


/** Channel number to operate on */
extern teChannel        eChannel;


/** IEEE802.15.4 PAN ID */
extern uint16_t         u16PanID;


/** User data used to keep JenNet networks separate */
extern uint32_t         u32UserData;


/** IPv6 Network prefix */
extern uint64_t         u64NetworkPrefix;


/** Flag that network is running securely */
extern int              iSecureNetwork;


/** Network profile to use */
extern uint8_t          u8JenNetProfile;


/** Security key in use */
extern struct in6_addr  sSecurityKey;


/** Security Auth scheme in use */
extern teAuthScheme     eAuthScheme;


/** Configuresd security Auth scheme data */
extern tuAuthSchemeData uAuthSchemeData;


/** Configured radio front end */
extern teRadioFrontEnd  eRadioFrontEnd;


/** Turn Antenna Diversity on */
extern int              iAntennaDiversity;


/** Configured Activity LED DIO */
extern teActivityLED    eActivityLED;


/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/


/** Start the Jennic module comms going
 *  \return E_MODULE_OK on success
 */
teModuleStatus eJennicModuleStart(void);


/** Start the wireless network on the Jennic module 
 *  \return E_MODULE_OK on success
 */
teModuleStatus eJennicModuleRun(void);


/** Reset the Jennic module 
 *  \return E_MODULE_OK on success
 */
teModuleStatus eJennicModuleReset(void);


/** Query the modules IPv6 Address 
 *  \return E_MODULE_OK on success
 */
teModuleStatus eJennicModuleGetIPv6Address(void);


/** Write available IPv6 packet to the module
 *  \param u32Length    Amount of data available
 *  \param pu8Data      Data to write
 *  \return E_MODULE_OK if data written ok
 */
teModuleStatus eJennicModuleWriteIPv6(uint32_t u32Length, uint8_t *pu8Data);


/** Process an incoming message from the module
 *  \param u8Message    Message number
 *  \param u32Length    Length of message
 *  \param pu8Data      Message payload
 *  \return E_MODULE_OK on success
 */
teModuleStatus eJennicModuleProcessMessage(uint8_t u8Message, uint32_t u32Length, uint8_t *pu8Data);


/** Jennic module state mechine
 *  Call at regular intervals and after receiving incoming packets.
 *  \return E_MODULE_OK on success
 */
teModuleStatus eJennicModuleStateMachine(uint8_t bTimeout);

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/

#if defined __cplusplus
}
#endif

#endif  /* MODULECONFIG_H_INCLUDED */

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/

