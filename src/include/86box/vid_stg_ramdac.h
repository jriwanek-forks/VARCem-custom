/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the STG RAMDAC driver.
 *
 * Version:	@(#)vid_stg_ramdac.h	1.0.2	2018/10/05
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
#ifndef VIDEO_STG_RAMDAC_H
# define VIDEO_STG_RAMDAC_H


typedef struct {
    int magic_count, index;
    uint8_t regs[256];
    uint8_t command;
} stg_ramdac_t;


extern const device_t stg_ramdac_device;


extern void	stg_ramdac_out(uint16_t addr, uint8_t val, stg_ramdac_t *dev, svga_t *svga);
extern uint8_t	stg_ramdac_in(uint16_t addr, stg_ramdac_t *dev, svga_t *svga);
extern float	stg_getclock(int clock, void *priv);


#endif	/*VIDEO_STG_RAMDAC_H*/
