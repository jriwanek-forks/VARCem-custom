/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the network module.
 *
 * FIXME:	We should move the "receiver thread" out of the providers,
 *		and into here, really.
 *
 * Version:	@(#)network.c	1.0.23	2021/03/23
 *
 * Author:	Fred N. van Kempen, <decwiz@yahoo.com>
 *
 *		Copyright 2017-2021 Fred N. van Kempen.
 *
 *		Redistribution and  use  in source  and binary forms, with
 *		or  without modification, are permitted  provided that the
 *		following conditions are met:
 *
 *		1. Redistributions of  source  code must retain the entire
 *		   above notice, this list of conditions and the following
 *		   disclaimer.
 *
 *		2. Redistributions in binary form must reproduce the above
 *		   copyright  notice,  this list  of  conditions  and  the
 *		   following disclaimer in  the documentation and/or other
 *		   materials provided with the distribution.
 *
 *		3. Neither the  name of the copyright holder nor the names
 *		   of  its  contributors may be used to endorse or promote
 *		   products  derived from  this  software without specific
 *		   prior written permission.
 *
 * THIS SOFTWARE  IS  PROVIDED BY THE  COPYRIGHT  HOLDERS AND CONTRIBUTORS
 * "AS IS" AND  ANY EXPRESS  OR  IMPLIED  WARRANTIES,  INCLUDING, BUT  NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE  ARE  DISCLAIMED. IN  NO  EVENT  SHALL THE COPYRIGHT
 * HOLDER OR  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL,  EXEMPLARY,  OR  CONSEQUENTIAL  DAMAGES  (INCLUDING,  BUT  NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE  GOODS OR SERVICES;  LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED  AND ON  ANY
 * THEORY OF  LIABILITY, WHETHER IN  CONTRACT, STRICT  LIABILITY, OR  TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING  IN ANY  WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <wchar.h>
#define HAVE_STDARG_H
#define dbglog network_log
#include <86box/86box.h>
#include <86box/config.h>
#include <86box/device.h>
#include <86box/ui.h>
#include <86box/plat.h>
#include <86box/network.h>


#define ENABLE_NETWORK_DUMP	1


typedef struct {
    int		network;			// current provider
    mutex_t	*mutex;

    void	*priv;				// card priv data
    int		(*poll)(void *);		// card poll function
    NETRXCB	rx;				// card RX function
    uint8_t	*mac;				// card MAC address

    volatile int poll_busy,			// polling thread data
		queue_in_use;
    event_t	*poll_wake,
		*poll_complete,
		*queue_not_in_use;
} netdata_t;


/* Global variables. */
#ifdef ENABLE_NETWORK_LOG
int		network_do_log = ENABLE_NETWORK_LOG;
#endif
int		network_host_ndev;
netdev_t	network_host_devs[32];


static const struct {
    const char		*internal_name;
    const network_t	*net;
} networks[] = {
    { "none",		NULL			},

#ifdef USE_SLIRP
    { "slirp",		&network_slirp		},
#endif
#ifdef USE_UDPLINK
    { "udplink",	&network_udplink	},
#endif
#ifdef USE_PCAP
    { "pcap",		&network_pcap		},
#endif
#ifdef USE_VNS
    { "vns",		&network_vns		},
#endif

    { NULL					}
};
static netdata_t	netdata;		/* operational data per card */


/* UI */
int
network_get_from_internal_name(const char *s)
{
    int c;
	
    for (c = 0; networks[c].internal_name != NULL; c++)
	if (! strcmp(networks[c].internal_name, s))
		return(c);

    /* Not found or not available. */
    return(0);
}


/* UI */
const char *
network_get_internal_name(int net)
{
    return(networks[net].internal_name);
}


/* UI */
const char *
network_getname(int net)
{
    if (networks[net].net != NULL)
	return(networks[net].net->name);

    return(NULL);
}


/* UI */
int
network_available(int net)
{
    if (networks[net].net && networks[net].net->available)
	return(networks[net].net->available());

    return(1);
}


#ifdef _LOGGING
void
network_log(int level, const char *fmt, ...)
{
# ifdef ENABLE_NETWORK_LOG
    va_list ap;

    if (network_do_log >= level) {
	va_start(ap, fmt);
	pclog_ex(fmt, ap);
	va_end(ap);
    }
# endif
}
#endif


void
network_wait(int8_t do_wait)
{
    if (do_wait)
	thread_wait_mutex(netdata.mutex);
      else
	thread_release_mutex(netdata.mutex);
}


void
network_poll(void)
{
    while (netdata.poll_busy)
	thread_wait_event(netdata.poll_wake, -1);

    thread_reset_event(netdata.poll_wake);
}


void
network_busy(int8_t set)
{
    netdata.poll_busy = !!set;

    if (! set)
	thread_set_event(netdata.poll_wake);
}


void
network_end(void)
{
    thread_set_event(netdata.poll_complete);
}


/*
 * Initialize the configured network cards.
 *
 * This function gets called only once, from the System
 * Platform initialization code (currently in pc.c) to
 * set our local stuff to a known state.
 */
void
network_init(void)
{
    int i, k;

    /* Clear the local data. */
    memset(&netdata, 0x00, sizeof(netdata_t));
    netdata.network = NET_NONE;

    /* Initialize to a known state. */
    config.network_type = NET_NONE;
    config.network_card = NET_CARD_NONE;

    /* Create a first device entry that's always there, as needed by UI. */
    strcpy(network_host_devs[0].device, "none");
    strcpy(network_host_devs[0].description, "None");
    network_host_ndev = 1;

    /* Initialize the network provider modules, if present. */
    for (i = 0; networks[i].internal_name != NULL; i++) {
	if (networks[i].net == NULL) continue;

	/* Try to load network provider module. */
	k = networks[i].net->init(&network_host_devs[network_host_ndev]);

	/* If they have interfaces, add them. */
	if (k > 0)
		network_host_ndev += k;
    }
}


/*
 * Attach a network card to the system.
 *
 * This function is called by a hardware driver ("card") after it has
 * finished initializing itself, to link itself to the platform support
 * modules.
 */
int
network_attach(void *dev, uint8_t *mac, NETRXCB rx)
{
    wchar_t temp[256];

    if (config.network_card == NET_CARD_NONE)
	return(1);

    /* Reset the network provider module. */
    if (networks[netdata.network].net->reset(mac) < 0) {
	/* Tell user we can't do this (at the moment.) */
	swprintf(temp, sizeof_w(temp), get_string(IDS_ERR_NONET),
		 networks[netdata.network].net->name);

	(void)ui_msgbox(MBX_ERROR, temp);

	return(0);
    }

    /* All good. Save the card's info. */
    netdata.priv = dev;
    netdata.rx = rx;
    netdata.mac = mac;

    /* Create the network events. */
    netdata.poll_wake = thread_create_event();
    netdata.poll_complete = thread_create_event();

    return(1);
}


/* Stop any network activity. */
void
network_close(void)
{
    /* If already closed, do nothing. */
    if (netdata.network == NET_NONE)

    /* Force-close the network provider module. */
    if (networks[netdata.network].net)
	networks[netdata.network].net->close();
    netdata.network = NET_NONE;

    /* Close the network events. */
    if (netdata.poll_wake != NULL) {
	thread_destroy_event(netdata.poll_wake);
	netdata.poll_wake = NULL;
    }
    if (netdata.poll_complete != NULL) {
	thread_destroy_event(netdata.poll_complete);
	netdata.poll_complete = NULL;
    }

    /* Close the network thread mutex. */
    thread_close_mutex(netdata.mutex);
    netdata.mutex = NULL;
}


/*
 * Reset the network card(s).
 *
 * This function is called each time the system is reset,
 * either a hard reset (including power-up) or a soft reset
 * including C-A-D reset.)  It is responsible for connecting
 * everything together.
 */
void
network_reset(void)
{
    const device_t *dev;

#ifdef ENABLE_NETWORK_LOG
    INFO("NETWORK: reset (type=%i, card=%i) debug=%i\n",
	config.network_type, config.network_card, network_do_log);
#else
    INFO("NETWORK: reset (type=%i, card=%i)\n",
	config.network_type, config.network_card);
#endif
    ui_sb_icon_update(SB_NETWORK, 0);

    /* Just in case.. */
    network_close();

    /* If no active card, we're done. */
    if ((config.network_type == NET_NONE) ||
	(config.network_card == NET_CARD_NONE)) return;

    /* All good. */
    INFO("NETWORK: set up for %s, card='%s'\n",
	network_getname(config.network_type),
	network_card_getname(config.network_card));

    netdata.network = config.network_type;
    netdata.mutex = thread_create_mutex(L"VARCem.NetMutex");

    /* Add the selected card to the I/O system. */
    dev = network_card_getdevice(config.network_card);
    if (dev != NULL)
	device_add(dev);
}


/* Transmit a packet to one of the network providers. */
void
network_tx(uint8_t *bufp, int len)
{
    ui_sb_icon_update(SB_NETWORK, 1);

#if defined(WALTJE) && defined(_DEBUG) && ENABLE_NETWORK_DUMP
{
    char temp[16384];
    hexdump_p(temp, 0, bufp, len);
    pclog_repeat(0);
    DEBUG("NETWORK: >> len=%i\n%s\n", len, temp);
    pclog_repeat(1);
}
#endif

    networks[netdata.network].net->send(bufp, len);

    ui_sb_icon_update(SB_NETWORK, 0);
}


/* Process a packet received from one of the network providers. */
void
network_rx(uint8_t *bufp, int len)
{
    ui_sb_icon_update(SB_NETWORK, 1);

#if defined(WALTJE) && defined(_DEBUG) && ENABLE_NETWORK_DUMP
{
    char temp[16384];
    hexdump_p(temp, 0, bufp, len);
    pclog_repeat(0);
    DEBUG("NETWORK: << len=%i\n%s\n", len, temp);
    pclog_repeat(1);
}
#endif

    if (netdata.rx && netdata.priv)
	netdata.rx(netdata.priv, bufp, len);

    ui_sb_icon_update(SB_NETWORK, 0);
}


/* Get name of host-based network interface. */
int
network_card_to_id(const char *devname)
{
    int i;

    for (i = 0; i < network_host_ndev; i++) {
	if (! strcmp(network_host_devs[i].device, devname)) {
		return(i);
	}
    }

    /* Not found. */
    return(0);
}
