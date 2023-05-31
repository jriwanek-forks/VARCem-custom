/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the ACC 2036 chipset.
 *
 * Version:	@(#)acc2036.c	1.0.1	2021/03/18
 *
 * Authors:	Altheos, <altheos@varcem.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2020 Altheos.
 *		Copyright 2019 Sarah Walker.
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
#include <86box/acc2036.h>


typedef struct {
    int		reg_idx;
    uint8_t	regs[256];
} acc2036_t;


static void 
shadow_recalc(acc2036_t *dev)
{
    if (dev->regs[0x2c] & 0x08) {
	switch (dev->regs[0x22] & 0x03) {
		case 0x00:
			mem_set_mem_state(0xf0000, 0x10000,
			MEM_READ_EXTERNAL|MEM_WRITE_EXTERNAL);
			break;

		case 0x01:
			mem_set_mem_state(0xf0000, 0x10000,
			MEM_READ_INTERNAL|MEM_WRITE_EXTERNAL);
			break;

		case 0x02:
			mem_set_mem_state(0xf0000, 0x10000,
			MEM_READ_EXTERNAL|MEM_WRITE_INTERNAL);
			break;

		case 0x03:
			mem_set_mem_state(0xf0000, 0x10000,
			MEM_READ_INTERNAL|MEM_WRITE_INTERNAL);
			break;
	}
    } else {
	    mem_set_mem_state(0xf0000, 0x10000,
			      MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
    }

    if (dev->regs[0x2c] & 0x04) switch (dev->regs[0x22] & 0x03) {
	       case 0x00:
	        mem_set_mem_state(0xe0000, 0x10000,
				  MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
	        break;

        case 0x01:
	        mem_set_mem_state(0xe0000, 0x10000,
				  MEM_READ_INTERNAL | MEM_WRITE_EXTERNAL);
	        break;

        case 0x02:
	        mem_set_mem_state(0xe0000, 0x10000,
				  MEM_READ_EXTERNAL | MEM_WRITE_INTERNAL);
	        break;

        case 0x03:
	        mem_set_mem_state(0xe0000, 0x10000,
				  MEM_READ_INTERNAL | MEM_WRITE_INTERNAL);
	        break;
    } else {
        mem_set_mem_state(0xe0000, 0x10000,
			  MEM_READ_EXTERNAL | MEM_WRITE_EXTERNAL);
    }
}


static uint8_t 
acc2036_in(uint16_t port, priv_t priv)
{
    acc2036_t *dev = (acc2036_t *)priv;

    if (port & 1)  {
	if (dev->reg_idx >= 0xbc)
		return(0xff);

	return(dev->regs[dev->reg_idx]);
    }

    return(dev->reg_idx);
}


static void 
acc2036_out(uint16_t port, uint8_t val, priv_t priv)
{
    acc2036_t *dev = (acc2036_t *)priv;

    if (port & 1) {
	dev->regs[dev->reg_idx] = val;

	switch (dev->reg_idx) {
		case 0x22: case 0x2c:
		    	shadow_recalc(dev);
			break;
	    
		case 0x23:
			mem_map_disable(&ram_remapped_mapping);
			if (val & 0x10 ) {
				if (dev->regs[0x2c] & 0x0c)
					mem_remap_top(256);
				else
					mem_remap_top(384);
			}
			break;
	}
    } else
	dev->reg_idx = val;
}


static void
acc2036_close(priv_t priv)
{
    acc2036_t *dev = (acc2036_t *)priv;

    free(dev);
}


static priv_t
acc2036_init(UNUSED(const device_t *info), UNUSED(void *parent))
{
    acc2036_t *dev;

    dev = (acc2036_t *)mem_alloc(sizeof(acc2036_t));
    memset(dev, 0x00, sizeof(acc2036_t));
	
    io_sethandler(0x00f2, 2,
		  acc2036_in,NULL,NULL, acc2036_out,NULL,NULL, dev);	

    return((priv_t)dev);
}


const device_t acc2036_device = {
    "ACC 2036",
    0, 0, NULL,
    acc2036_init, acc2036_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
