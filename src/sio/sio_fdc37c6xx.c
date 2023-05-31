/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the SMC FDC37C663 and FDC37C665 Super
 *		I/O Chips.
 *
 * Version:	@(#)sio_fdc37c66x.c	1.0.14	2021/03/23
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2021 Fred N. van Kempen.
 *		Copyright 2016-2021 Miran Grca.
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
#include <86box/device.h>
#include <86box/timer.h>
#include <86box/pci.h>
#include <86box/parallel.h>
#include <86box/serial.h>
#include <86box/fdd.h>
#include <86box/fdc.h>
#include <86box/hdc.h>
#include <86box/hdc_ide.h>
#include <86box/plat.h>
#include <86box/sio.h>


typedef struct {
    uint8_t	chip_id,
		cur_reg,
		tries;

    uint16_t	com3_addr,
		com4_addr;

    fdc_t	*fdc;

    uint8_t	regs[16];
} fdc37c66x_t;


static void
set_com34_addr(fdc37c66x_t *dev)
{
    switch (dev->regs[1] & 0x60) {
	case 0x00:
		dev->com3_addr = 0x0338;
		dev->com4_addr = 0x0238;
		break;

	case 0x20:
		dev->com3_addr = 0x03e8;
		dev->com4_addr = 0x02e8;
		break;

	case 0x40:
		dev->com3_addr = 0x03e8;
		dev->com4_addr = 0x02e0;
		break;

	case 0x60:
		dev->com3_addr = 0x0220;
		dev->com4_addr = 0x0228;
		break;
    }
}


static void
set_serial_addr(fdc37c66x_t *dev, int port)
{
    uint8_t shift = (port << 4);

#if 0
    serial_remove(0);
#endif
    if (dev->regs[2] & (4 << shift)) switch (dev->regs[2] & (3 << shift)) {
	case 0:
		serial_setup(port, SERIAL1_ADDR, SERIAL1_IRQ);
		break;

	case 1:
		serial_setup(port, SERIAL2_ADDR, SERIAL2_IRQ);
		break;

	case 2:
		serial_setup(port, dev->com3_addr, 4);
		break;

	case 3:
		serial_setup(port, dev->com4_addr, 3);
		break;
    }
}


static void
lpt1_handler(fdc37c66x_t *dev)
{
#if 0
    parallel_remove(0);
#endif

    switch (dev->regs[1] & 3) {
	case 1:
		parallel_setup(0, 0x03bc);
		break;

	case 2:
		parallel_setup(0, 0x0378);
		break;

	case 3:
		parallel_setup(0, 0x0278);
		break;
    }
}


static void
fdc_handler(fdc37c66x_t *dev)
{
    fdc_remove(dev->fdc);

    if (dev->regs[0] & 0x10)
	fdc_set_base(dev->fdc, (dev->regs[5] & 0x01) ? 0x0370 : 0x03f0);
}


#if 0
static void
ide_handler(fdc37c66x_t *dev)
{
    ide_sec_disable();

    ide_set_base(0, (dev->regs[0x05] & 0x02) ? 0x170 : 0x1f0);
    ide_set_side(0, (dev->regs[0x05] & 0x02) ? 0x376 : 0x3f6);

    if (dev->regs[0x00] & 0x01)
	ide_sec_enable();
}
#endif


static uint8_t
fdc37c66x_in(uint16_t port, priv_t priv)
{
    fdc37c66x_t *dev = (fdc37c66x_t *)priv;
    uint8_t ret = 0x00;

    if (dev->tries == 2) {
	if (port == 0x03f1)
		ret = dev->regs[dev->cur_reg];
    }

    return(ret);
}


static void
fdc37c66x_out(uint16_t port, uint8_t val, priv_t priv)
{
    fdc37c66x_t *dev = (fdc37c66x_t *)priv;
    uint8_t valxor = 0;

    if (dev->tries == 2) {
	if (port == 0x03f0) {
		if (val == 0xaa)
			dev->tries = 0;
		else
			dev->cur_reg = val;
	} else {
		if (dev->cur_reg > 15)
			return;

		valxor = val ^ dev->regs[dev->cur_reg];
		dev->regs[dev->cur_reg] = val;

		switch (dev->cur_reg) {
			case 0:
#if 0
				if (valxor & 0x01)
					ide_handler(dev);
#endif
				if (valxor & 0x10)
					fdc_handler(dev);
				break;
			case 1:
				if (valxor & 0x03)
					lpt1_handler(dev);
				if (valxor & 0x60) {
					set_com34_addr(dev);
					set_serial_addr(dev, 0);
					set_serial_addr(dev, 1);
				}
				break;

			case 2:
				if (valxor & 0x07)
					set_serial_addr(dev, 0);

				if (valxor & 0x70) 
					set_serial_addr(dev, 1);
				break;

			case 3:
				if (valxor & 0x02)
					fdc_update_enh_mode(dev->fdc, (dev->regs[3] & 2) ? 1 : 0);
				break;

			case 4:
				if (valxor & 0x10)
					set_serial_addr(dev, 0);
				if (valxor & 0x20)
					set_serial_addr(dev, 1);
				break;

			case 5:
				if (valxor & 0x01)
					fdc_handler(dev);
#if 0
				if (valxor & 0x02)
					ide_handler(dev);
#endif
				if (valxor & 0x18)
					fdc_update_densel_force(dev->fdc, (dev->regs[5] & 0x18) >> 3);
				if (valxor & 0x20)
					fdc_set_swap(dev->fdc, (dev->regs[5] & 0x20) >> 5);
				break;
		}
	}
    } else {
	if ((port == 0x03f0) && (val == 0x55)) 
		dev->tries++;
    }
}


static void
fdc37c66x_reset(fdc37c66x_t *dev)
{
    dev->com3_addr = 0x338;
    dev->com4_addr = 0x238;

#if 0
    serial_remove(0);
#endif
    serial_setup(0, SERIAL1_ADDR, SERIAL1_IRQ);

#if 0
    serial_remove(1);
#endif
    serial_setup(1, SERIAL2_ADDR, SERIAL2_IRQ);

#if 0
    parallel_remove(1);
#endif

#if 0
    parallel_remove(0);
#endif
    parallel_setup(0, 0x0378);

    fdc_reset(dev->fdc);

    dev->tries = 0;
    memset(dev->regs, 0x00, sizeof(dev->regs));

    dev->regs[0x0] = 0x3a;
    dev->regs[0x1] = 0x9f;
    dev->regs[0x2] = 0xdc;
    dev->regs[0x3] = 0x78;
    dev->regs[0x6] = 0xff;
    dev->regs[0xd] = dev->chip_id;
    dev->regs[0xe] = 0x01;

#if 0
    ide_handler(dev);
#endif
}


static void
fdc37c66x_close(priv_t priv)
{
    fdc37c66x_t *dev = (fdc37c66x_t *)priv;

    free(dev);
}


static priv_t
fdc37c66x_init(const device_t *info, UNUSED(void *parent))
{
    fdc37c66x_t *dev;

    dev = (fdc37c66x_t *)mem_alloc(sizeof(fdc37c66x_t));
    memset(dev, 0x00, sizeof(fdc37c66x_t));
    dev->chip_id = info->local;

    dev->fdc = device_add(&fdc_at_smc_device);

    io_sethandler(0x03f0, 2,
		  fdc37c66x_in,NULL,NULL, fdc37c66x_out,NULL,NULL, dev);

    fdc37c66x_reset(dev);

    return(dev);
}


/* These seem to differ only in the chip ID, according to their datasheets. */
const device_t fdc37c663_device = {
    "SMC FDC37C663 Super I/O",
    0,
    0x63,
    NULL,
    fdc37c66x_init, fdc37c66x_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t fdc37c665_device = {
    "SMC FDC37C665 Super I/O",
    0,
    0x65,
    NULL,
    fdc37c66x_init, fdc37c66x_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t fdc37c666_device = {
    "SMC FDC37C666 Super I/O",
    0,
    0x66,
    NULL,
    fdc37c66x_init, fdc37c66x_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
