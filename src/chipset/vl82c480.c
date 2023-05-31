/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the VLSI 82C480 chipset.
 *
 * Version:	@(#)vl82c480.c	1.0.2	2021/04/30
 *
 * Authors:	Altheos, <altheos@varcem.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2020-2021 Altheos.
 *		Copyright 2020-2021 Miran Grca.
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
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <86box/86box.h>
#include "../cpu/cpu.h"
#include <86box/io.h>
#include <86box/mem.h>
#include <86box/rom.h>
#include <86box/device.h>
#include <86box/plat.h>
#include <86box/port92.h>
#include <86box/vl82c480.h>


typedef struct {
    uint8_t	reg_idx,
		regs[256];
} vl82c480_t;


static int
vl82c480_shflags(uint8_t access)
{
    int ret = MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL;

    switch (access) {
	case 0x00:
	default:
		ret = MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL;
		break;

	case 0x01:
		ret = MEM_READ_EXTERNAL | MEM_WRITE_INTERNAL;
		break;

	case 0x02:
		ret = MEM_READ_INTERNAL | MEM_WRITE_EXTERNAL;
		break;

	case 0x03:
		ret = MEM_READ_INTERNAL | MEM_WRITE_INTERNAL;
		break;
    }

    return(ret);
}


static void 
shadow_recalc(vl82c480_t *dev)
{
    int i, j;
    uint32_t base;
    uint8_t access;

    shadowbios = 0;
    shadowbios_write = 0;

    for (i = 0; i < 6; i++) {
		for (j = 0; j < 8; j += 2) {
			base = 0x000a0000 + (i << 16) + (j << 13);
			access = (dev->regs[0x0d + i] >> j) & 0x03;
			mem_set_mem_state(base, 0x4000, vl82c480_shflags(access));
			shadowbios |= ((base >= 0xe0000) && (access & 0x02));
			shadowbios_write |= ((base >= 0xe0000) && (access & 0x01));
		}
    }

    flushmmucache();
}


static uint8_t 
vl82c480_in(uint16_t port, priv_t priv)
{
    vl82c480_t *dev = (vl82c480_t *)priv;
    uint8_t ret = 0xff;

    switch (port) {
	    case 0xec:
		    ret = dev->reg_idx;
		    break;

	    case 0xed:
		    ret = dev->regs[dev->reg_idx];
		    break;

	    case 0xee:
		    if (!mem_a20_alt)
			    outb(0x92, inb(0x92) | 2);
		    break;

	    case 0xef:
		    cpu_reset(0);
		    cpu_set_edx();
		    break;
    }

    return(ret);
}


static void
vl82c480_out(uint16_t port, uint8_t val, priv_t priv)
{
    vl82c480_t *dev = (vl82c480_t *)priv;

    switch (port) {
	case 0xec:
		dev->reg_idx = val;
		break;

	case 0xed:
		if (dev->reg_idx >= 0x01 && dev->reg_idx <= 0x24) {
			switch (dev->reg_idx) {
				case 0x05:
					dev->regs[dev->reg_idx] = (dev->regs[dev->reg_idx] & 0x10) | (val & 0xef);
					break;

				case 0x0d: case 0x0e: case 0x0f: case 0x10:
				case 0x11: case 0x12:
					dev->regs[dev->reg_idx] = val;
					shadow_recalc(dev);
					break;

				default:
					dev->regs[dev->reg_idx] = val;
					break;
			}
		}
		break;

	case 0xee:
		if (mem_a20_alt)
			outb(0x92, inb(0x92) & ~2);
		break;
    }
}


static void
vl82c480_close(priv_t priv)
{
    vl82c480_t *dev = (vl82c480_t *)priv;

    free(dev);
}


static priv_t
vl82c480_init(UNUSED(const device_t *info), UNUSED(void *parent))
{
    vl82c480_t *dev;
    
    dev = (vl82c480_t *)mem_alloc(sizeof(vl82c480_t));
    memset(dev, 0x00, sizeof(vl82c480_t));

    dev->regs[0x00] = 0x90;
    dev->regs[0x01] = 0xff;
    dev->regs[0x02] = 0x8a;
    dev->regs[0x03] = 0x88;
    dev->regs[0x06] = 0x1b;
    dev->regs[0x08] = 0x38;

    io_sethandler(0x00ec, 4,  
		  vl82c480_in,NULL,NULL, vl82c480_out,NULL,NULL, dev);

    device_add(&port92_device);

    return(dev);
}


const device_t vl82c480_device = {
    "VLSI VL82c480",
    0,
    0,
    NULL,
    vl82c480_init, vl82c480_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
