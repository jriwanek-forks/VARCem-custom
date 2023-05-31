/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implement I/O ports and their operations.
 *
 * Version:	@(#)io.c	1.0.6	2019/05/17
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
 *		Copyright 2016-2018 Miran Grca.
 *		Copyright 2008-2018 Sarah Walker.
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
#include <86box/86box.h>
#include <86box/io.h>
#include "cpu/cpu.h"


#define NPORTS		65536		/* PC/AT supports 64K ports */


typedef struct _io_ {
    uint8_t	(*inb)(uint16_t, priv_t);
    void	(*outb)(uint16_t, uint8_t, priv_t);

    uint16_t	(*inw)(uint16_t, priv_t);
    void	(*outw)(uint16_t, uint16_t, priv_t);

    uint32_t	(*inl)(uint16_t, priv_t);
    void	(*outl)(uint16_t, uint32_t, priv_t);

    priv_t	priv;

    struct _io_ *prev, *next;
} io_t;


static io_t	**io = NULL,
		**io_last = NULL;
  

/* Add an I/O handler to the chain. */
static void
io_insert(int c, io_t *q)
{
    io_t *p;

    p = io_last[c];
    if (p != NULL) {
	p->next = q;
	q->prev = p;
    } else {
	io[c] = q;
	q->prev = NULL;
    }
    io_last[c] = q;
}


/* Remove I/O handler from the chain. */
static void
io_unlink(int c)
{
    io_t *p = io[c];

    if (p->prev != NULL)
	p->prev->next = p->next;
    else
	io[c] = p->next;

    if (p->next != NULL)
	p->next->prev = p->prev;
    else
	io_last[c] = p->prev;

    free(p);
}


#ifdef IO_CATCH
static uint8_t
catch_inb(uint16_t addr, priv_t p)
{
    DEBUG("IO: inb(%04x)\n", addr);
    return(0xff);
}

static uint16_t
catch_inw(uint16_t addr, priv_t p)
{
    DEBUG("IO: inw(%04x)\n", addr);
    return(0xffff);
}

static uint32_t
catch_inl(uint16_t addr, priv_t p)
{
    DEBUG("IO: inl(%04x)\n", addr);
    return(0xffffffff);
}


static void
catch_outb(uint16_t addr, uint8_t val, priv_t p)
{
    DEBUG("IO: outb(%04x, %02x)\n", addr, val);
}

static void
catch_outw(uint16_t addr, uint16_t val, priv_t p)
{
    DEBUG("IO: outw(%04x, %04x)\n", addr, val);
}

static void
catch_outl(uint16_t addr, uint32_t val, priv_t p)
{
    DEBUG("IO: outl(%04x, %08lx)\n", addr, val);
}


/* Add a catch handler to a port. */
static void
catch_add(int port)
{
    io_t *p;

    /* Create new handler. */
    p = (io_t *)mem_alloc(sizeof(io_t));
    memset(p, 0x00, sizeof(io_t));
    p->inb  = catch_inb;
    p->outb = catch_outb;
    p->inw  = catch_inw;
    p->outw = catch_outw;
    p->inl  = catch_inl;
    p->outl = catch_outl;

    /* Add to chain. */
    io_insert(port, p);
}


/* If the only handler is the catcher, remove it. */
static void
catch_del(int port)
{
    if ((io[port] != NULL) && (io[port]->inb == catch_inb))
	io_unlink(port);
}
#endif


void
io_reset(void)
{
    io_t *p, *q;
    int c;

    INFO("IO: initializing\n");
    if (io == NULL) {
	/* Allocate the arrays, one-time only. */
	c = sizeof(io_t **) * NPORTS;
	io = (io_t **)mem_alloc(c);
	memset(io, 0x00, c);
	io_last = (io_t **)mem_alloc(c);
	memset(io_last, 0x00, c);
    }

    /* Clear both arrays. */
    for (c = 0; c < NPORTS; c++) {
       	if (io_last[c] != NULL) {
		/* At least one handler, free all handlers. */
		p = io_last[c];
		while (p != NULL) {
			q = p->prev;
			free(p);
			p = q;
		}
	}

	/* Reset handler. */
	io[c] = io_last[c] = NULL;

#ifdef IO_CATCH
	/* Add a default (catch) handler. */
	catch_add(c);
#endif
    }
}


void
io_sethandler(uint16_t base, int size, 
	      uint8_t (*f_inb)(uint16_t addr, priv_t priv),
	      uint16_t (*f_inw)(uint16_t addr, priv_t priv),
	      uint32_t (*f_inl)(uint16_t addr, priv_t priv),
	      void (*f_outb)(uint16_t addr, uint8_t val, priv_t priv),
	      void (*f_outw)(uint16_t addr, uint16_t val, priv_t priv),
	      void (*f_outl)(uint16_t addr, uint32_t val, priv_t priv),
	      priv_t priv)
{
    io_t *p;
    int c;

    for (c = 0; c < size; c++) {
	/* Create entry for the new handler. */
	p = (io_t *)mem_alloc(sizeof(io_t));
	memset(p, 0x00, sizeof(io_t));
	p->inb = f_inb; p->inw = f_inw; p->inl = f_inl;
	p->outb = f_outb; p->outw = f_outw; p->outl = f_outl;
	p->priv = priv;

#ifdef IO_CATCH
	/* Unlink the catcher if that is the only handler. */
	if (log_level < LOG_DETAIL)
		catch_del(base + c);
#endif

	/* Insert this new handler. */
	io_insert(base + c, p);
    }
}


void
io_removehandler(uint16_t base, int size,
	uint8_t (*f_inb)(uint16_t addr, priv_t priv),
	uint16_t (*f_inw)(uint16_t addr, priv_t priv),
	uint32_t (*f_inl)(uint16_t addr, priv_t priv),
	void (*f_outb)(uint16_t addr, uint8_t val, priv_t priv),
	void (*f_outw)(uint16_t addr, uint16_t val, priv_t priv),
	void (*f_outl)(uint16_t addr, uint32_t val, priv_t priv),
	priv_t priv)
{
    io_t *p;
    int c;

    for (c = 0; c < size; c++) {
	p = io[base + c];
	if (p == NULL)
		continue;

	for (; p != NULL; p = p->next) {
		if ((p->inb == f_inb) && (p->inw == f_inw) &&
		    (p->inl == f_inl) && (p->outb == f_outb) &&
		    (p->outw == f_outw) && (p->outl == f_outl) &&
		    (p->priv == priv)) {
			io_unlink(base + c);
			p = NULL;
			break;
		}
	}
    }
}


#ifdef PC98
void
io_sethandler_interleaved(uint16_t base, int size,
	uint8_t (*f_inb)(uint16_t addr, priv_t priv),
	uint16_t (*f_inw)(uint16_t addr, priv_t priv),
	uint32_t (*f_inl)(uint16_t addr, priv_t priv),
	void (*f_outb)(uint16_t addr, uint8_t val, priv_t priv),
	void (*f_outw)(uint16_t addr, uint16_t val, priv_t priv),
	void (*f_outl)(uint16_t addr, uint32_t val, priv_t priv),
	priv_t priv)
{
    io_t *p, *q;
    int c;

    size <<= 2;
    for (c = 0; c < size; c += 2) {
	p = last_handler(base + c);
	q = (io_t *)mem_alloc(sizeof(io_t));
	memset(q, 0x00, sizeof(io_t));
	if (p != NULL) {
		p->next = q;
		q->prev = p;
	} else {
		io[base + c] = q;
		q->prev = NULL;
	}

	q->inb = f_inb; q->inw = f_inw; q->inl = f_inl;

	q->outb = f_outb; q->outw = f_outw; q->outl = f_outl;

	q->priv = priv;
    }
}


void
io_removehandler_interleaved(uint16_t base, int size,
	uint8_t (*f_inb)(uint16_t addr, priv_t priv),
	uint16_t (*f_inw)(uint16_t addr, priv_t priv),
	uint32_t (*f_inl)(uint16_t addr, priv_t priv),
	void (*f_outb)(uint16_t addr, uint8_t val, priv_t priv),
	void (*f_outw)(uint16_t addr, uint16_t val, priv_t priv),
	void (*f_outl)(uint16_t addr, uint32_t val, priv_t priv),
	priv_t priv)
{
    io_t *p;
    int c;

    size <<= 2;
    for (c = 0; c < size; c += 2) {
	p = io[base + c];
	if (p == NULL)
		return;
	while (p != NULL) {
		if ((p->inb == f_inb) && (p->inw == f_inw) &&
		    (p->inl == f_inl) && (p->outb == f_outb) &&
		    (p->outw == f_outw) && (p->outl == f_outl) &&
		    (p->priv == priv)) {
			if (p->prev != NULL)
				p->prev->next = p->next;
			if (p->next != NULL)
				p->next->prev = p->prev;
			free(p);
			break;
		}
		p = p->next;
	}
    }
}
#endif


uint8_t
inb(uint16_t port)
{
    uint8_t r = 0xff;
    io_t *p;

    p = io[port];
    while(p != NULL) {
	if (p->inb != NULL)
		r &= p->inb(port, p->priv);
	p = p->next;
    }

#ifdef IO_TRACE
    if (CS == IO_TRACE)
	DEBUG("IOTRACE(%04x): inb(%04x)=%02x\n", IO_TRACE, port, r);
#endif

    return(r);
}


void
outb(uint16_t port, uint8_t val)
{
    io_t *p;

    if (io[port] != NULL) {
	p = io[port];
	while (p != NULL) {
		if (p->outb != NULL)
			p->outb(port, val, p->priv);
		p = p->next;
	}
    }

#ifdef IO_TRACE
    if (CS == IO_TRACE)
	DEBUG("IOTRACE(%04X): outb(%04x,%02x)\n", IO_TRACE, port, val);
#endif
}


uint16_t
inw(uint16_t port)
{
    io_t *p;

    p = io[port];
    while(p != NULL) {
	if (p->inw != NULL)
		return(p->inw(port, p->priv));
	p = p->next;
    }

    return(inb(port) | (inb(port + 1) << 8));
}


void
outw(uint16_t port, uint16_t val)
{
    io_t *p;

    p = io[port];
    while(p != NULL) {
	if (p->outw != NULL) {
		p->outw(port, val, p->priv);
		return;
	}
	p = p->next;
    }

    outb(port,val & 0xff);
    outb(port+1,val>>8);
}


uint32_t
inl(uint16_t port)
{
    io_t *p;

    p = io[port];
    while(p != NULL) {
	if (p->inl != NULL)
		return(p->inl(port, p->priv));
	p = p->next;
    }

    return(inw(port) | (inw(port + 2) << 16));
}


void
outl(uint16_t port, uint32_t val)
{
    io_t *p;

    p = io[port];
    while(p != NULL) {
	if (p->outl != NULL) {
		p->outl(port, val, p->priv);
		return;
	}
	p = p->next;
    }

    outw(port, val);
    outw(port + 2, val >> 16);
}
