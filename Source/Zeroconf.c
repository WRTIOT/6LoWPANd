/****************************************************************************
 *
 * MODULE:             Linux 6LoWPAN Routing daemon
 *
 * COMPONENT:          ZeroConf module
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

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <libdaemon/daemon.h>

#include <avahi-client/client.h>
#include <avahi-client/publish.h>

#include <avahi-common/alternative.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>
#include <avahi-common/timeval.h>

static AvahiEntryGroup *group = NULL;
static AvahiSimplePoll *simple_poll = NULL;

static int iClientRunning = 0;

static char *name       = NULL;
static char *hostname   = NULL;
static char *address    = NULL;

static void create_services(AvahiClient *c);

static void entry_group_callback(AvahiEntryGroup *g, AvahiEntryGroupState state, AVAHI_GCC_UNUSED void *userdata) {
    assert(g == group || group == NULL);
    group = g;

    /* Called whenever the entry group state changes */

    switch (state)
    {
        case AVAHI_ENTRY_GROUP_REGISTERING :
            daemon_log(LOG_DEBUG, "Registering Service '%s' on host '%s'.", name, hostname);
            break;
            
        case AVAHI_ENTRY_GROUP_ESTABLISHED :
            /* The entry group has been established successfully */
            daemon_log(LOG_INFO, "Service '%s' successfully established on host '%s'.", name, hostname);
            break;

        case AVAHI_ENTRY_GROUP_COLLISION :
        {
            char *n;
            
            /* A service name collision with a remote service
             * happened. Let's pick a new name */
            
            /* New hostname */
            n = avahi_alternative_host_name(hostname);
            avahi_free(hostname);
            hostname = n;
            
            /* New service name */
            n = avahi_alternative_service_name(name);
            avahi_free(name);
            name = n;

            daemon_log(LOG_INFO, "Service name collision, renaming service to '%s' on host '%s'", name, hostname);

            /* And recreate the services */
            create_services(avahi_entry_group_get_client(g));
            break;
        }

        case AVAHI_ENTRY_GROUP_FAILURE :

            daemon_log(LOG_WARNING, "Entry group failure: %s", avahi_strerror(avahi_client_errno(avahi_entry_group_get_client(g))));

            /* Some kind of failure happened while we were registering our services */
            avahi_simple_poll_quit(simple_poll);
            break;

        case AVAHI_ENTRY_GROUP_UNCOMMITED:
        default:
            ;
    }
}

static void create_services(AvahiClient *c) {
    char *n;
    int ret;
    assert(c);

    /* If this is the first time we're called, let's create a new
     * entry group if necessary */

    if (!group)
        if (!(group = avahi_entry_group_new(c, entry_group_callback, NULL))) {
            daemon_log(LOG_WARNING, "avahi_entry_group_new() failed: %s", avahi_strerror(avahi_client_errno(c)));
            goto fail;
        }

    /* If the group is empty (either because it was just created, or
     * because it was reset previously, add our entries.  */

    if (avahi_entry_group_is_empty(group))
    {
        n = avahi_strdup_printf("%s.local", hostname);
        daemon_log(LOG_DEBUG, "Adding hostname '%s'", n);
        
        AvahiAddress sAddress;
        sAddress.proto = AVAHI_PROTO_INET6;
                    
        if (inet_pton(AF_INET6, address, &sAddress.data.ipv6) <= 0)
        {
            daemon_log(LOG_WARNING, "Error converting string to address");
            goto fail;
        }

        if ((ret = avahi_entry_group_add_address(group, AVAHI_IF_UNSPEC, AVAHI_PROTO_INET6, 0, n, &sAddress)) < 0)
        {
            daemon_log(LOG_WARNING, "Failed to add hostname: %s", avahi_strerror(ret));
            goto fail;
        }
        
        daemon_log(LOG_DEBUG, "Adding service '%s'", name);
        
        /* Add the service for JIP on the module */
        if ((ret = avahi_entry_group_add_service(group, AVAHI_IF_UNSPEC, AVAHI_PROTO_INET6, 0, name, "_jip._udp", NULL, n, 1873, NULL)) < 0) {

            if (ret == AVAHI_ERR_COLLISION)
                goto collision;

            daemon_log(LOG_WARNING, "Failed to add _jip._udp service: %s", avahi_strerror(ret));
            goto fail;
        }

        avahi_free(n);

        /* Tell the server to register the service */
        if ((ret = avahi_entry_group_commit(group)) < 0) {
            daemon_log(LOG_WARNING, "Failed to commit entry group: %s", avahi_strerror(ret));
            goto fail;
        }
    }

    return;

collision:

    /* A service name collision with a local service happened. Let's
     * pick a new name */
    
    /* New hostname */
    n = avahi_alternative_host_name(hostname);
    avahi_free(hostname);
    hostname = n;
    
    /* New service name */
    n = avahi_alternative_service_name(name);
    avahi_free(name);
    name = n;
    
    daemon_log(LOG_WARNING, "Service name collision, renaming service to '%s'", name);

    avahi_entry_group_reset(group);

    create_services(c);
    return;

fail:
    avahi_simple_poll_quit(simple_poll);
}


