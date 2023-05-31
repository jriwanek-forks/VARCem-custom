/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		EGA renderers.
 * NOTE:	FIXME: make sure this works (line 99 shadow parameter)
 *
 * Version:	@(#)vid_ega_render.c	1.0.7	2019/05/17
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
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include <86box/86box.h>
#include <86box/timer.h>
#include <86box/mem.h>
#include <86box/rom.h>
#include <86box/device.h>
#include <86box/video.h>
#include <86box/vid_ega.h>
#include <86box/vid_ega_render.h>


int
ega_display_line(ega_t *ega)
{
    int y_add = (enable_overscan) ? (overscan_y >> 1) : 0;
    unsigned int dl = ega->displine;

    if (ega->crtc[9] & 0x1f)
	dl -= (ega->crtc[8] & 0x1f);
    dl += y_add;
    dl &= 0x7ff;

    return dl;
}


void
ega_render_blank(ega_t *ega)
{
    int x_add = (enable_overscan) ? 8 : 0;
    int dl = ega_display_line(ega);
    int x, xx;

    for (x = 0; x < ega->hdisp; x++) switch (ega->seqregs[1] & 9) {
	case 0:
		for (xx = 0; xx < 9; xx++)
			screen->line[dl][(x * 9) + xx + 32 + x_add].val = 0;
		break;

	case 1:
		for (xx = 0; xx < 8; xx++)
			screen->line[dl][(x * 8) + xx + 32 + x_add].val = 0;
		break;

	case 8:
		for (xx = 0; xx < 18; xx++)
			screen->line[dl][(x * 18) + xx + 32 + x_add].val = 0;
		break;

	case 9:
		for (xx = 0; xx < 16; xx++)
			screen->line[dl][(x * 16) + xx + 32 + x_add].val = 0;
		break;
    }
}


void
ega_render_text_standard(ega_t *ega, int draw)
{
    int x_add = (enable_overscan) ? 8 : 0;
    int dl = ega_display_line(ega);
    int x, xx;
	
    for (x = 0; x < ega->hdisp; x++) {
	int do_draw = ((ega->ma == ega->ca) && ega->con && ega->cursoron);
	uint8_t chr  = ega->vram[(ega->ma << 1) & ega->vrammask];
	uint8_t attr = ega->vram[((ega->ma << 1) + 1) & ega->vrammask];
	uint8_t dat;
	uint32_t fg, bg;
	uint32_t charaddr;
	
	if (attr & 8)
		charaddr = ega->charsetb + (chr * 128);
	else
		charaddr = ega->charseta + (chr * 128);

	if (do_draw) { 
		bg = ega->pallook[ega->egapal[attr & 15]]; 
		fg = ega->pallook[ega->egapal[attr >> 4]]; 
	} else {
		fg = ega->pallook[ega->egapal[attr & 15]];
		bg = ega->pallook[ega->egapal[attr >> 4]];
		if (attr & 0x80 && ega->attrregs[0x10] & 8) {
			bg = ega->pallook[ega->egapal[(attr >> 4) & 7]];
			if (ega->blink & 16) 
				fg = bg;
		}
	}

	dat = ega->vram[charaddr + (ega->sc << 2)];
	if (ega->seqregs[1] & 8) {
		if (ega->seqregs[1] & 1) { 
			for (xx = 0; xx < 8; xx++) 
				screen->line[dl][((x << 4) + 32 + (xx << 1) + x_add) & 2047].val = screen->line[dl][((x << 4) + 33 + (xx << 1) + x_add) & 2047].val = (dat & (0x80 >> xx)) ? fg : bg; 
		} else {
			for (xx = 0; xx < 8; xx++) 
				screen->line[dl][((x * 18) + 32 + (xx << 1) + x_add) & 2047].val = screen->line[dl][((x * 18) + 33 + (xx << 1) + x_add) & 2047].val = (dat & (0x80 >> xx)) ? fg : bg;
			if ((chr & ~0x1f) != 0xc0 || !(ega->attrregs[0x10] & 4)) 
				screen->line[dl][((x * 18) + 32 + 16 + x_add) & 2047].val = screen->line[dl][((x * 18) + 32 + 17 + x_add) & 2047].val = bg;
			else
				screen->line[dl][((x * 18) + 32 + 16 + x_add) & 2047].val = screen->line[dl][((x * 18) + 32 + 17 + x_add) & 2047].val = (dat & 1) ? fg : bg;
		}
	} else {
		if (ega->seqregs[1] & 1) { 
			for (xx = 0; xx < 8; xx++) 
				screen->line[dl][((x << 3) + 32 + xx + x_add) & 2047].val = (dat & (0x80 >> xx)) ? fg : bg; 
		} else {
			for (xx = 0; xx < 8; xx++) 
				screen->line[dl][((x * 9) + 32 + xx + x_add) & 2047].val = (dat & (0x80 >> xx)) ? fg : bg;
			if ((chr & ~0x1f) != 0xc0 || !(ega->attrregs[0x10] & 4)) 
				screen->line[dl][((x * 9) + 32 + 8 + x_add) & 2047].val = bg;
			else		  
				screen->line[dl][((x * 9) + 32 + 8 + x_add) & 2047].val = (dat & 1) ? fg : bg;
		}
	}

	ega->ma += 4; 
	ega->ma &= ega->vrammask;
    }
}


#ifdef JEGA
static __inline int
is_kanji1(uint8_t chr)
{
    return (chr >= 0x81 && chr <= 0x9f) || (chr >= 0xe0 && chr <= 0xfc);
}


static __inline int
is_kanji2(uint8_t chr)
{
    return (chr >= 0x40 && chr <= 0x7e) || (chr >= 0x80 && chr <= 0xfc);
}


void
ega_jega_render_blit_text(ega_t *ega, int x, int dl, int start, int width, uint16_t dat, int cw, uint32_t fg, uint32_t bg)
{
    int x_add = (enable_overscan) ? 8 : 0;
    int xx = 0;
    int xxx = 0;

    if (ega->seqregs[1] & 8) {
	for (xx = start; xx < (start + width); xx++) 
		for (xxx = 0; xxx < cw; xxx++)
			screen->line[dl][(((x * width) + 32 + (xxx << 1) + ((xx << 1) * cw)) & 2047) + x_add].val = screen->line[dl][(((x * width) + 33 + (xxx << 1) + ((xx << 1) * cw)) & 2047) + x_add].val = (dat & (0x80 >> xx)) ? fg : bg;
    } else {
	for (xx = start; xx < (start + width); xx++) 
		screen->line[dl][(((x * width) + 32 + xxx + (xx * cw)) & 2047) + x_add].val = (dat & (0x80 >> xx)) ? fg : bg;
    }
}


void
ega_render_text_jega(ega_t *ega, int draw)
{     
    int dl = ega_display_line(ega);
    uint8_t chr, attr;
    uint16_t dat = 0, dat2;
    uint32_t fg = 0, bg = 0;
    int x;

    /* Temporary for DBCS. */
    unsigned int chr_left = 0;
    unsigned int bsattr = 0;
    int chr_wide = 0;
    uint32_t bg_ex = 0;
    uint32_t fg_ex = 0;

    int blocks = ega->hdisp;
    int fline;

    int pad_y;
    unsigned int exattr;

    if (fullchange) {
	for (x = 0; x < ega->hdisp; x++) {
		draw = ((ega->ma == ega->ca) && ega->con && ega->cursoron);
		chr  = ega->vram[(ega->ma << 1) & ega->vrammask];
		attr = ega->vram[((ega->ma << 1) + 1) & ega->vrammask];

		if (chr_wide == 0) {
			if (ega->RMOD2 & 0x80) {
				fg_ex = ega->pallook[ega->egapal[attr & 15]];

				if (attr & 0x80 && ega->attrregs[0x10] & 8) {
					bg_ex = ega->pallook[ega->egapal[(attr >> 4) & 7]];
				} else {
					bg_ex = ega->pallook[ega->egapal[attr >> 4]];
				}
			} else {
				if (attr & 0x40) {
					/* Reversed in JEGA mode */
					bg_ex = ega->pallook[ega->egapal[attr & 15]];
					fg_ex = ega->pallook[0];
				} else {
					/* Reversed in JEGA mode */
					fg_ex = ega->pallook[ega->egapal[attr & 15]];
					bg_ex = ega->pallook[0];
				}
			}

			if (draw) { 
				bg = fg_ex;
				fg = bg_ex;
			} else {
				fg = fg_ex;
				bg = bg_ex;
			}
	
			if (attr & 0x80 && ega->attrregs[0x10] & 8) {
				if (ega->blink & 16) 
					fg = bg;
			}

			/* Stay drawing if the char code is DBCS and not at last column. */
			if (is_kanji1((uint8_t)dat) && (blocks > 1)) {
				/* Set the present char/attr code to the next loop. */
				chr_left = chr;
				chr_wide = 1;
			} else {
				/* The char code is ANK (8 dots width). */
				dat = jfont_sbcs_19[chr*19+(ega->sc)];	/* w8xh19 font */
				ega_jega_render_blit_text(ega, x, dl, 0, 8, dat, 1, fg, bg);
				if (bsattr & 0x20) {
					/* Vertical line. */
					dat = 0x18;
					ega_jega_render_blit_text(ega, x, fline, 0, 8, dat, 1, fg, bg);
				}
				if (ega->sc == 18 && bsattr & 0x10) {
					/* Underline. */
					dat = 0xff;
					ega_jega_render_blit_text(ega, x, fline, 0, 8, dat, 1, fg, bg);
				}
				chr_wide = 0;
				blocks--;
			}
		} else {
			/* The char code may be in DBCS. */
			pad_y = ega->RPSSC;
			exattr = 0;

			/* Note: The second column should be applied its basic attribute. */
			if (ega->RMOD2 & 0x40) {
				/* If JEGA Extended Attribute is enabled. */
				exattr = attr;
				if ((exattr & 0x30) == 0x30)  pad_y = ega->RPSSL;	/* Set top padding of lower 2x character. */
				else if (exattr & 0x30)  pad_y = ega->RPSSU;	/* Set top padding of upper 2x character. */
			}

			if (ega->sc >= pad_y && ega->sc < 16 + pad_y) {
				/* Check the char code is in Wide charset of Shift-JIS. */
				if (is_kanji2((uint8_t)chr)) {
					fline = ega->sc - pad_y;
					chr_left <<= 8;
					/* Fix vertical position. */
					chr |= chr_left;
					/* Horizontal wide font (Extended Attribute). */
					if (exattr & 0x20) {
						if (exattr & 0x10)  fline = (fline >> 1) + 8;
						else  fline = fline >> 1;
					}
					/* Vertical wide font (Extended Attribute). */
					if (exattr & 0x40) {
						dat = jfont_dbcs_16[chr * 32 + fline * 2];
						if (!(exattr & 0x08))
							dat = jfont_dbcs_16[chr * 32 + fline * 2 + 1];
						/* Draw 8 dots. */
						ega_jega_render_blit_text(ega, x, dl, 0, 8, dat, 2, fg, bg);
					} else {
						/* Get the font pattern. */
						dat = jfont_dbcs_16[chr * 32 + fline * 2];
						dat <<= 8;
						dat |= jfont_dbcs_16[chr * 32 + fline * 2 + 1];
						/* Bold (Extended Attribute). */
						if (exattr &= 0x80) {
							dat2 = dat;
							dat2 >>= 1;
							dat |= dat2;
							/* Original JEGA colours the last row with the next column's attribute. */
						}
						/* Draw 16 dots */
						ega_jega_render_blit_text(ega, x, dl, 0, 16, dat, 1, fg, bg);
					}
				} else {
					/* Ignore wide char mode, put blank. */
					dat = 0;
					ega_jega_render_blit_text(ega, x, dl, 0, 16, dat, 1, fg, bg);
				}
			} else if (ega->sc == (17 + pad_y) && (bsattr & 0x10)) {
				/* Underline. */
				dat = 0xffff;
				ega_jega_render_blit_text(ega, x, dl, 0, 16, dat, 1, fg, bg);
			} else {
				/* Draw blank */
				dat = 0;
				ega_jega_render_blit_text(ega, x, dl, 0, 16, dat, 1, fg, bg);
			}

			if (bsattr & 0x20) {
				/* Vertical line draw at last. */
				dat = 0x0180;
				ega_jega_render_blit_text(ega, x, dl, 0, 16, dat, 1, fg, bg);
			}

			chr_wide = 0;
			blocks -= 2;	/* Move by 2 columns. */
		}

		ega->ma += 4; 
		ega->ma &= ega->vrammask;
	}
    }
}
#endif


void
ega_render_2bpp(ega_t *ega)
{
    int dl = ega_display_line(ega);
    int offset = ((8 - ega->scrollcache) << 1) + 16;
    int x;
	
    for (x = 0; x <= ega->hdisp; x++) {
	uint8_t edat[2];
	uint32_t addr = ega->ma;
			
	if (!(ega->crtc[0x17] & 0x40)) {
		addr = (addr << 1) & ega->vrammask;
		addr &= ~7;
		if ((ega->crtc[0x17] & 0x20) && (ega->ma & 0x20000))
			addr |= 4;
		if (!(ega->crtc[0x17] & 0x20) && (ega->ma & 0x8000))
			addr |= 4;
	}
	if (!(ega->crtc[0x17] & 0x01))
		addr = (addr & ~0x8000) | ((ega->sc & 1) ? 0x8000 : 0);
	if (!(ega->crtc[0x17] & 0x02))
		addr = (addr & ~0x10000) | ((ega->sc & 2) ? 0x10000 : 0);

	edat[0] = ega->vram[addr];
	edat[1] = ega->vram[addr | 0x1];
	if (ega->seqregs[1] & 4)
		ega->ma += 2;
	else
		ega->ma += 4;

	ega->ma &= ega->vrammask;

	screen->line[dl][(x << 4) + 14 + offset].val = screen->line[ega->displine][(x << 4) + 15 + offset].val = ega->pallook[ega->egapal[edat[1] & 3]];
	screen->line[dl][(x << 4) + 12 + offset].val = screen->line[ega->displine][(x << 4) + 13 + offset].val = ega->pallook[ega->egapal[(edat[1] >> 2) & 3]];
	screen->line[dl][(x << 4) + 10 + offset].val = screen->line[ega->displine][(x << 4) + 11 + offset].val = ega->pallook[ega->egapal[(edat[1] >> 4) & 3]];
	screen->line[dl][(x << 4) +  8 + offset].val = screen->line[ega->displine][(x << 4) +  9 + offset].val = ega->pallook[ega->egapal[(edat[1] >> 6) & 3]];
	screen->line[dl][(x << 4) +  6 + offset].val = screen->line[ega->displine][(x << 4) +  7 + offset].val = ega->pallook[ega->egapal[(edat[0] >> 0) & 3]];
	screen->line[dl][(x << 4) +  4 + offset].val = screen->line[ega->displine][(x << 4) +  5 + offset].val = ega->pallook[ega->egapal[(edat[0] >> 2) & 3]];
	screen->line[dl][(x << 4) +  2 + offset].val = screen->line[ega->displine][(x << 4) +  3 + offset].val = ega->pallook[ega->egapal[(edat[0] >> 4) & 3]];
	screen->line[dl][(x << 4) +      offset].val = screen->line[ega->displine][(x << 4) +  1 + offset].val = ega->pallook[ega->egapal[(edat[0] >> 6) & 3]];
    }
}


void
ega_render_4bpp_lowres(ega_t *ega)
{
    int x_add = (enable_overscan) ? 8 : 0;
    int dl = ega_display_line(ega);
    int offset = ((8 - ega->scrollcache) << 1) + 16;
    int x;

    for (x = 0; x <= ega->hdisp; x++) {
	uint8_t edat[4];
	uint8_t dat;
	uint32_t addr = ega->ma;
	int oddeven = 0;

	if (!(ega->crtc[0x17] & 0x40)) {
		addr = (addr << 1) & ega->vrammask;
		if (ega->seqregs[1] & 4)
			oddeven = (addr & 4) ? 1 : 0;
		addr &= ~7;
		if ((ega->crtc[0x17] & 0x20) && (ega->ma & 0x20000))
			addr |= 4;
		if (!(ega->crtc[0x17] & 0x20) && (ega->ma & 0x8000))
			addr |= 4;
	}
	if (!(ega->crtc[0x17] & 0x01))
		addr = (addr & ~0x8000) | ((ega->sc & 1) ? 0x8000 : 0);
	if (!(ega->crtc[0x17] & 0x02))
		addr = (addr & ~0x10000) | ((ega->sc & 2) ? 0x10000 : 0);

	if (ega->seqregs[1] & 4) {
		edat[0] = ega->vram[addr | oddeven];
		edat[2] = ega->vram[addr | oddeven | 0x2];
		edat[1] = edat[3] = 0;
		ega->ma += 2;
	} else {
		edat[0] = ega->vram[addr];
		edat[1] = ega->vram[addr | 0x1];
		edat[2] = ega->vram[addr | 0x2];
		edat[3] = ega->vram[addr | 0x3];
		ega->ma += 4;
	}
	ega->ma &= ega->vrammask;

	dat = edatlookup[edat[0] & 3][edat[1] & 3] | (edatlookup[edat[2] & 3][edat[3] & 3] << 2);
	screen->line[dl][(x << 4) + 14 + offset + x_add].val = screen->line[dl][(x << 4) + 15 + offset + x_add].val = ega->pallook[ega->egapal[(dat & 0xf) & ega->attrregs[0x12]]];
	screen->line[dl][(x << 4) + 12 + offset + x_add].val = screen->line[dl][(x << 4) + 13 + offset + x_add].val = ega->pallook[ega->egapal[(dat >> 4)  & ega->attrregs[0x12]]];

	dat = edatlookup[(edat[0] >> 2) & 3][(edat[1] >> 2) & 3] | (edatlookup[(edat[2] >> 2) & 3][(edat[3] >> 2) & 3] << 2);
	screen->line[dl][(x << 4) + 10 + offset + x_add].val = screen->line[dl][(x << 4) + 11 + offset + x_add].val = ega->pallook[ega->egapal[(dat & 0xf) & ega->attrregs[0x12]]];
	screen->line[dl][(x << 4) +  8 + offset + x_add].val = screen->line[dl][(x << 4) +  9 + offset + x_add].val = ega->pallook[ega->egapal[(dat >> 4)  & ega->attrregs[0x12]]];

	dat = edatlookup[(edat[0] >> 4) & 3][(edat[1] >> 4) & 3] | (edatlookup[(edat[2] >> 4) & 3][(edat[3] >> 4) & 3] << 2);
	screen->line[dl][(x << 4) +  6 + offset + x_add].val = screen->line[dl][(x << 4) +  7 + offset + x_add].val = ega->pallook[ega->egapal[(dat & 0xf) & ega->attrregs[0x12]]];
	screen->line[dl][(x << 4) +  4 + offset + x_add].val = screen->line[dl][(x << 4) +  5 + offset + x_add].val = ega->pallook[ega->egapal[(dat >> 4)  & ega->attrregs[0x12]]];

	dat = edatlookup[edat[0] >> 6][edat[1] >> 6] | (edatlookup[edat[2] >> 6][edat[3] >> 6] << 2);
	screen->line[dl][(x << 4) +  2 + offset + x_add].val = screen->line[dl][(x << 4) +  3 + offset + x_add].val = ega->pallook[ega->egapal[(dat & 0xf) & ega->attrregs[0x12]]];
	screen->line[dl][(x << 4) +      offset + x_add].val = screen->line[dl][(x << 4) +  1 + offset + x_add].val = ega->pallook[ega->egapal[(dat >> 4)  & ega->attrregs[0x12]]];
    }
}


void
ega_render_4bpp_highres(ega_t *ega)
{
    int x_add = (enable_overscan) ? 8 : 0;
    int dl = ega_display_line(ega);
    int offset = (8 - ega->scrollcache) + 24;
    int x;

    for (x = 0; x <= ega->hdisp; x++) {
	uint8_t edat[4];
	uint8_t dat;
	uint32_t addr = ega->ma;
	int oddeven = 0;

	if (!(ega->crtc[0x17] & 0x40)) {
		addr = (addr << 1) & ega->vrammask;
		if (ega->seqregs[1] & 4)
			oddeven = (addr & 4) ? 1 : 0;
		addr &= ~7;
		if ((ega->crtc[0x17] & 0x20) && (ega->ma & 0x20000))
			addr |= 4;
		if (!(ega->crtc[0x17] & 0x20) && (ega->ma & 0x8000))
			addr |= 4;
	}
	if (!(ega->crtc[0x17] & 0x01))
	       addr = (addr & ~0x8000) | ((ega->sc & 1) ? 0x8000 : 0);
	if (!(ega->crtc[0x17] & 0x02))
		addr = (addr & ~0x10000) | ((ega->sc & 2) ? 0x10000 : 0);

	if (ega->seqregs[1] & 4) {
		edat[0] = ega->vram[addr | oddeven];
		edat[2] = ega->vram[addr | oddeven | 0x2];
		edat[1] = edat[3] = 0;
		ega->ma += 2;
	} else {
		edat[0] = ega->vram[addr];
		edat[1] = ega->vram[addr | 0x1];
		edat[2] = ega->vram[addr | 0x2];
		edat[3] = ega->vram[addr | 0x3];
		ega->ma += 4;
	}
	ega->ma &= ega->vrammask;

	dat = edatlookup[edat[0] & 3][edat[1] & 3] | (edatlookup[edat[2] & 3][edat[3] & 3] << 2);
	screen->line[dl][(x << 3) + 7 + offset + x_add].val = ega->pallook[ega->egapal[(dat & 0xf) & ega->attrregs[0x12]]];
	screen->line[dl][(x << 3) + 6 + offset + x_add].val = ega->pallook[ega->egapal[(dat >> 4)  & ega->attrregs[0x12]]];

	dat = edatlookup[(edat[0] >> 2) & 3][(edat[1] >> 2) & 3] | (edatlookup[(edat[2] >> 2) & 3][(edat[3] >> 2) & 3] << 2);
	screen->line[dl][(x << 3) + 5 + offset + x_add].val = ega->pallook[ega->egapal[(dat & 0xf) & ega->attrregs[0x12]]];
	screen->line[dl][(x << 3) + 4 + offset + x_add].val = ega->pallook[ega->egapal[(dat >> 4)  & ega->attrregs[0x12]]];

	dat = edatlookup[(edat[0] >> 4) & 3][(edat[1] >> 4) & 3] | (edatlookup[(edat[2] >> 4) & 3][(edat[3] >> 4) & 3] << 2);
	screen->line[dl][(x << 3) + 3 + offset + x_add].val = ega->pallook[ega->egapal[(dat & 0xf) & ega->attrregs[0x12]]];
	screen->line[dl][(x << 3) + 2 + offset + x_add].val = ega->pallook[ega->egapal[(dat >> 4)  & ega->attrregs[0x12]]];

	dat = edatlookup[edat[0] >> 6][edat[1] >> 6] | (edatlookup[edat[2] >> 6][edat[3] >> 6] << 2);
	screen->line[dl][(x << 3) + 1 + offset + x_add].val = ega->pallook[ega->egapal[(dat & 0xf) & ega->attrregs[0x12]]];
	screen->line[dl][(x << 3) +     offset + x_add].val = ega->pallook[ega->egapal[(dat >> 4)  & ega->attrregs[0x12]]];
    }
}
