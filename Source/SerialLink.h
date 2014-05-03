/****************************************************************************
 *
 * MODULE:             SerialLink
 *
 * COMPONENT:          $RCSfile: SerialLink.h,v $
 *
 * REVISION:           $Revision: 43420 $
 *
 * DATED:              $Date: 2012-06-18 15:13:17 +0100 (Mon, 18 Jun 2012) $
 *
 * AUTHOR:             Lee Mitchell
 *
 * DESCRIPTION:
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

#ifndef  SERIALLINK_H_INCLUDED
#define  SERIALLINK_H_INCLUDED

#if defined __cplusplus
extern "C" {
#endif

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/

#include "Serial.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/

#define SL_READ(PDATA)		serial_read(serial_fd, PDATA)

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

typedef enum
{
    E_SL_MSG_VERSION_REQUEST    =   0,
    E_SL_MSG_VERSION            =   1,
    E_SL_MSG_IPV4               = 100,
    E_SL_MSG_IPV6               = 101,
    E_SL_MSG_CONFIG             = 102,
    E_SL_MSG_RUN_COORDINATOR    = 103,
    E_SL_MSG_RESET              = 104,
    E_SL_MSG_ADDR               = 105,
    E_SL_MSG_CONFIG_REQUEST     = 106,
    E_SL_MSG_SECURITY           = 107,
    E_SL_MSG_LOG                = 108,
    E_SL_MSG_PING               = 109,
    E_SL_MSG_PROFILE            = 110,
    E_SL_MSG_RUN_ROUTER         = 111,
    E_SL_MSG_RUN_COMMISIONING   = 112,
    E_SL_MSG_ACTIVITY_LED       = 113,
    E_SL_MSG_SET_RADIO_FRONTEND = 114,
    E_SL_MSG_ENABLE_DIVERSITY   = 115,
} teSL_MsgType;


/** Typedef bool to builtin integer */
typedef enum
{
#ifdef FALSE
#undef FALSE
#endif
    FALSE = 0,
#ifdef TRUE
#undef TRUE
#endif
    TRUE  = 1,
} bool;

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/

bool bSL_ReadMessage(uint8_t *pu8Type, uint16_t *pu16Length, uint16_t u16MaxLength, uint8_t *pu8Message);
void vSL_WriteMessage(uint8_t u8Type, uint16_t u16Length, uint8_t *pu8Data);

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/

#if defined __cplusplus
}
#endif

#endif  /* SERIALLINK_H_INCLUDED */

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/

