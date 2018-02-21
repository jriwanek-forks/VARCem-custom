/*
 * VARCem	Virtual Archaelogical Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the LPT style parallel ports.
 *
 * Version:	@(#)lpt.c	1.0.1	2018/02/14
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017,2018 Fred N. van Kempen.
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
#include "emu.h"
#include "io.h"
#include "lpt.h"
#include "sound/snd_lpt_dac.h"
#include "sound/snd_lpt_dss.h"


char lpt_device_names[3][16];


static struct
{
        const char *name;
        const char *internal_name;
        lpt_device_t *device;
} lpt_devices[] =
{
        {"None",                         "none",           NULL},
        {"Disney Sound Source",          "dss",            &dss_device},
        {"LPT DAC / Covox Speech Thing", "lpt_dac",        &lpt_dac_device},
        {"Stereo LPT DAC",               "lpt_dac_stereo", &lpt_dac_stereo_device},
        {"", "", NULL}
};

char *lpt_device_get_name(int id)
{
        if (strlen((char *)lpt_devices[id].name) == 0)
                return NULL;
        return (char *)lpt_devices[id].name;
}
char *lpt_device_get_internal_name(int id)
{
        if (strlen((char *)lpt_devices[id].internal_name) == 0)
                return NULL;
        return (char *)lpt_devices[id].internal_name;
}

static lpt_device_t *lpt_device_ts[3];
static void *lpt_device_ps[3];

void lpt_devices_init()
{
	int i = 0;
        int c;

	for (i = 0; i < 3; i++) {
		c = 0;

	        while ((char *)strcmp(lpt_devices[c].internal_name, lpt_device_names[i]) && strlen((char *)lpt_devices[c].internal_name) != 0)
        	        c++;

	        if (strlen((char *)lpt_devices[c].internal_name) == 0)
        	        lpt_device_ts[i] = NULL;
	        else
        	{
                	lpt_device_ts[i] = lpt_devices[c].device;
	                if (lpt_device_ts[i])
        	                lpt_device_ps[i] = lpt_device_ts[i]->init();
	        }
	}
}

void lpt_devices_close()
{
	int i = 0;

	for (i = 0; i < 3; i++) {
	        if (lpt_device_ts[i])
        	        lpt_device_ts[i]->close(lpt_device_ps[i]);
	        lpt_device_ts[i] = NULL;
	}
}

static uint8_t lpt_dats[3], lpt_ctrls[3];

void lpt_write(int i, uint16_t port, uint8_t val, void *priv)
{
        switch (port & 3)
        {
                case 0:
                if (lpt_device_ts[i])
                        lpt_device_ts[i]->write_data(val, lpt_device_ps[i]);
                lpt_dats[i] = val;
                break;
                case 2:
                if (lpt_device_ts[i])
                        lpt_device_ts[i]->write_ctrl(val, lpt_device_ps[i]);
                lpt_ctrls[i] = val;
                break;
        }
}
uint8_t lpt_read(int i, uint16_t port, void *priv)
{
        switch (port & 3)
        {
                case 0:
                return lpt_dats[i];
                case 1:
                if (lpt_device_ts[i])
                        return lpt_device_ts[i]->read_status(lpt_device_ps[i]);
                return 0;
                case 2:
                return lpt_ctrls[i];
        }
        return 0xff;
}

void lpt1_write(uint16_t port, uint8_t val, void *priv)
{
	lpt_write(0, port, val, priv);
}

uint8_t lpt1_read(uint16_t port, void *priv)
{
	return lpt_read(0, port, priv);
}

void lpt2_write(uint16_t port, uint8_t val, void *priv)
{
	lpt_write(1, port, val, priv);
}

uint8_t lpt2_read(uint16_t port, void *priv)
{
	return lpt_read(1, port, priv);
}

void lpt3_write(uint16_t port, uint8_t val, void *priv)
{
	lpt_write(2, port, val, priv);
}

uint8_t lpt3_read(uint16_t port, void *priv)
{
	return lpt_read(2, port, priv);
}

uint16_t lpt_addr[3] = { 0x378, 0x278, 0x3bc };

void lpt_init(void)
{
	if (lpt_enabled)
	{
	        io_sethandler(0x0378, 0x0003, lpt1_read, NULL, NULL, lpt1_write, NULL, NULL,  NULL);
        	io_sethandler(0x0278, 0x0003, lpt2_read, NULL, NULL, lpt2_write, NULL, NULL,  NULL);
		lpt_addr[0] = 0x378;
		lpt_addr[1] = 0x278;
	}
}

void lpt1_init(uint16_t port)
{
	if (lpt_enabled)
	{
	        io_sethandler(port, 0x0003, lpt1_read, NULL, NULL, lpt1_write, NULL, NULL,  NULL);
		lpt_addr[0] = port;
	}
}
void lpt1_remove(void)
{
	if (lpt_enabled)
	{
	        io_removehandler(lpt_addr[0], 0x0003, lpt1_read, NULL, NULL, lpt1_write, NULL, NULL,  NULL);
	}
}
void lpt2_init(uint16_t port)
{
	if (lpt_enabled)
	{
	        io_sethandler(port, 0x0003, lpt2_read, NULL, NULL, lpt2_write, NULL, NULL,  NULL);
		lpt_addr[1] = port;
	}
}
void lpt2_remove(void)
{
	if (lpt_enabled)
	{
	        io_removehandler(lpt_addr[1], 0x0003, lpt2_read, NULL, NULL, lpt2_write, NULL, NULL,  NULL);
	}
}

void lpt2_remove_ams(void)
{
	if (lpt_enabled)
	{
	        io_removehandler(0x0379, 0x0002, lpt2_read, NULL, NULL, lpt2_write, NULL, NULL,  NULL);
	}
}

void lpt3_init(uint16_t port)
{
	if (lpt_enabled)
	{
	        io_sethandler(port, 0x0003, lpt3_read, NULL, NULL, lpt3_write, NULL, NULL,  NULL);
		lpt_addr[2] = port;
	}
}
void lpt3_remove(void)
{
	if (lpt_enabled)
	{
	        io_removehandler(lpt_addr[2], 0x0003, lpt3_read, NULL, NULL, lpt3_write, NULL, NULL,  NULL);
	}
}