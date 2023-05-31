/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the ATI drivers.
 *
 * Version:	@(#)vid_ati.h	1.0.2	2019/03/03
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
#ifndef VIDEO_ATI_H
# define VIDEO_ATI_H


typedef struct {
    int		type;

    int		wp;
    int		oldclk, oldena;
    int		opcode, state, count, out;
    uint32_t	dat;

    wchar_t	fn[256];

    uint16_t	data[256];
} ati_eeprom_t;


extern void	ati_eeprom_load(ati_eeprom_t *, wchar_t *fn, int type);
extern void	ati_eeprom_write(ati_eeprom_t *, int ena, int clk, int dat);
extern int	ati_eeprom_read(ati_eeprom_t *);

#ifdef VIDEO_SVGA_H
extern int	ati28800k_load_font(svga_t *, const wchar_t *);
#endif


#endif	/*VIDEO_ATI_H*/