static void client_callback(AvahiClient *c, AvahiClientState state, AVAHI_GCC_UNUSED void * userdata) {
    assert(c);

    /* Called whenever the client or server state changes */
    switch (state)
    {
        case AVAHI_CLIENT_S_RUNNING:
            /* The server has startup successfully and registered its host
             * name on the network, so it's time to create our services */
            create_services(c);
            break;

        case AVAHI_CLIENT_FAILURE:
            daemon_log(LOG_WARNING, "Client failure: %s", avahi_strerror(avahi_client_errno(c)));
            avahi_simple_poll_quit(simple_poll);
            break;

        case AVAHI_CLIENT_S_COLLISION:
            /* Let's drop our registered services. When the server is back
             * in AVAHI_SERVER_RUNNING state we will register them
             * again with the new host name. */
            
        case AVAHI_CLIENT_S_REGISTERING:
            /* The server records are now being established. This
             * might be caused by a host name change. We need to wait
             * for our own records to register until the host name is
             * properly esatblished. */

            if (group)
                avahi_entry_group_reset(group);

            break;

        case AVAHI_CLIENT_CONNECTING:
            daemon_log(LOG_DEBUG, "Connecting to Avahi daemon");
            break;
            
        default:
            daemon_log(LOG_DEBUG, "Unhandled client state %d", state);
            break;
    }
}

static pthread_t sZC_ThreadInfo;
static     AvahiClient *client = NULL;


/** Zeroconf thread. This thread loops forever establishing the service with Avahi. */
void *pvZC_Thread(void *args)
{
    int error;
    while (iClientRunning)
    {
        daemon_log(LOG_DEBUG, "Starting avahi client.");
        
        /* Allocate main loop object */
        if (!(simple_poll = avahi_simple_poll_new()))
        {
            daemon_log(LOG_ERR, "Failed to create Avahi simple poll object.");
        }
        else
        {
            /* Allocate a new client */
            client = avahi_client_new(avahi_simple_poll_get(simple_poll), AVAHI_CLIENT_NO_FAIL, client_callback, NULL, &error);

            /* Check wether creating the client object succeeded */
            if (!client)
            {
                daemon_log(LOG_ERR, "Failed to create Avahi client: %s", avahi_strerror(error));
            }
            else
            {
                /* Run the main loop */
                avahi_simple_poll_loop(simple_poll);
                daemon_log(LOG_INFO, "Avahi client terminated");
                /* Client terminated - most likely because the daemon exited */
                
                /* Cleanup things */
                if (group)
                {
                    avahi_entry_group_free(group);
                    group = NULL;
                }
                if (client)
                {
                    avahi_client_free(client);
                    client = NULL;
                }
                if (simple_poll)
                {
                    avahi_simple_poll_free(simple_poll);
                    simple_poll = NULL;
                }
            }

            /* Sleep before trying to re-establish the service */
            sleep(1);
        }
    }
    daemon_log(LOG_INFO, "Zeroconf thread exit");
    return NULL;
}


int ZC_RegisterService(const char *pcServiceName, const char *pcHostname, const char *pcNodeAddress)
{
    if (iClientRunning)
    {
        iClientRunning = 0;
        
        /* If we have an existing avahi client, tell it to quit */
        if (simple_poll)
        {
            avahi_simple_poll_quit(simple_poll);
        }
        
        /* Wait for the avahi thread to exit */
        pthread_join(sZC_ThreadInfo, NULL);
        
        /* Free storage */
        avahi_free(name);
        avahi_free(hostname);
        avahi_free(address); 
    }
    
    name = avahi_strdup(pcServiceName);
    hostname = avahi_strdup(pcHostname);
    address = avahi_strdup(pcNodeAddress);

    /* Create thread to run the main loop */
    if (pthread_create(&sZC_ThreadInfo, NULL, pvZC_Thread, NULL) != 0)
    {
        daemon_log(LOG_ERR, "Error starting ZeroConf thread (%s)", strerror(errno));
        goto fail;
    }

    iClientRunning = 1;

    return 0;

fail:
    avahi_free(name);
    avahi_free(hostname);
    avahi_free(address);

    return 1;
}
