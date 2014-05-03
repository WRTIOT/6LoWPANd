/****************************************************************************
 *
 * MODULE:             Linux 6LoWPAN Routing daemon
 *
 * COMPONENT:          main program control
 *
 * REVISION:           $Revision: 55220 $
 *
 * DATED:              $Date: 2013-07-10 10:47:34 +0100 (Wed, 10 Jul 2013) $
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
#include <getopt.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/time.h>

#include <libdaemon/daemon.h>

#include "JennicModule.h"
#include "TunDevice.h"
#include "Serial.h"
#include "SerialLink.h"

#define vDelay(a) usleep(a * 1000)

typedef struct
{
    uint8_t     u8Type;
    uint16_t    u16Length;
    uint8_t     u8Message[2048];
} sJennicModuleMsg;

static sJennicModuleMsg sIncomingMsg;

extern int serial_fd;

int verbosity = LOG_INFO;       /** Default log level */

int daemonize = 1;              /** Run as daemon */
int iResetCoordinator = 0;      /** Reset the coordinator at exit */

#ifndef VERSION
#error Version is not defined!
#else
const char *Version = "0.14 (r" VERSION ")";
#endif


/** Baud rate to use for communications */
static uint32_t u32BaudRate = 1000000;

/** Program to run when network configuration changes */
static const char *pcConfigProgram = NULL;

/** Main loop running flag */
volatile sig_atomic_t bRunning = 1;


/** The signal handler just clears the running flag and re-enables itself. */
static void vQuitSignalHandler (int sig)
{
    //printf("Got signal %d\n", sig);
    bRunning = 0;
    signal (sig, vQuitSignalHandler);
    return;
}


static void print_usage_exit(char *argv[])
{
    fprintf(stderr, "6LoWPANd Version: %s\n", Version);
    fprintf(stderr, "Usage: %s\n", argv[0]);
    fprintf(stderr, "  Arguments:\n");
    fprintf(stderr, "    -s --serial        <serial device>     Serial device for 15.4 module, e.g. /dev/tts/1\n");
    fprintf(stderr, "  Options:\n");
    fprintf(stderr, "    -h --help                              Print this help.\n");
    fprintf(stderr, "    -f --foreground                        Do not detatch daemon process, run in foreground.\n");
    fprintf(stderr, "    -v --verbosity     <verbosity>         Verbosity level. Increses amount of debug information. Default %d.\n",  LOG_INFO);
    fprintf(stderr, "    -B --baud          <baud rate>         Baud rate to communicate with border router node at. Default %d\n",     u32BaudRate);
    fprintf(stderr, "    -I --interface     <Interface>         Interface name to create. Default %s.\n", cpTunDevice);
    fprintf(stderr, "    -R --reset                             Reset the coordinator node when 6LoWPANd exits. Default %d.\n", iResetCoordinator);
    fprintf(stderr, "    -C --confignotify  <program>           Program to run when the configuration of the 6LoWPAN network is known.\n");
    fprintf(stderr, "    -A --activityled   <DIO For LED>       Specify an DIO to toggle as an activity LED on the border router.\n");
    
    fprintf(stderr, "  Module options\n");
    fprintf(stderr, "    -F --frontend      <SP,HP,ETSI>        Specify the frontend fitted to the radio. SP=Standard power,HP=High power, ETSI=ETSI compliant mode.\n");
    fprintf(stderr, "    -D --diversity                         Turn on antenna diversity.\n");
    
    fprintf(stderr, "  6LoWPAN Network options:\n");
    fprintf(stderr, "    -m --mode          <mode>              802.15.4 stack mode (coordinator, router, commissioning). Default coordinator.\n");
    fprintf(stderr, "    -r --region        <region>            802.15.4 region (0-Europe,1-USA,2-Japan). Default %d.\n", CONFIG_DEFAULT_REGION);
    fprintf(stderr, "    -c --channel       <channel>           802.15.4 channel to run on. 0 to autoselect. Default %d.\n", CONFIG_DEFAULT_CHANNEL);
    fprintf(stderr, "    -p --pan           <PAN ID>            802.15.4 Pan ID to use. 0xFFFF to autoselect. Default 0x%x.\n", CONFIG_DEFAULT_PAN_ID);
    fprintf(stderr, "    -j --network       <JenNet ID>         JenNet Network ID to use. Default 0x%x.\n", CONFIG_DEFAULT_NETWORK_ID);
    fprintf(stderr, "    -P --profile       <Profile>           JenNet network profile to use. Default %d.\n", CONFIG_DEFAULT_PROFILE);
    fprintf(stderr, "    -6 --prefix        <IPv6 Prefix>       IPv6 Prefix to use. Default 0x%llx.\n", CONFIG_DEFAULT_PREFIX);
    fprintf(stderr, "  Network Security options:\n");
    fprintf(stderr, "    -k --key           <Network key>       Enable JenNet Security for the network, using the given network key\n");
    fprintf(stderr, "    -a --authscheme    <Auth scheme>       Enable authorisation scheme. 0 = disabled, 1 = Radius with PAP\n");
    fprintf(stderr, "   Authorisation scheme = 1 - Radius with PAP\n");
    fprintf(stderr, "    -i --radiusip      <RADIUS IP>         IPv6 address of the RADIUS server\n");
    exit(EXIT_FAILURE);
}


