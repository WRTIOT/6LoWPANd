/****************************************************************************
 *
 * MODULE:             Linux 6LoWPAN Routing daemon
 *
 * COMPONENT:          Serial interface
 *
 * REVISION:           $Revision: 43420 $
 *
 * DATED:              $Date: 2012-06-18 15:13:17 +0100 (Mon, 18 Jun 2012) $
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

#include "Serial.h"

#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <string.h>

#include <libdaemon/daemon.h>

#define DEBUG 0

extern int verbosity;

extern volatile sig_atomic_t bRunning;

int serial_fd;

static struct termios options;       //place for settings for serial port
char buf[255];                       //buffer for where data is put

int serial_open(char *name, uint32_t baud)
{
    int fd;
    
    daemon_log(LOG_INFO, "Opening serial device '%s' at baud rate %ubps", name, baud);
    
    switch (baud)
    {
#ifdef B50
        case (50):          baud = B50;         break;
#endif /* B50 */
#ifdef B75
        case (75):          baud = B75;         break;
#endif /* B75 */
#ifdef B110
        case (110):         baud = B110;        break;
#endif /* B110 */
#ifdef B134
        case (134):         baud = B134;        break;
#endif /* B134 */
#ifdef B150
        case (150):         baud = B150;        break;
#endif /* B150 */
#ifdef B200
        case (200):         baud = B200;        break;
#endif /* B200 */
#ifdef B300
        case (300):         baud = B300;        break;
#endif /* B300 */
#ifdef B600
        case (600):         baud = B600;        break;
#endif /* B600 */
#ifdef B1200
        case (1200):        baud = B1200;       break;
#endif /* B1200 */
#ifdef B1800
        case (1800):        baud = B1800;       break;
#endif /* B1800 */
#ifdef B2400
        case (2400):        baud = B2400;       break;
#endif /* B2400 */
#ifdef B4800
        case (4800):        baud = B4800;       break;
#endif /* B4800 */
#ifdef B9600
        case (9600):        baud = B9600;       break;
#endif /* B9600 */
#ifdef B19200
        case (19200):       baud = B19200;      break;
#endif /* B19200 */
#ifdef B38400
        case (38400):       baud = B38400;      break;
#endif /* B38400 */
#ifdef B57600
        case (57600):       baud = B57600;      break;
#endif /* B57600 */
#ifdef B115200
        case (115200):      baud = B115200;     break;
#endif /* B115200 */
#ifdef B230400
        case (230400):      baud = B230400;     break;
#endif /* B230400 */
#ifdef B460800
        case (460800):      baud = B460800;     break;
#endif /* B460800 */
#ifdef B500000
        case (500000):      baud = B500000;     break;
#endif /* B500000 */
#ifdef B576000
        case (576000):      baud = B576000;     break;
#endif /* B576000 */
#ifdef B921600
        case (921600):      baud = B921600;     break;
#endif /* B921600 */
#ifdef B1000000
        case (1000000):     baud = B1000000;    break;
#endif /* B1000000 */
#ifdef B1152000
        case (1152000):     baud = B1152000;    break;
#endif /* B1152000 */
#ifdef B1500000
        case (1500000):     baud = B1500000;    break;
#endif /* B1500000 */
#ifdef B2000000
        case (2000000):     baud = B2000000;    break;
#endif /* B2000000 */
#ifdef B2500000
        case (2500000):     baud = B2500000;    break;
#endif /* B2500000 */
#ifdef B3000000
        case (3000000):     baud = B3000000;    break;
#endif /* B3000000 */
#ifdef B3500000
        case (3500000):     baud = B3500000;    break;
#endif /* B3500000 */
#ifdef B4000000
        case (4000000):     baud = B4000000;    break;
