/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the "LPT" parallel port handlerss.
 *
 * Version:	@(#)parallel.h	1.0.8	2019/05/13
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
#ifndef EMU_PARALLEL_H
# define EMU_PARALLEL_H


#define PARALLEL1_ADDR	0x0378
#define PARALLEL2_ADDR	0x0278
#define PARALLEL3_ADDR	0x03BC			/* part of the MDA */


#ifdef EMU_DEVICE_H
extern const device_t parallel_1_device;
extern const device_t parallel_2_device;
extern const device_t parallel_3_device;
#endif


extern void	parallel_log(int level, const char *fmt, ...);
extern void	parallel_reset(void);
extern void	parallel_setup(int id, uint16_t port);

extern void	parallel_set_func(priv_t arg,
				  uint8_t (*rfunc)(uint16_t, priv_t), priv_t);


#endif	/*EMU_PARALLEL_H*/