/** Function to be called when the network configuration has been changed.
 *  This is actually run in a separate pthread, so that a blocking program 
 *  can't interrupt communications with the module
 */
void *ConfigChangedCallback(void *arg)
{
    char acCommand[1024];
    char acAddress[INET6_ADDRSTRLEN];
    struct in6_addr sin6_addr;
    int result;
    
    memset(&sin6_addr, 0, sizeof(struct in6_addr));
    
    sin6_addr.s6_addr[0] = (u64NetworkPrefix >> 56) & 0xFF;
    sin6_addr.s6_addr[1] = (u64NetworkPrefix >> 48) & 0xFF;
    sin6_addr.s6_addr[2] = (u64NetworkPrefix >> 40) & 0xFF;
    sin6_addr.s6_addr[3] = (u64NetworkPrefix >> 32) & 0xFF;
    sin6_addr.s6_addr[4] = (u64NetworkPrefix >> 24) & 0xFF;
    sin6_addr.s6_addr[5] = (u64NetworkPrefix >> 16) & 0xFF;
    sin6_addr.s6_addr[6] = (u64NetworkPrefix >>  8) & 0xFF;
    sin6_addr.s6_addr[7] = (u64NetworkPrefix >>  0) & 0xFF;
    
    inet_ntop(AF_INET6, &sin6_addr, acAddress, INET6_ADDRSTRLEN);

    result = sprintf(acCommand, "%s --channel=%d --pan=0x%04x --network=0x%08x --prefix=%s",
                     pcConfigProgram, eChannel, u16PanID, u32UserData, acAddress);
    
    if (iSecureNetwork)
    {
        char buffer[INET6_ADDRSTRLEN] = "Could not determine address\n";
        inet_ntop(AF_INET6, &sSecurityKey, buffer, INET6_ADDRSTRLEN);
        
        sprintf(&acCommand[result], " --key=%s", buffer);
    }    
    
    daemon_log(LOG_DEBUG, "Running configuration notification:\n%s", acCommand);
    
    result = system(acCommand);
    
    if (result == 0)
    {
        daemon_log(LOG_INFO, "Configuration notification program run successfully");
    }
    else
    {
        daemon_log(LOG_ERR, "Configuration notification program result: %d\n", result);
    }
    return NULL;
}


