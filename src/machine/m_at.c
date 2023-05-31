/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Standard PC/AT implementation.
 *
 * Version:	@(#)m_at.c	1.0.17	2021/03/09
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
#include <string.h>
#include <wchar.h>
#include <86box/86box.h>
#include <86box/timer.h>
#include "../cpu/cpu.h"
#include <86box/mem.h>
#include <86box/device.h>
#include <86box/nvr.h>
#include <86box/pic.h>
#include <86box/pit.h>
#include <86box/ppi.h>
#include <86box/dma.h>
#include <86box/keyboard.h>
#include <86box/fdd.h>
#include <86box/fdc.h>
#include <86box/hdc.h>
#include <86box/plat.h>
#include <86box/machine.h>


void
m_at_common_init(void)
{
    machine_common_init();

    pit_set_out_func(&pit, 1, m_at_refresh_timer);

    pic2_init();
    dma16_init();

    device_add(&at_nvr_device);
}


void
m_at_init(void)
{
    m_at_common_init();

    device_add(&keyboard_at_device);
}


void
m_at_ps2_init(void)
{
    m_at_common_init();

    device_add(&keyboard_ps2_device);
}


void
m_at_common_ide_init(void)
{
    m_at_common_init();

    device_add(&ide_isa_device);
}


void
m_at_ide_init(void)
{
    m_at_init();

    device_add(&ide_isa_device);
}


void
m_at_ps2_ide_init(void)
{
    m_at_ps2_init();

    device_add(&ide_isa_device);
}


static priv_t
at_init(const device_t *info, UNUSED(void *arg))
{
    device_add_ex(info, (priv_t)arg);

    m_at_init();

    switch (info->local) {
	case 0:			/* IBM PC-AT */
	case 1:			/* IBM PC-XT286 */
		break;

	default:		/* clones */
		break;
    }

    mem_remap_top(384);

    device_add(&fdc_at_device);

    return(arg);
}


static const CPU cpus_ibmat[] = {
    { "286/6", CPU_286, 6000000, 1, 0, 0, 0, 0, 0, 3,3,3,3, 1 },
    { "286/8", CPU_286, 8000000, 1, 0, 0, 0, 0, 0, 3,3,3,3, 1 },
    { NULL }
};

static const machine_t at_info = {
    MACHINE_ISA | MACHINE_AT,
    0,
    256, 15872, 128, 64, -1,
    {{"",cpus_ibmat}}
};

const device_t m_at = {
    "IBM PC/AT",
    DEVICE_ROOT,
    0,
    L"ibm/at",
    at_init, NULL, NULL,
    NULL, NULL, NULL,
    &at_info,
    NULL
};


static const CPU cpus_ibmxt286[] = {
    { "286/6", CPU_286, 6000000, 1, 0, 0, 0, 0, 0, 2,2,2,2, 1 },
    { NULL }
};

static const machine_t xt286_info = {
    MACHINE_ISA | MACHINE_AT,
    0,
    256, 15872, 128, 128, -1,
    {{"",cpus_ibmxt286}}
};

const device_t m_xt286 = {
    "IBM PC/XT286",
    DEVICE_ROOT,
    1,
    L"ibm/xt286",
    at_init, NULL, NULL,
    NULL, NULL, NULL,
    &xt286_info,
    NULL
};


static const machine_t dtk286_info = {
    MACHINE_ISA | MACHINE_AT,
    0,
    256, 15872, 128, 64, -1,
    {{"",cpus_286}}
};

/* DTK PTM-1xxx (1010, 1030, 1230, 1630, 1739) 80286. */
const device_t m_dtk_ptm1000 = {
    "DTK 286 (PTM-1xxx)",
    DEVICE_ROOT,
    10,
    L"dtk/286",
    at_init, NULL, NULL,
    NULL, NULL, NULL,
    &dtk286_info,
    NULL
};


/* Emulate a DRAM refresh cycle. */
void
m_at_refresh_timer(int new_out, int old_out)
{
    if (new_out && !old_out)
        ppi.pb ^= 0x10;
}
