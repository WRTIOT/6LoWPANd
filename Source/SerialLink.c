/****************************************************************************
 *
 * MODULE:             SerialLink
 *
 * COMPONENT:          $RCSfile: SerialLink.c,v $
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

#ifdef DEBUG_SERIAL_LINK
#define DEBUG_ENABLE
#endif

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/

#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>

#include <stdint.h>
#include <string.h>
#include <libdaemon/daemon.h>
#include "SerialLink.h"


/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/

#define SL_START_CHAR	0x01
#define SL_ESC_CHAR		0x02
#define SL_END_CHAR		0x03

#if DEBUG_ENABLE
#define vDebug(...)     daemon_log(LOG_DEBUG, __VA_ARGS__)
#define vPrintf(...)    daemon_log(LOG_DEBUG, __VA_ARGS__)
#else
#define vDebug(...)
#define vPrintf(...)
#endif

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

typedef enum
{
    E_STATE_RX_WAIT_START,
    E_STATE_RX_WAIT_TYPE,
    E_STATE_RX_WAIT_LENMSB,
    E_STATE_RX_WAIT_LENLSB,
    E_STATE_RX_WAIT_CRC,
    E_STATE_RX_WAIT_DATA,
} teSL_RxState;

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/

static uint8_t u8SL_CalculateCRC(uint8_t u8Type, uint16_t u16Length, uint8_t *pu8Data);

static int iSL_TxByte(bool bSpecialCharacter, uint8_t u8Data);



static bool bSL_RxByte(uint8_t *pu8Data);

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

extern int serial_fd;

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/
bool bSL_ReadMessage(uint8_t *pu8Type, uint16_t *pu16Length, uint16_t u16MaxLength, uint8_t *pu8Message)
{

    static teSL_RxState eRxState = E_STATE_RX_WAIT_START;
    static uint8_t u8CRC;
    uint8_t u8Data;
    static uint16_t u16Bytes;
    static bool bInEsc = FALSE;

    while(bSL_RxByte(&u8Data))
    {
        //vDebug("0x%02x ", u8Data);
        switch(u8Data)
        {

        case SL_START_CHAR:
            u16Bytes = 0;
            bInEsc = FALSE;
            vDebug("RX Start\n");
            eRxState = E_STATE_RX_WAIT_TYPE;
            break;

        case SL_ESC_CHAR:
            vDebug("Got ESC\n");
            bInEsc = TRUE;
            break;

        case SL_END_CHAR:
            vDebug("Got END\n");
            if(u8CRC == u8SL_CalculateCRC(*pu8Type, *pu16Length, pu8Message))
            {
                eRxState = E_STATE_RX_WAIT_START;
                return(TRUE);
            }
            vDebug("CRC BAD\n");
            break;

        default:
            if(bInEsc)
            {
                u8Data ^= 0x10;
                bInEsc = FALSE;
            }

            switch(eRxState)
            {

                case E_STATE_RX_WAIT_START:
                    break;

                case E_STATE_RX_WAIT_TYPE:
                    vDebug("Type %d\n", u8Data);
                    *pu8Type = u8Data;
                    eRxState++;
                    break;

                case E_STATE_RX_WAIT_LENMSB:
                    *pu16Length = (uint16_t)u8Data << 8;
                    eRxState++;
                    break;

                case E_STATE_RX_WAIT_LENLSB:
                    *pu16Length += (uint16_t)u8Data;
                    vDebug("Length %d\n", *pu16Length);
                    if(*pu16Length > u16MaxLength)
                    {
                        vDebug("Length > MaxLength\n");
                        eRxState = E_STATE_RX_WAIT_START;
                    }
                    else
                    {
                        eRxState++;
                    }
                    break;

                case E_STATE_RX_WAIT_CRC:
                    vDebug("CRC %02x\n", u8Data);
                    u8CRC = u8Data;
                    eRxState++;
                    break;

                case E_STATE_RX_WAIT_DATA:
                    if(u16Bytes < *pu16Length)
                    {
                        vDebug("Data\n");
                        pu8Message[u16Bytes++] = u8Data;
                    }
                    break;

                default:
                    vDebug("Unknown state\n");
                    eRxState = E_STATE_RX_WAIT_START;
            }
            break;

        }

    }

    return(FALSE);
}


/****************************************************************************
 *
 * NAME: vSL_WriteRawMessage
 *
 * DESCRIPTION:
 *
 * PARAMETERS: Name        RW  Usage
 *
 * RETURNS:
 * void
 ****************************************************************************/
void vSL_WriteMessage(uint8_t u8Type, uint16_t u16Length, uint8_t *pu8Data)
{
    int n;

    vDebug("\nvSL_WriteMessage(%d, %d, %02x)\n", u8Type, u16Length, u8SL_CalculateCRC(u8Type, u16Length, pu8Data));

    /* Send start character */
    if (iSL_TxByte(TRUE, SL_START_CHAR) < 0) return;

    /* Send message type */
    if (iSL_TxByte(FALSE, u8Type) < 0) return;

    /* Send message length */
    if (iSL_TxByte(FALSE, (u16Length >> 8) & 0xff) < 0) return;
    if (iSL_TxByte(FALSE, (u16Length >> 0) & 0xff) < 0) return;

    /* Send message checksum */
    if (iSL_TxByte(FALSE, u8SL_CalculateCRC(u8Type, u16Length, pu8Data)) < 0) return;

    /* Send message payload */

    for(n = 0; n < u16Length; n++)
    {
        if (iSL_TxByte(FALSE, pu8Data[n]) < 0) return;
    }

    /* Send end character */
    if (iSL_TxByte(TRUE, SL_END_CHAR) < 0) return;
}


/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/
static uint8_t u8SL_CalculateCRC(uint8_t u8Type, uint16_t u16Length, uint8_t *pu8Data)
{
    int n;
    uint8_t u8CRC = 0;

//    vDebug("\nBegin CRC Calc\n");

//    vDebug("%02x %02x %02x", u8Type, (uint8)(u16Length >> 8) & 0xff, (uint8)(u16Length >> 0) & 0xff);

    u8CRC ^= u8Type;
    u8CRC ^= (u16Length >> 8) & 0xff;
    u8CRC ^= (u16Length >> 0) & 0xff;

    for(n = 0; n < u16Length; n++)
    {
//    vDebug(" %02x", pu8Data[n]);
        u8CRC ^= pu8Data[n];
    }

//    vDebug("\n[CRC=%02x]", u8CRC);
    return(u8CRC);
}

/****************************************************************************
 *
 * NAME: vSL_TxByte
 *
 * DESCRIPTION:
 *
 * PARAMETERS: 	Name        		RW  Usage
 *
 * RETURNS:
 * void
 ****************************************************************************/
static int iSL_TxByte(bool bSpecialCharacter, uint8_t u8Data)
{
    if(!bSpecialCharacter && (u8Data < 0x10))
    {
        u8Data ^= 0x10;

        if (serial_write(serial_fd, SL_ESC_CHAR) < 0) return -1;
        //vDebug(" 0x%02x", SL_ESC_CHAR);
    }
    //vDebug(" 0x%02x", u8Data);

    return serial_write(serial_fd, u8Data);
}


/****************************************************************************
 *
 * NAME: bSL_RxByte
 *
 * DESCRIPTION:
 *
 * PARAMETERS: 	Name        		RW  Usage
 *
 * RETURNS:
 * void
 ****************************************************************************/
static bool bSL_RxByte(uint8_t *pu8Data)
{
    return serial_read(serial_fd, pu8Data);
}


/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/

