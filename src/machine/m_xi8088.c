/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the Xi8088 open-source machine.
 *
 * Version:	@(#)m_xi8088.c	1.0.17	2021/03/18
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2021 Fred N. van Kempen.
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
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <86box/86box.h>
#include <86box/timer.h>
#include "../cpu/cpu.h"
#include <86box/mem.h>
#include <86box/rom.h>
#include <86box/device.h>
#include <86box/nvr.h>
#include <86box/dma.h>
#include <86box/nmi.h>
#include <86box/pic.h>
#include <86box/pit.h>
#include <86box/keyboard.h>
#include <86box/parallel.h>
#include <86box/fdd.h>
#include <86box/fdc.h>
#include <86box/hdc.h>
#include <86box/plat.h>
#include <86box/machine.h>


typedef struct {
    int8_t	turbo,
		turbo_setting,
		bios_128k;
    char	pad;
} xi8088_t;


static int
bios_128k_get(xi8088_t *dev)
{
    return(dev->bios_128k);
}


static void
bios_128k_set(xi8088_t *dev, int val)
{
    dev->bios_128k = (int8_t)val;
}


static uint8_t
turbo_get(priv_t priv)
{
    xi8088_t *dev = (xi8088_t *)priv;

    return(dev->turbo);
}


static void
turbo_set(priv_t priv, uint8_t val)
{
    xi8088_t *dev = (xi8088_t *)priv;

    if (! dev->turbo_setting) return;

    dev->turbo = (int8_t)val;

    pc_set_speed(dev->turbo);
}


static void
xi_close(priv_t priv)
{
    xi8088_t *dev = (xi8088_t *)priv;

    free(dev);
}


static priv_t
xi_init(const device_t *info, UNUSED(void *arg))
{
    xi8088_t *dev;
    priv_t kbd;

    dev = (xi8088_t *)mem_alloc(sizeof(xi8088_t));
    memset(dev, 0x00, sizeof(xi8088_t));

    /* Add machine device to system. */
    device_add_ex(info, (priv_t)dev);

    machine_common_init();

    nmi_init();
    pic2_init();

    device_add(&at_nvr_device);

    /*
     * Get the selected Turbo Speed setting.
     *
     * Even though the bios by default turns the turbo off when
     * controlling by hotkeys, we always start at full speed.
     */
    dev->turbo = 1;
    dev->turbo_setting = (int8_t)device_get_config_int("turbo_setting");

    if (biosmask < 0x010000)
	bios_128k_set(dev, 0);
     else
	bios_128k_set(dev, 1);

    if (bios_128k_get(dev))
	rom_add_upper_bios();

    /*
     * TODO: set UMBs?
     * See if PCem always sets when we have > 640KB ram and avoids
     * conflicts when a peripheral uses the same memory space
     */

    kbd = device_add(&keyboard_ps2_xi8088_device);

    /* Tell keyboard driver we want to handle some stuff here. */
    keyboard_at_set_funcs(kbd, turbo_get, turbo_set, dev);

    device_add(&fdc_xt_device);

    return((priv_t)dev);
}


static const device_config_t xi_config[] = {
    {
	"turbo_setting", "Turbo", CONFIG_SELECTION, "", 0,
	{
		{ "Always at selected speed", 0		},
		{ "Hotkeys (starts off)", 1		},
		{ NULL					}
	}
    },
    {
	NULL
    }
};


static const machine_t xi_info = {
    MACHINE_ISA /*| MACHINE_AT*/ | MACHINE_PS2,
    0,
    64, 1024, 128, 128, -1,
    {{"Intel",cpus_8088},{"NEC",cpus_nec}}
};

const device_t m_xi8088 = {
    "Xi8088",
    DEVICE_ROOT,
    0,
    L"malinov/xi8088",
    xi_init, xi_close, NULL,
    NULL, NULL, NULL,
    &xi_info,
    xi_config
};
