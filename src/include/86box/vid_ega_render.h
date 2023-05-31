/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the EGA renderer.
 *
 * Version:	@(#)vid_ega_render.h	1.0.1	2018/02/14
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
#ifndef VIDEO_EGA_RENDERER_H
# define VIDEO_EGA_RENDERER_H


extern int firstline_draw, lastline_draw;
extern int displine;
extern int sc;

extern uint32_t ma, ca;
extern int con, cursoron, cgablink;

extern int scrollcache;

extern uint8_t edatlookup[4][4];

void ega_render_blank(ega_t *ega);
void ega_render_text_standard(ega_t *ega, int drawcursor);
#ifdef JEGA
void ega_render_text_jega(ega_t *ega, int drawcursor);
#endif

void ega_render_2bpp(ega_t *ega);

void ega_render_4bpp_lowres(ega_t *ega);
void ega_render_4bpp_highres(ega_t *ega);


#endif	/*VIDEO_EGA_RENDERER_H*/
