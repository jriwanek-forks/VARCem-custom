/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the 3Com Etherlink II 3c503 (ISA 8-bit).
 *
 * Version:	@(#)net_3c503.c	1.0.10	2021/03/18
 *
 * Based on	@(#)3c503.cpp Carl (MAME)
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		TheCollector1995, <mariogplayer@gmail.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Carl, <unknown e-mail address>
 *
 *		Copyright 2017-2021 Fred N. van Kempen.
 *		Copyright 2018 Miran Grca.
 *		Portions Copyright 2018 MAME Project.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free  Software  Foundation; either  version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is  distributed in the hope that it will be useful, but
 * WITHOUT   ANY  WARRANTY;  without  even   the  implied  warranty  of
 * MERCHANTABILITY  or FITNESS  FOR A PARTICULAR  PURPOSE. See  the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the:
 *
 *   Free Software Foundation, Inc.
 *   59 Temple Place - Suite 330
 *   Boston, MA 02111-1307
 *   USA.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include <time.h>
#define dbglog network_card_log
#include "../../emu.h"
#include "../../io.h"
#include "../../mem.h"
#include "../../device.h"
#include "../../ui/ui.h"
#include "../../plat.h"
#include "../../misc/random.h"
#include "../system/dma.h"
#include "../system/pic.h"
#include "network.h"
#include "net_dp8390.h"
#include "net_3com.h"
#include "bswap.h"


typedef struct {
    uint16_t	base_address;
    int8_t	base_irq;
    int8_t	dma_channel;
    uint32_t	bios_addr;

    mem_map_t	ram_mapping;

    dp8390_t	dp8390;

    uint8_t	maclocal[6];		/* configured MAC (local) address */
    uint8_t	prom[32];

    struct {
	uint8_t pstr;
	uint8_t pspr;
	uint8_t dqtr;
	uint8_t bcfr;
	uint8_t pcfr;
	uint8_t gacfr;
	uint8_t ctrl;
	uint8_t streg;
	uint8_t idcfr;
	uint16_t da;
	uint32_t vptr;
	uint8_t rfmsb;
	uint8_t rflsb;
    } regs;
} el2_t;


static void
el2_interrupt(el2_t *dev, int set)
{
    switch (dev->base_irq) {
	case 2:
		dev->regs.idcfr = 0x10;
		break;

	case 3:
		dev->regs.idcfr = 0x20;
		break;

	case 4:
		dev->regs.idcfr = 0x40;
		break;

	case 5:
		dev->regs.idcfr = 0x80;
		break;
    }	

    if (set)
	picint(1 << dev->base_irq);
      else
	picintc(1 << dev->base_irq);
}


/*
 * Called by the platform-specific code when an Ethernet frame
 * has been received. The destination address is tested to see
 * if it should be accepted, and if the RX ring has enough room,
 * it is copied into it and the receive process is updated.
 */
static void
el2_rx(void *priv, uint8_t *buf, int io_len)
{
    static uint8_t bcast_addr[6] = {0xff,0xff,0xff,0xff,0xff,0xff};
    el2_t *dev = (el2_t *)priv;
    uint8_t pkthdr[4];
    uint8_t *startptr;
    int rx_pages, avail;
    int idx, nextpage;
    int endbytes;

    /* FIXME: move to upper layer */
    ui_sb_icon_update(SB_NETWORK, 1);

    if (io_len != 60) {
	DEBUG("3C503: rx_frame with length %d\n", io_len);
    }

    if ((dev->dp8390.CR.stop != 0) || (dev->dp8390.page_start == 0)) return;

    /*
     * Add the pkt header + CRC to the length, and work
     * out how many 256-byte pages the frame would occupy.
     */
    rx_pages = (io_len + sizeof(pkthdr) + sizeof(uint32_t) + 255)/256;
    if (dev->dp8390.curr_page < dev->dp8390.bound_ptr) {
	avail = dev->dp8390.bound_ptr - dev->dp8390.curr_page;
    } else {
	avail = (dev->dp8390.page_stop - dev->dp8390.page_start) -
		(dev->dp8390.curr_page - dev->dp8390.bound_ptr);
    }

    /*
     * Avoid getting into a buffer overflow condition by
     * not attempting to do partial receives. The emulation
     * to handle this condition seems particularly painful.
     */
    if	((avail < rx_pages)
#if DP8390_NEVER_FULL_RING
		 || (avail == rx_pages)
#endif
		) {
	DEBUG("3C503: no space\n");

	/* FIXME: move to upper layer */
	ui_sb_icon_update(SB_NETWORK, 0);
	return;
    }

    if ((io_len < 40/*60*/) && !dev->dp8390.RCR.runts_ok) {
	DEBUG("3C503: rejected small packet, length %d\n", io_len);

    /* FIXME: move to upper layer */
	ui_sb_icon_update(SB_NETWORK, 0);
	return;
    }

    /* Some computers don't care... */
    if (io_len < 60)
	io_len = 60;

    DBGLOG(1, "3C503: RX %x:%x:%x:%x:%x:%x > %x:%x:%x:%x:%x:%x len %d\n",
	   buf[6], buf[7], buf[8], buf[9], buf[10], buf[11],
	   buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], io_len);

    /* Do address filtering if not in promiscuous mode. */
    if (! dev->dp8390.RCR.promisc) {
	/* If this is a broadcast frame.. */
	if (! memcmp(buf, bcast_addr, 6)) {
		/* Broadcast not enabled, we're done. */
		if (! dev->dp8390.RCR.broadcast) {
			DEBUG("3C503: RX BC disabled\n");

    /* FIXME: move to upper layer */
			ui_sb_icon_update(SB_NETWORK, 0);
			return;
		}
	}

	/* If this is a multicast frame.. */
	else if (buf[0] & 0x01) {
		/* Multicast not enabled, we're done. */
		if (! dev->dp8390.RCR.multicast) {
			DEBUG("3C503: RX MC disabled\n");

			/* FIXME: move to upper layer */
			ui_sb_icon_update(SB_NETWORK, 0);
			return;
		}

		/* Are we listening to this multicast address? */
		idx = mcast_index(buf);
		if (! (dev->dp8390.mchash[idx>>3] & (1<<(idx&0x7)))) {
			DEBUG("3C503: RX MC not listed\n");

			/* FIXME: move to upper layer */
			ui_sb_icon_update(SB_NETWORK, 0);
			return;
		}
	}

	/* Unicast, must be for us.. */
	else if (memcmp(buf, dev->dp8390.physaddr, 6)) return;
    } else {
	DEBUG("3C503: RX promiscuous receive\n");
    }

    nextpage = dev->dp8390.curr_page + rx_pages;
    if (nextpage >= dev->dp8390.page_stop)
	nextpage -= (dev->dp8390.page_stop - dev->dp8390.page_start);

    /* Set up packet header. */
    pkthdr[0] = 0x01;			/* RXOK - packet is OK */
    if (buf[0] & 0x01)
	pkthdr[0] |= 0x20;		/* MULTICAST packet */
    pkthdr[1] = nextpage;		/* ptr to next packet */
    pkthdr[2] = (uint8_t) ((io_len + sizeof(pkthdr)) & 0xff);	/* length-low */
    pkthdr[3] = (uint8_t) ((io_len + sizeof(pkthdr)) >> 8);	/* length-hi */
    DBGLOG(1, "3C503: RX pkthdr [%02x %02x %02x %02x]\n",
	   pkthdr[0], pkthdr[1], pkthdr[2], pkthdr[3]);

    /* Copy into buffer, update curpage, and signal interrupt if config'd */
	startptr = &dev->dp8390.mem[(dev->dp8390.curr_page * 256) - DP8390_WORD_MEMSTART];
    memcpy(startptr, pkthdr, sizeof(pkthdr));
    if ((nextpage > dev->dp8390.curr_page) ||
	((dev->dp8390.curr_page + rx_pages) == dev->dp8390.page_stop)) {
	memcpy(startptr+sizeof(pkthdr), buf, io_len);
    } else {
	endbytes = (dev->dp8390.page_stop - dev->dp8390.curr_page) * 256;
	memcpy(startptr+sizeof(pkthdr), buf, endbytes-sizeof(pkthdr));
	startptr = &dev->dp8390.mem[(dev->dp8390.page_start * 256) - DP8390_WORD_MEMSTART];
	memcpy(startptr, buf+endbytes-sizeof(pkthdr), io_len-endbytes+8);
    }
    dev->dp8390.curr_page = nextpage;

    dev->dp8390.RSR.rx_ok = 1;
    dev->dp8390.RSR.rx_mbit = (buf[0] & 0x01) ? 1 : 0;
    dev->dp8390.ISR.pkt_rx = 1;

    if (dev->dp8390.IMR.rx_inte)
	el2_interrupt(dev, 1);

    /* FIXME: move to upper layer */
    ui_sb_icon_update(SB_NETWORK, 0);
}


static void
el2_tx(el2_t *dev, uint32_t val)
{
    dev->dp8390.CR.tx_packet = 0;
    dev->dp8390.TSR.tx_ok = 1;
    dev->dp8390.ISR.pkt_tx = 1;

    /* Generate an interrupt if not masked */
    if (dev->dp8390.IMR.tx_inte)
	el2_interrupt(dev, 1);

    dev->dp8390.tx_timer_active = 0;
}


/*
 * Access the 32K private RAM.
 *
 * The NE2000 memory is accessed through the data port of the
 * ASIC (offset 0) after setting up a remote-DMA transfer.
 * Both byte and word accesses are allowed.
 * The first 16 bytes contains the MAC address at even locations,
 * and there is 16K of buffer memory starting at 16K.
 */
static uint32_t
chipmem_read(el2_t *dev, uint32_t addr, unsigned int len)
{
    uint32_t retval = 0;

    if (addr <= 15) {
	retval = dev->prom[addr % 16];
	if (len == 2)
		retval |= (dev->prom[(addr + 1) % 16] << 8);
	return(retval);
    }

    if ((addr >= DP8390_WORD_MEMSTART) && (addr < DP8390_WORD_MEMEND)) {
	retval = dev->dp8390.mem[addr - DP8390_WORD_MEMSTART];
	if (len == 2)
		retval |= (dev->dp8390.mem[addr - DP8390_WORD_MEMSTART + 1] << 8);
	return(retval);
    }

    DEBUG("3C503: out-of-bounds chipmem read, %04X\n", addr);

    return(0xff);
}


static void
chipmem_write(el2_t *dev, uint32_t addr, uint32_t val, unsigned len)
{
    if ((addr >= DP8390_WORD_MEMSTART) && (addr < DP8390_WORD_MEMEND)) {
	dev->dp8390.mem[addr-DP8390_WORD_MEMSTART] = val & 0xff;
	if (len == 2)
		dev->dp8390.mem[addr-DP8390_WORD_MEMSTART+1] = val >> 8;
    } else
	DEBUG("3C503: out-of-bounds chipmem write, %04X\n", addr);
}


/* Handle reads/writes to the 'zeroth' page of the DS8390 register file. */
static uint32_t
page0_read(el2_t *dev, uint32_t off, unsigned int len)
{
    uint8_t retval = 0;

    if (len > 1) {
	/* encountered with win98 hardware probe */
	DEBUG("3C503: bad length! Page0 read from register 0x%02x, len=%u\n",
								off, len);
	return(retval);
    }

    switch(off) {
	case 0x01:	/* CLDA0 */
		retval = (dev->dp8390.local_dma & 0xff);
		break;

	case 0x02:	/* CLDA1 */
		retval = (dev->dp8390.local_dma >> 8);
		break;

	case 0x03:	/* BNRY */
		retval = dev->dp8390.bound_ptr;
		break;

	case 0x04:	/* TSR */
		retval = ((dev->dp8390.TSR.ow_coll    << 7) |
			  (dev->dp8390.TSR.cd_hbeat   << 6) |
			  (dev->dp8390.TSR.fifo_ur    << 5) |
			  (dev->dp8390.TSR.no_carrier << 4) |
			  (dev->dp8390.TSR.aborted    << 3) |
			  (dev->dp8390.TSR.collided   << 2) |
			  (dev->dp8390.TSR.tx_ok));
		break;

	case 0x05:	/* NCR */
		retval = dev->dp8390.num_coll;
		break;

	case 0x06:	/* FIFO */
		/* reading FIFO is only valid in loopback mode */
		DEBUG("3C503: reading FIFO not supported yet\n");
		retval = dev->dp8390.fifo;
		break;

	case 0x07:	/* ISR */
		retval = ((dev->dp8390.ISR.reset     << 7) |
			  (dev->dp8390.ISR.rdma_done << 6) |
			  (dev->dp8390.ISR.cnt_oflow << 5) |
			  (dev->dp8390.ISR.overwrite << 4) |
			  (dev->dp8390.ISR.tx_err    << 3) |
			  (dev->dp8390.ISR.rx_err    << 2) |
			  (dev->dp8390.ISR.pkt_tx    << 1) |
			  (dev->dp8390.ISR.pkt_rx));
		break;

	case 0x08:	/* CRDA0 */
		retval = (dev->dp8390.remote_dma & 0xff);
		break;

	case 0x09:	/* CRDA1 */
		retval = (dev->dp8390.remote_dma >> 8);
		break;

	case 0x0a:	/* reserved / RTL8029ID0 */
		DEBUG("3C503: reserved Page0 read - 0x0a\n");
		retval = 0xff;
		break;

	case 0x0b:	/* reserved / RTL8029ID1 */
		DEBUG("3C503: reserved Page0 read - 0x0b\n");
		retval = 0xff;
		break;

	case 0x0c:	/* RSR */
		retval = ((dev->dp8390.RSR.deferred    << 7) |
			  (dev->dp8390.RSR.rx_disabled << 6) |
			  (dev->dp8390.RSR.rx_mbit     << 5) |
			  (dev->dp8390.RSR.rx_missed   << 4) |
			  (dev->dp8390.RSR.fifo_or     << 3) |
			  (dev->dp8390.RSR.bad_falign  << 2) |
			  (dev->dp8390.RSR.bad_crc     << 1) |
			  (dev->dp8390.RSR.rx_ok));
		break;

	case 0x0d:	/* CNTR0 */
		retval = dev->dp8390.tallycnt_0;
		break;

	case 0x0e:	/* CNTR1 */
		retval = dev->dp8390.tallycnt_1;
		break;

	case 0x0f:	/* CNTR2 */
		retval = dev->dp8390.tallycnt_2;
		break;

	default:
		DEBUG("3C503: Page0 register 0x%02x out of range\n", off);
		break;
    }

    DEBUG("3C503: Page0 read from register 0x%02x, val=0x%02x\n", off, retval);

    return(retval);
}


static void
page0_write(el2_t *dev, uint32_t off, uint32_t val, unsigned len)
{
    uint8_t val2;

    /* It appears to be a common practice to use outw on page0 regs... */

    /* break up outw into two outb's */
    if (len == 2) {
	page0_write(dev, off, (val & 0xff), 1);
	if (off < 0x0f)
		page0_write(dev, off+1, ((val>>8)&0xff), 1);
	return;
    }

    DEBUG("3C503: Page0 write to register 0x%02x, val=0x%02x\n", off, val);

    switch(off) {
	case 0x01:	/* PSTART */
		dev->dp8390.page_start = val;
		break;

	case 0x02:	/* PSTOP */
		dev->dp8390.page_stop = val;
		break;

	case 0x03:	/* BNRY */
		dev->dp8390.bound_ptr = val;
		break;

	case 0x04:	/* TPSR */
		dev->dp8390.tx_page_start = val;
		break;

	case 0x05:	/* TBCR0 */
		/* Clear out low byte and re-insert */
		dev->dp8390.tx_bytes &= 0xff00;
		dev->dp8390.tx_bytes |= (val & 0xff);
		break;

	case 0x06:	/* TBCR1 */
		/* Clear out high byte and re-insert */
		dev->dp8390.tx_bytes &= 0x00ff;
		dev->dp8390.tx_bytes |= ((val & 0xff) << 8);
		break;

	case 0x07:	/* ISR */
		val &= 0x7f;  /* clear RST bit - status-only bit */
		/* All other values are cleared iff the ISR bit is 1 */
		dev->dp8390.ISR.pkt_rx    &= !((int)((val & 0x01) == 0x01));
		dev->dp8390.ISR.pkt_tx    &= !((int)((val & 0x02) == 0x02));
		dev->dp8390.ISR.rx_err    &= !((int)((val & 0x04) == 0x04));
		dev->dp8390.ISR.tx_err    &= !((int)((val & 0x08) == 0x08));
		dev->dp8390.ISR.overwrite &= !((int)((val & 0x10) == 0x10));
		dev->dp8390.ISR.cnt_oflow &= !((int)((val & 0x20) == 0x20));
		dev->dp8390.ISR.rdma_done &= !((int)((val & 0x40) == 0x40));
		val = ((dev->dp8390.ISR.rdma_done << 6) |
		       (dev->dp8390.ISR.cnt_oflow << 5) |
		       (dev->dp8390.ISR.overwrite << 4) |
		       (dev->dp8390.ISR.tx_err    << 3) |
		       (dev->dp8390.ISR.rx_err    << 2) |
		       (dev->dp8390.ISR.pkt_tx    << 1) |
		       (dev->dp8390.ISR.pkt_rx));
		val &= ((dev->dp8390.IMR.rdma_inte << 6) |
		        (dev->dp8390.IMR.cofl_inte << 5) |
		        (dev->dp8390.IMR.overw_inte << 4) |
		        (dev->dp8390.IMR.txerr_inte << 3) |
		        (dev->dp8390.IMR.rxerr_inte << 2) |
		        (dev->dp8390.IMR.tx_inte << 1) |
		        (dev->dp8390.IMR.rx_inte));
		if (val == 0x00)
			el2_interrupt(dev, 0);
		break;

	case 0x08:	/* RSAR0 */
		/* Clear out low byte and re-insert */
		dev->dp8390.remote_start &= 0xff00;
		dev->dp8390.remote_start |= (val & 0xff);
		dev->dp8390.remote_dma = dev->dp8390.remote_start;
		break;

	case 0x09:	/* RSAR1 */
		/* Clear out high byte and re-insert */
		dev->dp8390.remote_start &= 0x00ff;
		dev->dp8390.remote_start |= ((val & 0xff) << 8);
		dev->dp8390.remote_dma = dev->dp8390.remote_start;
		break;

	case 0x0a:	/* RBCR0 */
		/* Clear out low byte and re-insert */
		dev->dp8390.remote_bytes &= 0xff00;
		dev->dp8390.remote_bytes |= (val & 0xff);
		break;

	case 0x0b:	/* RBCR1 */
		/* Clear out high byte and re-insert */
		dev->dp8390.remote_bytes &= 0x00ff;
		dev->dp8390.remote_bytes |= ((val & 0xff) << 8);
		break;

	case 0x0c:	/* RCR */
		/* Check if the reserved bits are set */
		if (val & 0xc0) {
			DEBUG("3C503: RCR write, reserved bits set\n");
		}

		/* Set all other bit-fields */
		dev->dp8390.RCR.errors_ok = ((val & 0x01) == 0x01);
		dev->dp8390.RCR.runts_ok  = ((val & 0x02) == 0x02);
		dev->dp8390.RCR.broadcast = ((val & 0x04) == 0x04);
		dev->dp8390.RCR.multicast = ((val & 0x08) == 0x08);
		dev->dp8390.RCR.promisc   = ((val & 0x10) == 0x10);
		dev->dp8390.RCR.monitor   = ((val & 0x20) == 0x20);

		/* Monitor bit is a little suspicious... */
		if (val & 0x20) {
			DEBUG("3C503: RCR write, monitor bit set!\n");
		}
		break;

	case 0x0d:	/* TCR */
		/* Check reserved bits */
		if (val & 0xe0) {
			DEBUG("3C503: TCR write, reserved bits set\n");
		}

		/* Test loop mode (not supported) */
		if (val & 0x06) {
			dev->dp8390.TCR.loop_cntl = (val & 0x6) >> 1;
			DEBUG("3C503: TCR write, loop mode %d not supported\n",
						dev->dp8390.TCR.loop_cntl);
		} else {
			dev->dp8390.TCR.loop_cntl = 0;
		}

		/* Inhibit-CRC not supported. */
		if (val & 0x01) {
			DEBUG("3C503: TCR write, inhibit-CRC not supported\n");
		}

		/* Auto-transmit disable very suspicious */
		if (val & 0x08) {
			DEBUG("3C503: TCR write, auto transmit disable not supported\n");
		}

		/* Allow collision-offset to be set, although not used */
		dev->dp8390.TCR.coll_prio = ((val & 0x08) == 0x08);
		break;

	case 0x0e:	/* DCR */
		/* the loopback mode is not suppported yet */
		if (! (val & 0x08)) {
			DEBUG("3C503: DCR write, loopback mode selected\n");
		}

		/* It is questionable to set longaddr and auto_rx, since
		 * they are not supported on the NE2000. Print a warning
		 * and continue. */
		if (val & 0x04) {
			DEBUG("3C503: DCR write - LAS set ???\n");
		}
		if (val & 0x10) {
			DEBUG("3C503: DCR write - AR set ???\n");
		}

		/* Set other values. */
		dev->dp8390.DCR.wdsize   = ((val & 0x01) == 0x01);
		dev->dp8390.DCR.endian   = ((val & 0x02) == 0x02);
		dev->dp8390.DCR.longaddr = ((val & 0x04) == 0x04); /* illegal ? */
		dev->dp8390.DCR.loop     = ((val & 0x08) == 0x08);
		dev->dp8390.DCR.auto_rx  = ((val & 0x10) == 0x10); /* also illegal ? */
		dev->dp8390.DCR.fifo_size = (val & 0x50) >> 5;
		break;

	case 0x0f:  /* IMR */
		/* Check for reserved bit */
		if (val & 0x80) {
			DEBUG("3C503: IMR write, reserved bit set\n");
		}

		/* Set other values */
		dev->dp8390.IMR.rx_inte    = ((val & 0x01) == 0x01);
		dev->dp8390.IMR.tx_inte    = ((val & 0x02) == 0x02);
		dev->dp8390.IMR.rxerr_inte = ((val & 0x04) == 0x04);
		dev->dp8390.IMR.txerr_inte = ((val & 0x08) == 0x08);
		dev->dp8390.IMR.overw_inte = ((val & 0x10) == 0x10);
		dev->dp8390.IMR.cofl_inte  = ((val & 0x20) == 0x20);
		dev->dp8390.IMR.rdma_inte  = ((val & 0x40) == 0x40);
		val2 = ((dev->dp8390.ISR.rdma_done << 6) |
		        (dev->dp8390.ISR.cnt_oflow << 5) |
		        (dev->dp8390.ISR.overwrite << 4) |
		        (dev->dp8390.ISR.tx_err    << 3) |
		        (dev->dp8390.ISR.rx_err    << 2) |
		        (dev->dp8390.ISR.pkt_tx    << 1) |
		        (dev->dp8390.ISR.pkt_rx));
		if (((val & val2) & 0x7f) == 0)
			el2_interrupt(dev, 0);
		else
			el2_interrupt(dev, 1);
		break;

	default:
		DEBUG("3C503: Page0 write, bad register 0x%02x\n", off);
		break;
    }
}


/* Handle reads/writes to the first page of the DS8390 register file. */
static uint32_t
page1_read(el2_t *dev, uint32_t off, unsigned int len)
{
    DBGLOG(2, "3C503: Page1 read from register 0x%02x, len=%u\n", off, len);

    switch(off) {
	case 0x01:	/* PAR0-5 */
	case 0x02:
	case 0x03:
	case 0x04:
	case 0x05:
	case 0x06:
		return(dev->dp8390.physaddr[off - 1]);

	case 0x07:	/* CURR */
		DBGLOG(1, "3C503: returning current page: 0x%02x\n",
					(dev->dp8390.curr_page));
		return(dev->dp8390.curr_page);

	case 0x08:	/* MAR0-7 */
	case 0x09:
	case 0x0a:
	case 0x0b:
	case 0x0c:
	case 0x0d:
	case 0x0e:
	case 0x0f:
		return(dev->dp8390.mchash[off - 8]);

	default:
		DEBUG("3C503: Page1 read register 0x%02x out of range\n", off);
		return(0);
    }
}


static void
page1_write(el2_t *dev, uint32_t off, uint32_t val, unsigned int len)
{
    DBGLOG(2, "3C503: Page1 write to register 0x%02x, len=%u, value=0x%04x\n",
								off, len, val);
    switch(off) {
	case 0x01:	/* PAR0-5 */
	case 0x02:
	case 0x03:
	case 0x04:
	case 0x05:
	case 0x06:
		dev->dp8390.physaddr[off - 1] = val;
		if (off == 6) {
			DEBUG("3C503: physical address set to %02x:%02x:%02x:%02x:%02x:%02x\n",
			      dev->dp8390.physaddr[0], dev->dp8390.physaddr[1],
			      dev->dp8390.physaddr[2], dev->dp8390.physaddr[3],
			      dev->dp8390.physaddr[4], dev->dp8390.physaddr[5]);
		}
		break;

	case 0x07:	/* CURR */
		dev->dp8390.curr_page = val;
		break;

	case 0x08:	/* MAR0-7 */
	case 0x09:
	case 0x0a:
	case 0x0b:
	case 0x0c:
	case 0x0d:
	case 0x0e:
	case 0x0f:
		dev->dp8390.mchash[off - 8] = val;
		break;

	default:
		DEBUG("3C503: Page1 write register 0x%02x out of range\n", off);
		break;
    }
}


/* Handle reads/writes to the second page of the DS8390 register file. */
static uint32_t
page2_read(el2_t *dev, uint32_t off, unsigned int len)
{
    DBGLOG(2, "3C503: Page2 read from register 0x%02x, len=%u\n", off, len);
  
    switch(off) {
	case 0x01:	/* PSTART */
		return(dev->dp8390.page_start);

	case 0x02:	/* PSTOP */
		return(dev->dp8390.page_stop);

	case 0x03:	/* Remote Next-packet pointer */
		return(dev->dp8390.rempkt_ptr);

	case 0x04:	/* TPSR */
		return(dev->dp8390.tx_page_start);

	case 0x05:	/* Local Next-packet pointer */
		return(dev->dp8390.localpkt_ptr);

	case 0x06:	/* Address counter (upper) */
		return(dev->dp8390.address_cnt >> 8);

	case 0x07:	/* Address counter (lower) */
		return(dev->dp8390.address_cnt & 0xff);

	case 0x08:	/* Reserved */
	case 0x09:
	case 0x0a:
	case 0x0b:
		DEBUG("3C503: reserved Page2 read - register 0x%02x\n", off);
		return(0xff);

	case 0x0c:	/* RCR */
		return	((dev->dp8390.RCR.monitor   << 5) |
			 (dev->dp8390.RCR.promisc   << 4) |
			 (dev->dp8390.RCR.multicast << 3) |
			 (dev->dp8390.RCR.broadcast << 2) |
			 (dev->dp8390.RCR.runts_ok  << 1) |
			 (dev->dp8390.RCR.errors_ok));

	case 0x0d:	/* TCR */
		return	((dev->dp8390.TCR.coll_prio   << 4) |
			 (dev->dp8390.TCR.ext_stoptx  << 3) |
			 ((dev->dp8390.TCR.loop_cntl & 0x3) << 1) |
			 (dev->dp8390.TCR.crc_disable));

	case 0x0e:	/* DCR */
		return	(((dev->dp8390.DCR.fifo_size & 0x3) << 5) |
			 (dev->dp8390.DCR.auto_rx  << 4) |
			 (dev->dp8390.DCR.loop     << 3) |
			 (dev->dp8390.DCR.longaddr << 2) |
			 (dev->dp8390.DCR.endian   << 1) |
			 (dev->dp8390.DCR.wdsize));

	case 0x0f:	/* IMR */
		return	((dev->dp8390.IMR.rdma_inte  << 6) |
			 (dev->dp8390.IMR.cofl_inte  << 5) |
			 (dev->dp8390.IMR.overw_inte << 4) |
			 (dev->dp8390.IMR.txerr_inte << 3) |
			 (dev->dp8390.IMR.rxerr_inte << 2) |
			 (dev->dp8390.IMR.tx_inte    << 1) |
			 (dev->dp8390.IMR.rx_inte));

	default:
		DEBUG("3C503: Page2 register 0x%02x out of range\n", off);
		break;
    }

    return(0);
}


/* Maybe all writes here should be BX_PANIC()'d, since they
   affect internal operation, but let them through for now
   and print a warning. */
static void
page2_write(el2_t *dev, uint32_t off, uint32_t val, unsigned int len)
{
    DBGLOG(2, "3C503: Page2 write to register 0x%02x, len=%u, value=0x%04x\n",
								off, len, val);
    switch(off) {
	case 0x01:	/* CLDA0 */
		/* Clear out low byte and re-insert */
		dev->dp8390.local_dma &= 0xff00;
		dev->dp8390.local_dma |= (val & 0xff);
		break;

	case 0x02:	/* CLDA1 */
		/* Clear out high byte and re-insert */
		dev->dp8390.local_dma &= 0x00ff;
		dev->dp8390.local_dma |= ((val & 0xff) << 8);
		break;

	case 0x03:	/* Remote Next-pkt pointer */
		dev->dp8390.rempkt_ptr = val;
		break;

	case 0x04:
		DEBUG("page 2 write to reserved register 0x04\n");
		break;

	case 0x05:	/* Local Next-packet pointer */
		dev->dp8390.localpkt_ptr = val;
		break;

	case 0x06:	/* Address counter (upper) */
		/* Clear out high byte and re-insert */
		dev->dp8390.address_cnt &= 0x00ff;
		dev->dp8390.address_cnt |= ((val & 0xff) << 8);
		break;

	case 0x07:	/* Address counter (lower) */
		/* Clear out low byte and re-insert */
		dev->dp8390.address_cnt &= 0xff00;
		dev->dp8390.address_cnt |= (val & 0xff);
		break;

	case 0x08:
	case 0x09:
	case 0x0a:
	case 0x0b:
	case 0x0c:
	case 0x0d:
	case 0x0e:
	case 0x0f:
		DEBUG("3C503: Page2 write to reserved register 0x%02x\n", off);
		break;

	default:
		DEBUG("3C503: Page2 write, illegal register 0x%02x\n", off);
		break;
    }
}


static void
set_drq(el2_t *dev)
{
    switch (dev->dma_channel) {
	case 1:
		dev->regs.idcfr = 1;
		break;

	case 2:
		dev->regs.idcfr = 2;
		break;

	case 3:
		dev->regs.idcfr = 4;
		break;
    }	
}


/* Routines for handling reads/writes to the Command Register. */
static uint32_t
read_cr(el2_t *dev)
{
    uint32_t retval;

    retval =	(((dev->dp8390.CR.pgsel    & 0x03) << 6) |
		 ((dev->dp8390.CR.rdma_cmd & 0x07) << 3) |
		  (dev->dp8390.CR.tx_packet << 2) |
		  (dev->dp8390.CR.start     << 1) |
		  (dev->dp8390.CR.stop));

    DBGLOG(1, "3C503: read CR returns 0x%02x\n", retval);

    return(retval);
}


static void
write_cr(el2_t *dev, uint32_t val)
{
    DBGLOG(1, "3C503: wrote 0x%02x to CR\n", val);

    /* Validate remote-DMA */
    if ((val & 0x38) == 0x00) {
	DEBUG("3C503: CR write - invalid rDMA value 0\n");
	val |= 0x20; /* dma_cmd == 4 is a safe default */
    }

    /* Check for s/w reset */
    if (val & 0x01) {
	dev->dp8390.ISR.reset = 1;
	dev->dp8390.CR.stop   = 1;
    } else
	dev->dp8390.CR.stop = 0;

    dev->dp8390.CR.rdma_cmd = (val & 0x38) >> 3;

    /* If start command issued, the RST bit in the ISR */
    /* must be cleared */
    if ((val & 0x02) && !dev->dp8390.CR.start)
	dev->dp8390.ISR.reset = 0;

    dev->dp8390.CR.start = ((val & 0x02) == 0x02);
    dev->dp8390.CR.pgsel = (val & 0xc0) >> 6;

    /* Check for send-packet command */
    if (dev->dp8390.CR.rdma_cmd == 3) {
	/* Set up DMA read from receive ring */
	dev->dp8390.remote_start = dev->dp8390.remote_dma = dev->dp8390.bound_ptr * 256;
	dev->dp8390.remote_bytes = (uint16_t)chipmem_read(dev, dev->dp8390.bound_ptr * 256 + 2, 2);
	DEBUG("3C503: sending buffer %x length %d\n",
	      dev->dp8390.remote_start, dev->dp8390.remote_bytes);
    }

    /* Check for start-tx */
    if ((val & 0x04) && dev->dp8390.TCR.loop_cntl) {
	if (dev->dp8390.TCR.loop_cntl != 1) {
		DEBUG("3C503: loop mode %d not supported\n",
				 dev->dp8390.TCR.loop_cntl);
	} else {
		el2_rx(dev,
			  &dev->dp8390.mem[dev->dp8390.tx_page_start*256 - DP8390_WORD_MEMSTART],
			  dev->dp8390.tx_bytes);
	}
    } else if (val & 0x04) {
	if (dev->dp8390.CR.stop || (!dev->dp8390.CR.start)) {
		if (dev->dp8390.tx_bytes == 0) /* njh@bandsman.co.uk */
			return; /* Solaris9 probe */
		DEBUG("3C503: CR write - tx start, dev in reset\n");
	}

	if (dev->dp8390.tx_bytes == 0)
		DEBUG("3C503: CR write - tx start, tx bytes == 0\n");

	/* Send the packet to the system driver */
	dev->dp8390.CR.tx_packet = 1;
	network_tx(&dev->dp8390.mem[dev->dp8390.tx_page_start*256 - DP8390_WORD_MEMSTART],
		   dev->dp8390.tx_bytes);
			   
	/* some more debug */
	if (dev->dp8390.tx_timer_active) {
		DEBUG("3C503: CR write, tx timer still active\n");
	}

	el2_tx(dev, val);
    }

    /* Linux probes for an interrupt by setting up a remote-DMA read
     * of 0 bytes with remote-DMA completion interrupts enabled.
     * Detect this here */
    if (dev->dp8390.CR.rdma_cmd == 0x01 && dev->dp8390.CR.start && dev->dp8390.remote_bytes == 0) {
	dev->dp8390.ISR.rdma_done = 1;
	if (dev->dp8390.IMR.rdma_inte) {
		el2_interrupt(dev, 1);
		el2_interrupt(dev, 0);
	}
    }
}


static uint8_t
read_lo(uint16_t addr, priv_t priv)
{
    el2_t *dev = (el2_t *)priv;
    int off = addr - dev->base_address;
    uint8_t retval = 0;

    switch ((dev->regs.ctrl >> 2) & 3) {
	case 0x00:
		DEBUG(0, "Read offset=%04x\n", off);
		if (off != 0x00) switch(dev->dp8390.CR.pgsel) {
			case 0x00:
				retval = page0_read(dev, off, 1);
				break;

			case 0x01:
				retval = page1_read(dev, off, 1);
				break;

			case 0x02:
				retval = page2_read(dev, off, 1);
				break;

			case 0x03:
				retval = 0xff;
				break;
		} else
			retval = read_cr(dev);
		break;

	case 0x01:
		retval = dev->prom[off];
		break;

	case 0x02:
		retval = dev->prom[off + 0x10];
		break;

	case 0x03:
		retval = 0xff;
		break;
    }

    return(retval);
}


static void
write_lo(uint16_t addr, uint8_t val, priv_t priv)
{
    el2_t *dev = (el2_t *)priv;
    int off = addr - dev->base_address;

    switch ((dev->regs.ctrl >> 2) & 3) {
	case 0x00:
		/* The high 16 bytes of i/o space are for the ne2000 asic -
		   the low 16 bytes are for the DS8390, with the current
		   page being selected by the PS0,PS1 registers in the
		   command register */
		if (off != 0x00) switch(dev->dp8390.CR.pgsel) {
			case 0x00:
				page0_write(dev, off, val, 1);
				break;

			case 0x01:
				page1_write(dev, off, val, 1);
				break;

			case 0x02:
				page2_write(dev, off, val, 1);
				break;

			case 0x03:
				break;
		} else
			write_cr(dev, val);
		break;

	case 0x01:
	case 0x02:
	case 0x03:
		break;
    }

    DEBUG("3C503: write addr %x, value %x\n", addr, val);
}


/* Restore state to power-up, cancelling all I/O. */
static void
el2_reset(priv_t priv)
{
    el2_t *dev = (el2_t *)priv;
    int i;

    DEBUG("3C503: reset\n");

    /* Initialize the MAC address area by doubling the physical address. */
    dev->prom[0]  = dev->dp8390.physaddr[0];
    dev->prom[1]  = dev->dp8390.physaddr[1];
    dev->prom[2]  = dev->dp8390.physaddr[2];
    dev->prom[3]  = dev->dp8390.physaddr[3];
    dev->prom[4]  = dev->dp8390.physaddr[4];
    dev->prom[5]  = dev->dp8390.physaddr[5];

//FIXME: why??  --Fvk
    /* NE1K signature. */
    for (i = 6; i < 16; i++)
	dev->prom[i] = 0x57;

    /* Zero out registers and memory. */
    memset(&dev->dp8390.CR,  0x00, sizeof(dev->dp8390.CR) );
    memset(&dev->dp8390.ISR, 0x00, sizeof(dev->dp8390.ISR));
    memset(&dev->dp8390.IMR, 0x00, sizeof(dev->dp8390.IMR));
    memset(&dev->dp8390.DCR, 0x00, sizeof(dev->dp8390.DCR));
    memset(&dev->dp8390.TCR, 0x00, sizeof(dev->dp8390.TCR));
    memset(&dev->dp8390.TSR, 0x00, sizeof(dev->dp8390.TSR));
    memset(&dev->dp8390.RSR, 0x00, sizeof(dev->dp8390.RSR));

    dev->dp8390.tx_timer_active = 0;
    dev->dp8390.local_dma  = 0;
    dev->dp8390.page_start = 0;
    dev->dp8390.page_stop  = 0;
    dev->dp8390.bound_ptr  = 0;
    dev->dp8390.tx_page_start = 0;
    dev->dp8390.num_coll   = 0;
    dev->dp8390.tx_bytes   = 0;
    dev->dp8390.fifo       = 0;
    dev->dp8390.remote_dma = 0;
    dev->dp8390.remote_start = 0;
    dev->dp8390.remote_bytes = 0;
    dev->dp8390.tallycnt_0 = 0;
    dev->dp8390.tallycnt_1 = 0;
    dev->dp8390.tallycnt_2 = 0;

    dev->dp8390.curr_page = 0;

    dev->dp8390.rempkt_ptr   = 0;
    dev->dp8390.localpkt_ptr = 0;
    dev->dp8390.address_cnt  = 0;

    memset(&dev->dp8390.mem, 0x00, sizeof(dev->dp8390.mem));

    /* Set power-up conditions. */
    dev->dp8390.CR.stop      = 1;
    dev->dp8390.CR.rdma_cmd  = 4;
    dev->dp8390.ISR.reset    = 1;
    dev->dp8390.DCR.longaddr = 1;

    memset(&dev->regs, 0, sizeof(dev->regs));

    dev->regs.ctrl = 0x0a;	

    el2_interrupt(dev, 0);
}


static uint8_t
read_hi(uint16_t addr, priv_t priv)
{
    el2_t *dev = (el2_t *)priv;

    DEBUG("3C503: Read GA address=%04x\n", addr);

    switch (addr & 0x0f) {
	case 0x00:
		return dev->regs.pstr;

	case 0x01:
		return dev->regs.pspr;

	case 0x02:
		return dev->regs.dqtr;

	case 0x03:
		switch (dev->base_address) {
			default:
			case 0x300:
				dev->regs.bcfr = 0x80;
				break;

			case 0x310:
				dev->regs.bcfr = 0x40;
				break;

			case 0x330:
				dev->regs.bcfr = 0x20;
				break;

			case 0x350:
				dev->regs.bcfr = 0x10;
				break;

			case 0x250:
				dev->regs.bcfr = 0x08;
				break;

			case 0x280:
				dev->regs.bcfr = 0x04;
				break;

			case 0x2a0:
				dev->regs.bcfr = 0x02;
				break;

			case 0x2e0:
				dev->regs.bcfr = 0x01;
				break;
		}
		return dev->regs.bcfr;

	case 0x04:
		switch (dev->bios_addr) {
			case 0xdc000:
				dev->regs.pcfr = 0x80;
				break;

			case 0xd8000:
				dev->regs.pcfr = 0x40;
				break;

			case 0xcc000:
				dev->regs.pcfr = 0x20;
				break;

			case 0xc8000:
				dev->regs.pcfr = 0x10;
				break;
		}
		return dev->regs.pcfr;

	case 0x05:
		return dev->regs.gacfr;

	case 0x06:
		return dev->regs.ctrl;

	case 0x07:
		return dev->regs.streg;

	case 0x08:
		return dev->regs.idcfr;

	case 0x09:
		return (dev->regs.da >> 8);

	case 0x0a:
		return (dev->regs.da & 0xff);

	case 0x0b:
		return (dev->regs.vptr >> 12) & 0xff;

	case 0x0c:
		return (dev->regs.vptr >> 4) & 0xff;

	case 0x0d:
		return (dev->regs.vptr & 0x0f) << 4;

	case 0x0e:
	case 0x0f:
		if (!(dev->regs.ctrl & 0x80))
			return 0xff;

		set_drq(dev); 

		return chipmem_read(dev, dev->regs.da++, 1);
    }

    return 0;
}


static void
write_hi(uint16_t addr, uint8_t val, priv_t priv)
{
    el2_t *dev = (el2_t *)priv;

    DEBUG("3C503: Write GA address=%04x, val=%04x\n", addr, val);

    switch (addr & 0x0f) {
	case 0x00:
		dev->regs.pstr = val;
		break;

	case 0x01:
		dev->regs.pspr = val;
		break;

	case 0x02:
		dev->regs.dqtr = val;
		break;

	case 0x05:
		if ((dev->regs.gacfr & 0x0f) != (val & 0x0f)) {
			mem_map_disable(&dev->ram_mapping);

			switch (val & 0x0f) {
				case 0: /*ROM mapping*/
					/* FIXME: Implement this when a BIOS is found/generated. */
					break;

				case 9: /*RAM mapping*/
					mem_map_enable(&dev->ram_mapping);
					break;

				default: /*No ROM mapping*/
					break;
			}
		}

		if (!(val & 0x80))
			el2_interrupt(dev, 1);
		else
			el2_interrupt(dev, 0);

		dev->regs.gacfr = val;
		break;

	case 0x06:
		if (val & 1) {
			el2_reset(dev);
			dev->dp8390.ISR.reset = 1;
			dev->regs.ctrl = 0x0b;
			return;
		}

		if ((val & 0x80) != (dev->regs.ctrl & 0x80)) {
			if (val & 0x80)
				dev->regs.streg |= 0x88;
			else
				dev->regs.streg &= ~0x88;
			dev->regs.streg &= ~0x10;
		}
		dev->regs.ctrl = val;
		break;

	case 0x08:
		switch (val & 0xf0) {
			case 0x00:
			case 0x10:
			case 0x20:
			case 0x40:
			case 0x80:
				dev->regs.idcfr = (dev->regs.idcfr & 0x0f) | (val & 0xf0);
				break;

			default:
				DEBUG("Trying to set multiple IRQs: %02x\n", val);
				break;
		}

		switch (val & 0x0f) {
			case 0x00:
			case 0x01:
			case 0x02:
			case 0x04:
				dev->regs.idcfr = (dev->regs.idcfr & 0xf0) | (val & 0x0f);
				break;

			case 0x08:
				break;

			default:
				DEBUG("Trying to set multiple DMA channels: %02x\n", val);
				break;
		}
		break;

	case 0x09:
		dev->regs.da = (val << 8) | (dev->regs.da & 0xff);
		break;

	case 0x0a:
		dev->regs.da = (dev->regs.da & 0xff00) | val;
		break;

	case 0x0b:
		dev->regs.vptr = (val << 12) | (dev->regs.vptr & 0xfff);
		break;

	case 0x0c:
		dev->regs.vptr = (val << 4) | (dev->regs.vptr & 0xff00f);
		break;

	case 0x0d:
		dev->regs.vptr = (val << 4) | (dev->regs.vptr & 0xffff0);
		break;

	case 0x0e:
	case 0x0f:
		if (!(dev->regs.ctrl & 0x80))
			return;

		set_drq(dev); 

		chipmem_write(dev, dev->regs.da++, val, 1);
		break;
    }
}


static void
el2_ioremove(el2_t *dev, uint16_t addr)
{
    io_removehandler(addr, 16,
		     read_lo,NULL,NULL, write_lo,NULL,NULL, dev);

    io_removehandler(addr + 0x400, 16,
		     read_hi,NULL,NULL, write_hi,NULL,NULL, dev);
}


static void
el2_ioset(el2_t *dev, uint16_t addr)
{
    io_sethandler(addr, 16,
		  read_lo,NULL,NULL, write_lo,NULL,NULL, dev);

    io_sethandler(addr + 0x400, 16,
		  read_hi,NULL,NULL, write_hi,NULL,NULL, dev);
}


static uint8_t
ram_read(uint32_t addr, priv_t priv)
{
    el2_t *dev = (el2_t *)priv;

    if ((addr & 0x3fff) >= 0x2000)
	return(0xff);

    return(dev->dp8390.mem[addr & 0x1fff]);
}


static void
ram_write(uint32_t addr, uint8_t val, priv_t priv)
{
    el2_t *dev = (el2_t *)priv;

    if ((addr & 0x3fff) >= 0x2000)
	return;

    dev->dp8390.mem[addr & 0x1fff] = val;
}


static void
el2_close(priv_t priv)
{
    el2_t *dev = (el2_t *)priv;
	
    /* Make sure the platform layer is shut down. */
    network_close();

    el2_ioremove(dev, dev->base_address);

    DEBUG("3C503: closed\n");

    free(dev);
}


static priv_t
el2_init(const device_t *info, UNUSED(void *parent))
{
    uint32_t mac;
    el2_t *dev;

    dev = (el2_t *)mem_alloc(sizeof(el2_t));
    memset(dev, 0x00, sizeof(el2_t));
    dev->maclocal[0] = 0x02;  /* 02:60:8C (3Com OID) */
    dev->maclocal[1] = 0x60;
    dev->maclocal[2] = 0x8C;

    dev->base_address = device_get_config_hex16("base");
    dev->base_irq = device_get_config_int("irq");
    dev->dma_channel = device_get_config_int("dma");
    dev->bios_addr = device_get_config_hex20("bios_addr");
	
    /* See if we have a local MAC address configured. */
    mac = device_get_config_mac("mac", -1);

    /*
     * Make this device known to the I/O system.
     *
     * PnP and PCI devices start with address spaces inactive.
     */
    el2_ioset(dev, dev->base_address);
	
    /* Set up our BIA. */
    if (mac & 0xff000000) {
	/* Generate new local MAC. */
	dev->maclocal[3] = random_generate();
	dev->maclocal[4] = random_generate();
	dev->maclocal[5] = random_generate();
	mac = (((int) dev->maclocal[3]) << 16);
	mac |= (((int) dev->maclocal[4]) << 8);
	mac |= ((int) dev->maclocal[5]);
	device_set_config_mac("mac", mac);
    } else {
	dev->maclocal[3] = (mac>>16) & 0xff;
	dev->maclocal[4] = (mac>>8) & 0xff;
	dev->maclocal[5] = (mac & 0xff);
    }
    memcpy(dev->dp8390.physaddr, dev->maclocal, sizeof(dev->maclocal));

    INFO("I/O=%04x, IRQ=%i, MAC=%02x:%02x:%02x:%02x:%02x:%02x\n",
	 dev->base_address, dev->base_irq,
	 dev->maclocal[0], dev->maclocal[1], dev->maclocal[2],
	 dev->maclocal[3], dev->maclocal[4], dev->maclocal[5]);

    /* Reset the board. */
    el2_reset(dev);

    /* Attach ourselves to the network module. */
    if (! network_attach(dev, dev->maclocal, el2_rx)) {
	el2_close(dev);

	return(NULL);
    }

    /* Map this system into the memory map. */
    mem_map_add(&dev->ram_mapping, dev->bios_addr, 0x4000,
		ram_read,NULL,NULL, ram_write,NULL,NULL,
		NULL, MEM_MAPPING_EXTERNAL, dev);
    mem_map_disable(&dev->ram_mapping);

    return((priv_t)dev);
}


static const device_config_t el2_config[] = {
    {
	"base", "Address", CONFIG_HEX16, "", 0x300,
	{
		{
			"250H", 0x250
		},
		{
			"280H", 0x280
		},
		{
			"2A0H", 0x2a0
		},
		{
			"2E0H", 0x2e0
		},
		{
			"300H", 0x300
		},
		{
			"310H", 0x310
		},
		{
			"330H", 0x330
		},
		{
			"350H", 0x350
		},
		{
			NULL
		}
	}
    },
    {
	"irq", "IRQ", CONFIG_SELECTION, "", 3,
	{
		{
			"IRQ 2", 2
		},
		{
			"IRQ 3", 3
		},
		{
			"IRQ 4", 4
		},
		{
			"IRQ 5", 5
		},
		{
			NULL
		}
	}
    },
    {
	"dma", "DMA", CONFIG_SELECTION, "", 3,
	{
		{
			"DMA 1", 1
		},
		{
			"DMA 2", 2
		},
		{
			"DMA 3", 3
		},
		{
			NULL
		}
	}
    },
    {
	"mac", "MAC Address", CONFIG_MAC, "", -1,
	{
		{
			NULL
		}
	}
    },
    {
	"bios_addr", "BIOS address", CONFIG_HEX20, "", 0xCC000,
	{
		{
			"DC00H", 0xDC000
		},
		{
			"D800H", 0xD8000
		},
		{
			"C800H", 0xC8000
		},
		{
			"CC00H", 0xCC000
		},
		{
			NULL
		}
	}
    },
    {
	NULL,
    }
};


const device_t el2_device = {
    "3Com EtherLink II",
    DEVICE_ISA,
    0,
    NULL,
    el2_init, el2_close, NULL,
    NULL, NULL, NULL, NULL,
    el2_config
};