#endif /* B4000000 */
        default:
            daemon_log(LOG_ERR, "Unsupported baud rate speccified (%d)", baud);
            return -1;
    }
    
    //open the device(com port) to be non-blocking (read will return immediately)
    fd = open(name, O_RDWR | O_NOCTTY);
    if (fd < 0)
    {
        daemon_log(LOG_ERR, "Couldn't open serial device \"%s\"(%s)", name, strerror(errno));
        return -1;
    }

    if (tcgetattr(fd,&options) == -1)
    {
        daemon_log(LOG_ERR, "Error getting port settings (%s)", strerror(errno));
        return -1;
    }

    options.c_iflag &= ~(INPCK | ISTRIP | INLCR | IGNCR | ICRNL | IUCLC | IXON | IXANY | IXOFF);
    options.c_iflag = IGNBRK | IGNPAR;
    options.c_oflag &= ~(OPOST | OLCUC | ONLCR | OCRNL | ONOCR | ONLRET);
    options.c_cflag &= ~(CSIZE | CSTOPB | PARENB | CRTSCTS);
    options.c_cflag |= CS8 | CREAD | HUPCL | CLOCAL;
    options.c_lflag &= ~(ISIG | ICANON | ECHO | IEXTEN);

    cfsetispeed(&options, baud);
    cfsetospeed(&options, baud);

    if (tcsetattr(fd,TCSAFLUSH,&options) == -1)
    {
        daemon_log(LOG_ERR, "Error setting port settings (%s)", strerror(errno));
        return -1;
    }
    
    fcntl(fd, F_SETFL, O_NONBLOCK);
    
    serial_fd = fd;
    return fd;
}


int serial_read(const int fd, unsigned char *data)
{
    signed char res;
    
    res = read(serial_fd,data,1);
    if (res > 0)
    {
#if DEBUG
        if (verbosity >= LOG_DEBUG) daemon_log(LOG_DEBUG, "RX %02x", res);
#endif /* DEBUG */
    }
    else
    {
        //printf("Serial read: %d\n", res);
        if (res == 0)
        {
            //daemon_log(LOG_ERR, "Serial connection to module interrupted");
            //bRunning = 0;
        }
        res = 0;
    }
    return res;
}

int serial_write(const int fd, const unsigned char data)
{
    int err, attempts = 0;
#if DEBUG
    if (verbosity >= LOG_DEBUG) daemon_log(LOG_DEBUG, "TX %02x", data);
#endif /* DEBUG */
    
    err = write(serial_fd,&data,1);
    if (err < 0)
    {
        if (errno == EAGAIN)
        {
            for (attempts = 0; attempts <= 5; attempts++)
            {
                usleep(1000);
                err = write(serial_fd,&data,1);
                if (err < 0) 
                {
                    if ((errno == EAGAIN) && (attempts == 5))
                    {
                        daemon_log(LOG_ERR, "Error writing to module after %d attempts(%s)", attempts, strerror(errno));
                        exit(-1);
                    }
                }
                else
                {
                    break;
                }
            }
        }
        else
        {
            daemon_log(LOG_ERR, "Error writing to module(%s)", strerror(errno));
            exit(-1);
        }
    }
    return 0;
}


int serial_read_buffer(const int fd, unsigned char *data, uint32_t *count)
{
    int res;
    
    res = read(serial_fd, data, *count);
    if (res > 0)
    {
        *count = res;
    }
    else
    {
#if DEBUG
        if (verbosity >= LOG_DEBUG) daemon_log(LOG_DEBUG, "Serial read: %d\n", res);
#endif /* DEBUG */
        if (res == 0)
        {
            daemon_log(LOG_ERR, "Serial connection to module interrupted");
            //bRunning = 0;
        }
        res = *count = 0;
    }
    return res;
}


int serial_write_buffer(const int fd, unsigned char *data, uint32_t count)
{
    int attempts = 0;
    //printf("send char %d\n", data);
    int total_sent_bytes = 0, sent_bytes = 0;
    
    while (total_sent_bytes < count)
    {
        sent_bytes = write(serial_fd, &data[total_sent_bytes], count - total_sent_bytes);
        if (sent_bytes <= 0)
        {
            if (errno == EAGAIN)
            {
                if (++attempts >= 5)
                {
                    daemon_log(LOG_ERR, "Error writing to module(%s)", strerror(errno));
                    return -1;
                }
                usleep(1000);
            }
            else
            {
                daemon_log(LOG_ERR, "Error writing to module(%s)", strerror(errno));
                return -1;
            }
        }
        else
        {
            attempts = 0;
            total_sent_bytes += sent_bytes;
        }
    }
    return total_sent_bytes;
}