int main(int argc, char *argv[])
{
    fd_set rfds;
    struct timeval tv;
    int retval;
    pid_t pid;
    char *cpSerialDevice = NULL;

    {
        static struct option long_options[] =
        {
            /* Required arguments */
            {"serial",                  required_argument,  NULL, 's'},

            /* Program options */
            {"help",                    no_argument,        NULL, 'h'},
            {"foreground",              no_argument,        NULL, 'f'},
            {"verbosity",               required_argument,  NULL, 'v'},
            {"baud",                    required_argument,  NULL, 'B'},
            {"interface",               required_argument,  NULL, 'I'},
            {"reset",                   no_argument,        NULL, 'R'},
            {"confignotify",            required_argument,  NULL, 'C'},
            {"activityled",             required_argument,  NULL, 'A'},

            /* Module options */
            {"frontend",                required_argument,  NULL, 'F'},
            {"diversity",               no_argument,        NULL, 'D'},
            
            /* 6LoWPAN network options */
            {"mode",                    required_argument,  NULL, 'm'},
            {"region",                  required_argument,  NULL, 'r'},
            {"channel",                 required_argument,  NULL, 'c'},
            {"pan",                     required_argument,  NULL, 'p'},
            {"network",                 required_argument,  NULL, 'j'},
            {"profile",                 required_argument,  NULL, 'P'},
            {"prefix",                  required_argument,  NULL, '6'},

            /* Security options */
            {"key",                     required_argument,  NULL, 'k'},
            {"authscheme",              required_argument,  NULL, 'a'},
            
            /* Radius with PAP options */
            {"radiusip",                required_argument,  NULL, 'i'},
            
            { NULL, 0, NULL, 0}
        };
        signed char opt;
        int option_index;

        while ((opt = getopt_long(argc, argv, "s:hfv:B:I:RC:A:F:Dm:r:c:p:j:P:6:k:a:i:", long_options, &option_index)) != -1) 
        {
            switch (opt) 
            {
                case 'h':
                    print_usage_exit(argv);
                    break;
                case 'f':
                    daemonize = 0;
                    break;
                case 'v':
                    verbosity = atoi(optarg);
                    break;
                case 'B':
                {
                    char *pcEnd;
                    errno = 0;
                    u32BaudRate = strtoul(optarg, &pcEnd, 0);
                    if (errno)
                    {
                        printf("Baud rate '%s' cannot be converted to 32 bit integer (%s)\n", optarg, strerror(errno));
                        print_usage_exit(argv);
                    }
                    if (*pcEnd != '\0')
                    {
                        printf("Baud rate '%s' contains invalid characters\n", optarg);
                        print_usage_exit(argv);
                    }
                    break;
                }
                case 's':
                    cpSerialDevice = optarg;
                    break;
                    
                case 'C':
                {
                    struct stat sStat;
                    
                    /* Check if the given program exists and is executable */
                    if (stat(optarg, &sStat) == 0)
                    {
                        /* File stat'd ok */
                        pcConfigProgram = optarg;
                        vprConfigChanged = ConfigChangedCallback;
                    }
                    else
                    {
                        printf("Config stat notification program '%s' (%s)\n", optarg, strerror(errno));
                        print_usage_exit(argv);
                    }
                    break;
                }
                
                case 'A':
                {
                    char *pcEnd;
                    uint32_t u32ActivityLED;
                    errno = 0;
                    u32ActivityLED = strtoul(optarg, &pcEnd, 0);
                    if (errno)
                    {
                        printf("Activity LED '%s' cannot be converted to 32 bit integer (%s)\n", optarg, strerror(errno));
                        print_usage_exit(argv);
                    }
                    if (*pcEnd != '\0')
                    {
                        printf("Activity LED '%s' contains invalid characters\n", optarg);
                        print_usage_exit(argv);
                    }
                    eActivityLED = u32ActivityLED;
                    break;
                }
                
                case 'F':
                    if (strcmp(optarg, "SP") == 0)
                    {
                        eRadioFrontEnd = E_FRONTEND_STANDARD_POWER;
                    }
                    else if (strcmp(optarg, "HP") == 0)
                    {
                        eRadioFrontEnd = E_FRONTEND_HIGH_POWER;
                    }
                    else if (strcmp(optarg, "ETSI") == 0)
                    {
                        eRadioFrontEnd = E_FRONTEND_ETSI;
                    }
                    else
                    {
                        printf("Unknown mode '%s' specified. Supported modes are 'coordinator', 'router', 'commissioning'\n", optarg);
                        print_usage_exit(argv);
                    }
                    break;
                    
                case 'D':
                    iAntennaDiversity = 1;
                    break;
                    
                case 'm':
                    if (strcmp(optarg, "coordinator") == 0)
                    {
                        eModuleMode = E_MODE_COORDINATOR;
                    }
                    else if (strcmp(optarg, "router") == 0)
                    {
                        eModuleMode = E_MODE_ROUTER;
                    }
                    else if (strcmp(optarg, "commissioning") == 0)
                    {
                        eModuleMode = E_MODE_COMMISSIONING;
                    }
                    else
                    {
                        printf("Unknown mode '%s' specified. Supported modes are 'coordinator', 'router', 'commissioning'\n", optarg);
                        print_usage_exit(argv);
                    }
                    break;
                    
                case 'r':
                {
                    char *pcEnd;
                    uint32_t u32Region;
                    errno = 0;
                    u32Region = strtoul(optarg, &pcEnd, 0);
                    if (errno)
                    {
                        printf("Region '%s' cannot be converted to 32 bit integer (%s)\n", optarg, strerror(errno));
                        print_usage_exit(argv);
                    }
                    if (*pcEnd != '\0')
                    {
                        printf("Region '%s' contains invalid characters\n", optarg);
                        print_usage_exit(argv);
                    }
                    if (u32Region > E_REGION_MAX)
                    {
                        printf("Invalid region '%s' specified\n", optarg);
                        print_usage_exit(argv);
                    }
                    eRegion = (uint8_t)u32Region;
                    break;
                }
                case 'c':
                {
                    char *pcEnd;
                    uint32_t u32Channel;
                    errno = 0;
                    u32Channel = strtoul(optarg, &pcEnd, 0);
                    if (errno)
                    {
                        printf("Channel '%s' cannot be converted to 32 bit integer (%s)\n", optarg, strerror(errno));
                        print_usage_exit(argv);
                    }
                    if (*pcEnd != '\0')
                    {
                        printf("Channel '%s' contains invalid characters\n", optarg);
                        print_usage_exit(argv);
                    }
                    if ( !((u32Channel == E_CHANNEL_AUTOMATIC) ||
                          ((u32Channel >= E_CHANNEL_MINIMUM) && 
                           (u32Channel <= E_CHANNEL_MAXIMUM))))
                    {
                        printf("Invalid Channel '%s' specified\n", optarg);
                        print_usage_exit(argv);
                    }
                    eChannel = (uint8_t)u32Channel;
                    break;
                }
                case 'p':
                {
                    char *pcEnd;
                    uint32_t u32PanID;
                    errno = 0;
                    u32PanID = strtoul(optarg, &pcEnd, 0);
                    if (errno)
                    {
                        printf("PAN ID '%s' cannot be converted to 32 bit integer (%s)\n", optarg, strerror(errno));
                        print_usage_exit(argv);
                    }
                    if (*pcEnd != '\0')
                    {
                        printf("PAN ID '%s' contains invalid characters\n", optarg);
                        print_usage_exit(argv);
                    }
                    if (u32PanID > 0xFFFF)
                    {
                        printf("Invalid PAN ID '%s' specified\n", optarg);
                        print_usage_exit(argv);
                    }
                    u16PanID = (uint16_t)u32PanID;
                    break;
                }
                case 'j':
                {
                    char *pcEnd;
                    uint32_t u32JenNetID;
                    errno = 0;
                    u32JenNetID = strtoul(optarg, &pcEnd, 0);
                    if (errno)
                    {
                        printf("JenNet ID '%s' cannot be converted to 32 bit integer (%s)\n", optarg, strerror(errno));
                        print_usage_exit(argv);
                    }
                    if (*pcEnd != '\0')
                    {
                        printf("JenNet ID '%s' contains invalid characters\n", optarg);
                        print_usage_exit(argv);
                    }
                    u32UserData = u32JenNetID;
                    break;
                }
                case 'P':
                {
                    char *pcEnd;
                    uint32_t u32JenNetProfile;
                    errno = 0;
                    u32JenNetProfile = strtoul(optarg, &pcEnd, 0);
                    if (errno)
                    {
                        printf("JenNet Profile '%s' cannot be converted to 32 bit integer (%s)\n", optarg, strerror(errno));
                        print_usage_exit(argv);
                    }
                    if (*pcEnd != '\0')
                    {
                        printf("JenNet Profile '%s' contains invalid characters\n", optarg);
                        print_usage_exit(argv);
                    }
                    if (u32JenNetProfile > 0xFF)
                    {
                        printf("Invalid JenNet Profile '%s' specified\n", optarg);
                        print_usage_exit(argv);
                    }
                    u8JenNetProfile = (uint8_t)u32JenNetProfile;
                    break;
                }
                case '6':
                {
                    int result;
                    struct in6_addr address;
                    
                    result = inet_pton(AF_INET6, optarg, &address);
                    if (result <= 0)
                    {
                        if (result == 0)
                        {
                            printf("Unknown host: %s\n", optarg);
                        }
                        else if (result < 0)
                        {
                            perror("inet_pton failed");
                        }
                        exit(EXIT_FAILURE);
                    }
                    else
                    {
                        u64NetworkPrefix =  ((uint64_t)address.s6_addr[0] << 56) | 
                                            ((uint64_t)address.s6_addr[1] << 48) | 
                                            ((uint64_t)address.s6_addr[2] << 40) | 
                                            ((uint64_t)address.s6_addr[3] << 32) |
                                            ((uint64_t)address.s6_addr[4] << 24) | 
                                            ((uint64_t)address.s6_addr[5] << 16) | 
                                            ((uint64_t)address.s6_addr[6] <<  8) | 
                                            ((uint64_t)address.s6_addr[7] <<  0);
                    }
                    break;
                }
                case  'I':
                    cpTunDevice = optarg;
                    break;
                    
                case  'R':
                    iResetCoordinator = 1;
                    break;
                    
                case 'k':
                {
                    int result;
                    
                    /* The network key is specified like an IPv6 address, in 8 groups of 16-bit hexadecimal values separated by colons (:) */
                    result = inet_pton(AF_INET6, optarg, &sSecurityKey);
                    if (result <= 0)
                    {
                        if (result == 0)
                        {
                            printf("Could not parse network key: %s\n", optarg);
                        }
                        else if (result < 0)
                        {
                            perror("inet_pton failed");
                        }
                        exit(EXIT_FAILURE);
                    }
                    iSecureNetwork = 1;
                    break;
                }
                
                case 'a':
                {
                    char *pcEnd;
                    uint32_t u32AuthScheme;
                    errno = 0;
                    u32AuthScheme = strtoul(optarg, &pcEnd, 0);
                    if (errno)
                    {
                        printf("Authorisation scheme '%s' cannot be converted to 32 bit integer (%s)\n", optarg, strerror(errno));
                        print_usage_exit(argv);
                    }
                    if (*pcEnd != '\0')
                    {
                        printf("Authorisation scheme '%s' contains invalid characters\n", optarg);
                        print_usage_exit(argv);
                    }

                    switch(u32AuthScheme)
                    {
                        case 0:
                            printf("Warning - no authorisation scheme selected\n");
                            eAuthScheme = u32AuthScheme;
                            break;
                            
                        case (1):
                            eAuthScheme = u32AuthScheme;
                            break;
                            
                        default:
                            printf("Unknown authorisation scheme selected (%d)\n", u32AuthScheme);
                            print_usage_exit(argv);
                            break;
                    }
                    break;
                }
                
                case 'i':
                {
                    switch(eAuthScheme)
                    {
                        case (E_AUTH_SCHEME_RADIUS_PAP):
                        {
                            int result = inet_pton(AF_INET6, optarg, &uAuthSchemeData.sRadiusPAP.sAuthServerIP);
                            if (result <= 0)
                            {
                                if (result == 0)
                                {
                                    printf("Unknown host: %s\n", optarg);
                                }
                                else if (result < 0)
                                {
                                    perror("inet_pton failed");
                                }
                                exit(EXIT_FAILURE);
                            }
                            break;
                        }
                        
                        default:
                            printf("Option '-i' is not appropriate for authorisation scheme %d\n", eAuthScheme);
                            break;
                    }
                    break;
                }
                    
                default: /* '?' */
                    print_usage_exit(argv);
            }
        }
    }
    
    /* Log everything into syslog */
    daemon_log_ident = daemon_ident_from_argv0(argv[0]);
    
    if (!cpSerialDevice)
    {
        print_usage_exit(argv);
    }
    
    if (daemonize)
    {
        /* Prepare for return value passing from the initialization procedure of the daemon process */
        if (daemon_retval_init() < 0) {
            daemon_log(LOG_ERR, "Failed to create pipe.");
            return 1;
        }

        /* Do the fork */
        if ((pid = daemon_fork()) < 0)
        {
            /* Exit on error */
            daemon_log(LOG_ERR, "Failed to fork() daemon process.");
            daemon_retval_done();
            return 1;
        } 
        else if (pid)
        { /* The parent */
            int ret;

            /* Wait for 20 seconds for the return value passed from the daemon process */
            if ((ret = daemon_retval_wait(20)) < 0)
            {
                daemon_log(LOG_ERR, "Could not recieve return value from daemon process: %s", strerror(errno));
                return 255;
            }

            if (ret == 0)
            {
                daemon_log(LOG_INFO, "Daemon process started.");
            }
            else
            {
                daemon_log(LOG_ERR, "Daemon returned %i.", ret);
            }
            return ret;
        } 
        else
        { /* The daemon */
            /* Close FDs */
            if (daemon_close_all(-1) < 0)
            {
                daemon_log(LOG_ERR, "Failed to close all file descriptors: %s", strerror(errno));

                /* Send the error condition to the parent process */
                daemon_retval_send(1);
                goto finish;
            }

            daemon_log_use = DAEMON_LOG_SYSLOG;

            /* Send OK to parent process */
            daemon_retval_send(0);

            daemon_log(LOG_INFO, "Daemon started");
        }
    }
    else
    {
        /* Running foreground - set verbosity */
        if (verbosity > LOG_WARNING)
        {
            daemon_set_verbosity(verbosity);
        }
    }

    FD_ZERO(&rfds);
    /* Wait up to five seconds. */
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    
    if ((serial_open(cpSerialDevice, u32BaudRate) < 0) || (eTunDeviceOpen(cpTunDevice) != E_TUN_OK))
    {
        goto finish;
    }
    
    /* Install signal handlers */
    signal(SIGTERM, vQuitSignalHandler);
    signal(SIGINT, vQuitSignalHandler);
    
    eJennicModuleStart();
    
    while (bRunning)
    {
        int max_fd = 0;
        
        /* Wait up to one second each loop. */
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        FD_ZERO(&rfds);
        FD_SET(serial_fd, &rfds);
        if (serial_fd > max_fd)
        {
            max_fd = serial_fd;
        }
        FD_SET(tun_fd, &rfds);
        if (tun_fd > max_fd)
        {
            max_fd = tun_fd;
        }

        /* Wait for data on one either the serial port or the TUN interface. */
        retval = select(max_fd + 1, &rfds, NULL, NULL, &tv);

        if (retval == -1)
        {
            daemon_log(LOG_ERR, "error in select(): %s", strerror(errno));
        }
        else if (retval)
        {
            int i;
            /* Got data on one of the file descriptors */
            for (i = 0; i < max_fd + 1; i++)
            {
                if (FD_ISSET(i, &rfds) && (i == serial_fd))
                {
                    if(bSL_ReadMessage(&sIncomingMsg.u8Type, &sIncomingMsg.u16Length, sizeof(sIncomingMsg.u8Message), sIncomingMsg.u8Message))
                    {
                        if (eJennicModuleProcessMessage(sIncomingMsg.u8Type, sIncomingMsg.u16Length, sIncomingMsg.u8Message) != E_MODULE_OK)
                        {
                            daemon_log(LOG_ERR, "Error communicating with border router module");
                            bRunning = FALSE;
                        }
                    }
                }
                else if (FD_ISSET(i, &rfds) && (i == tun_fd))
                {
                    if (eTunDeviceReadPacket() != E_TUN_OK)
                    {
                        daemon_log(LOG_ERR, "Error handling tun packet");
                    }
                }
                else if (FD_ISSET(i, &rfds))
                {
                    daemon_log(LOG_DEBUG, "Data on unknown file desciptor (%d)", i);
                }
                else
                {
                    /* Should not get here */
                }
            }
        }
        else
        {
            /* Select timeout */
            if (eJennicModuleStateMachine(1) != E_MODULE_OK)
            {
                daemon_log(LOG_ERR, "Error communicating with border router module");
                bRunning = FALSE;
            }
        }
    }
    
    if (iResetCoordinator)
    {
        daemon_log(LOG_INFO, "Resetting Coordinator Module");    
        eJennicModuleReset();
    }
    
finish:
    if (daemonize)
    {
        daemon_log(LOG_INFO, "Daemon process exiting");  
    }
    return 0;
}
