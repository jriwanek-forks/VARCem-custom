/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of the EGA, Chips & Technologies SuperEGA, and
 *		AX JEGA graphics cards.
 *
 * Version:	@(#)vid_ega.c	1.0.21	2019/05/17
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
 *		Copyright 2016-2019 Miran Grca.
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
#include <86box/timer.h>
#include "../cpu/cpu.h"
#include <86box/io.h>
#include <86box/mem.h>
#include <86box/rom.h>
#include <86box/device.h>
#include <86box/plat.h>
#include <86box/clk.h>
#include <86box/video.h>
#include <86box/vid_ega.h>
#include <86box/vid_ega_render.h>


#define BIOS_IBM_PATH		L"video/ibm/ega/ibm_6277356_ega_card_u44_27128.bin"
#define BIOS_CPQ_PATH		L"video/compaq/ega/108281-001.bin"
#define BIOS_SEGA_PATH		L"video/phoenix/ega/lega.vbi"
#define BIOS_PEGA1A_PATH	L"video/paradise/pega1a/wd-pega.rom"
#define BIOS_PEGA2A_PATH	L"video/paradise/pega2a/pega2a.bin"


#ifdef JEGA
typedef struct {
    char	id[ID_LEN];
    char	name[NAME_LEN];
    uint8_t	width;
    uint8_t	height;
    uint8_t	type;
} fontx_h;

typedef struct {
    uint16_t	start;
    uint16_t	end;
} fontxTbl;
#endif


static uint8_t	ega_rotate[8][256];
static uint32_t	pallook16[256],
		pallook64[256];
static int	old_overscan_color = 0;
static int	egaswitchread,
		egaswitches = 9;	/*7=CGA mode (200 lines),
					 *9=EGA mode (350 lines),
					 *8=EGA mode (200 lines)*/
#ifdef JEGA
uint8_t		jfont_sbcs_19[SBCS19_LEN];	/* 256 * 19( * 8) */
uint8_t		jfont_dbcs_16[DBCS16_LEN];	/* 65536 * 16 * 2 (* 8) */


static __inline int
jega_enabled(ega_t *dev)
{
    if (dev->type != EGA_JEGA)
	return 0;

    return !(dev->RMOD1 & 0x40);
}


void
ega_jega_write_font(ega_t *dev)
{
    unsigned int chr = dev->RDFFB;
    unsigned int chr_2 = dev->RDFSB;

    dev->RSTAT &= ~0x02;

    /* Check if the character code is in the Wide character set of Shift-JIS */
    if (((chr >= 0x40) && (chr <= 0x7e)) || ((chr >= 0x80) && (chr <= 0xfc))) {
	if (dev->font_index >= 32)
		dev->font_index = 0;
	chr <<= 8;

	/* Fix vertical character position */
	chr |= chr_2;
	if (dev->font_index < 16) {
		jfont_dbcs_16[(chr * 32) + (dev->font_index * 2)] = dev->RDFAP;				/* 16x16 font */
	} else {
		jfont_dbcs_16[(chr * 32) + ((dev->font_index - 16) * 2) + 1] = dev->RDFAP;		/* 16x16 font */
	}
    } else {
	if (dev->font_index >= 19)
		dev->font_index = 0;
	jfont_sbcs_19[(chr * 19) + dev->font_index] = dev->RDFAP;					/* 8x19 font */
    }

    dev->font_index++;
    dev->RSTAT |= 0x02;
}


void
ega_jega_read_font(ega_t *dev)
{
    unsigned int chr = dev->RDFFB;
    unsigned int chr_2 = dev->RDFSB;

    dev->RSTAT &= ~0x02;

    /* Check if the character code is in the Wide character set of Shift-JIS */
    if (((chr >= 0x40) && (chr <= 0x7e)) || ((chr >= 0x80) && (chr <= 0xfc))) {
	if (dev->font_index >= 32)
		dev->font_index = 0;
	chr <<= 8;

	/* Fix vertical character position */
	chr |= chr_2;
	if (dev->font_index < 16) {
		dev->RDFAP = jfont_dbcs_16[(chr * 32) + (dev->font_index * 2)];				/* 16x16 font */
	} else {
		dev->RDFAP = jfont_dbcs_16[(chr * 32) + ((dev->font_index - 16) * 2) + 1];		/* 16x16 font */
	}
    } else {
	if (dev->font_index >= 19)
		dev->font_index = 0;
	dev->RDFAP = jfont_sbcs_19[(chr * 19) + dev->font_index];					/* 8x19 font */
    }

    dev->font_index++;
    dev->RSTAT |= 0x02;
}


static uint16_t
chrtosht(FILE *fp)
{
    uint16_t i, j;

    i = (uint8_t) fgetc(fp);
    j = (uint8_t) fgetc(fp) << 8;

    return(i | j);
}


static int
getfontx2header(FILE *fp, fontx_h *header)
{
    fread(header->id, ID_LEN, 1, fp);
    if (strncmp(header->id, "FONTX2", ID_LEN) != 0)
	return(1);

    fread(header->name, NAME_LEN, 1, fp);

    header->width = (uint8_t)fgetc(fp);
    header->height = (uint8_t)fgetc(fp);
    header->type = (uint8_t)fgetc(fp);

    return(0);
}


static void
readfontxtbl(fontxTbl *table, unsigned int size, FILE *fp)
{
    while (size > 0) {
	table->start = chrtosht(fp);
	table->end = chrtosht(fp);

	table++;
	size++;
    }
}


static void
LoadFontxFile(const wchar_t *fn)
{
    fontx_h head;
    fontxTbl *table;
    unsigned int code;
    uint8_t size;
    unsigned int i;
    FILE *fp;

    if (fn == NULL) return;

    fp = rom_fopen(fn, L"rb");
    if (fp == NULL) {
	ERRLOG("EGA: Can't open FONTX2 file '%ls' !\n", fn);
	return;
    }

    if (getfontx2header(fp, &head) != 0) {
	DEBUG("EGA: FONTX2 header is incorrect\n");
	fclose(fp);
	return;
    }

    /* switch whether the font is DBCS or not */
    if (head.type == DBCS) {
	if (head.width != 16 || head.height != 16) {
		ERRLOG("EGA: FONTX2 DBCS font size is not correct\n");
		fclose(fp);
		return;
	}
	size = getc(fp);
	table = (fontxTbl *)mem_alloc(size * sizeof(fontxTbl));
	memset(table, 0x00, size * sizeof(fontxTbl));
	readfontxtbl(table, size, fp);
	for (i = 0; i < size; i++) {
		for (code = table[i].start; code <= table[i].end; code++) {
			fread(&jfont_dbcs_16[(code * 32)], sizeof(uint8_t), 32, fp);
		}
	}
    } else {
	if (head.width != 8 || head.height != 19) {
		ERRLOG("EGA: FONTX2 SBCS font size is not correct\n");
		fclose(fp);
		return;
	}
	fread(jfont_sbcs_19, sizeof(uint8_t), SBCS19_LEN, fp);
	//FIXME: no actual data read? */
    }

    fclose(fp);
}
#endif


void
ega_recalctimings(ega_t *dev)
{
    double _dispontime, _dispofftime, disptime;
    double crtcconst;

    dev->vtotal = dev->crtc[6];
    dev->dispend = dev->crtc[0x12];
    dev->vsyncstart = dev->crtc[0x10];
    dev->split = dev->crtc[0x18];

    if (dev->crtc[7] & 1)  dev->vtotal |= 0x100;
    if (dev->crtc[7] & 32) dev->vtotal |= 0x200;
    dev->vtotal += 2;

    if (dev->crtc[7] & 2)  dev->dispend |= 0x100;
    if (dev->crtc[7] & 64) dev->dispend |= 0x200;
    dev->dispend++;

    if (dev->crtc[7] & 4)   dev->vsyncstart |= 0x100;
    if (dev->crtc[7] & 128) dev->vsyncstart |= 0x200;
    dev->vsyncstart++;

    if (dev->crtc[7] & 0x10) dev->split |= 0x100;
    if (dev->crtc[9] & 0x40) dev->split |= 0x200;
    dev->split++;

    dev->hdisp = dev->crtc[1];
    dev->hdisp++;

    dev->rowoffset = dev->crtc[0x13];
    dev->rowcount = dev->crtc[9] & 0x1f;
    overscan_y = (dev->rowcount + 1) << 1;

    if (dev->vidclock)
	crtcconst = (dev->seqregs[1] & 1) ? MDACONST : (MDACONST * (9.0 / 8.0));
    else
	crtcconst = (dev->seqregs[1] & 1) ? CGACONST : (CGACONST * (9.0 / 8.0));

    if (dev->seqregs[1] & 8) { 
	disptime = (double) ((dev->crtc[0] + 2) << 1);
	_dispontime = (double) ((dev->crtc[1] + 1) << 1);

	overscan_y <<= 1;
    } else {
	disptime = (double) (dev->crtc[0] + 2);
	_dispontime = (double) (dev->crtc[1] + 1);
    }
    if (overscan_y < 16)
	overscan_y = 16;

    _dispofftime = disptime - _dispontime;
    _dispontime  = _dispontime * crtcconst;
    _dispofftime = _dispofftime * crtcconst;

    dev->dispontime  = (tmrval_t)(_dispontime  * (1LL << TIMER_SHIFT));
    dev->dispofftime = (tmrval_t)(_dispofftime * (1LL << TIMER_SHIFT));
}


void
ega_poll(priv_t priv)
{
    ega_t *dev = (ega_t *)priv;
    int y_add = enable_overscan ? (overscan_y >> 1) : 0;
    int x_add = enable_overscan ? 8 : 0;
    int y_add_ex = enable_overscan ? overscan_y : 0;
    int x_add_ex = enable_overscan ? 16 : 0;
    int wx = 640, wy = 350;
    int drawcursor = 0;
    int i, j, x;

   if (! dev->linepos) {
	dev->vidtime += dev->dispofftime;

	dev->stat |= 1;
	dev->linepos = 1;

	if (dev->dispon) {
		if (dev->firstline == 2000) {
			dev->firstline = dev->displine;
			video_blit_wait_buffer();
		}

		if (dev->scrblank)
			ega_render_blank(dev);
		else if (!(dev->gdcreg[6] & 1)) {
			if (fullchange) {
#ifdef JEGA
				if (jega_enabled(dev))
					ega_render_text_jega(dev, drawcursor);
				else
#endif
				ega_render_text_standard(dev, drawcursor);
			}
		} else {
			switch (dev->gdcreg[5] & 0x20) {
				case 0x00:
					if (dev->seqregs[1] & 8)
						ega_render_4bpp_lowres(dev);
					else
						ega_render_4bpp_highres(dev);
					break;

				case 0x20:
					ega_render_2bpp(dev);
					break;
			}
		}

		if (dev->lastline < dev->displine) 
			dev->lastline = dev->displine;
	}

	dev->displine++;
	if (dev->interlace) 
		dev->displine++;
	if ((dev->stat & 8) && ((dev->displine & 15) == (dev->crtc[0x11] & 15)) && dev->vslines)
		dev->stat &= ~8;
	dev->vslines++;

	if (dev->displine > 500) 
		dev->displine = 0;
    } else {
	dev->vidtime += dev->dispontime;
	if (dev->dispon) 
		dev->stat &= ~1;

	dev->linepos = 0;
	if (dev->sc == (dev->crtc[11] & 31)) 
		dev->con = 0; 
	if (dev->dispon) {

		if (dev->sc == (dev->crtc[9] & 31)) {
			dev->sc = 0;
			if (dev->sc == (dev->crtc[11] & 31))
				dev->con = 0;

			dev->maback += (dev->rowoffset << 3);
			if (dev->interlace)
				dev->maback += (dev->rowoffset << 3);
			dev->maback &= dev->vrammask;
			dev->ma = dev->maback;
		} else {
			dev->sc++;
			dev->sc &= 31;
			dev->ma = dev->maback;
		}
	}

	dev->vc++;
	dev->vc &= 1023;
	if (dev->vc == dev->split) {
		dev->ma = dev->maback = 0;
		if (dev->attrregs[0x10] & 0x20)
			dev->scrollcache = 0;
	}

	if (dev->vc == dev->dispend) {
		dev->dispon=0;
		if (dev->crtc[10] & 0x20)
			dev->cursoron = 0;
		else
			dev->cursoron = dev->blink & 16;
		if (!(dev->gdcreg[6] & 1) && !(dev->blink & 15)) 
			fullchange = 2;
		dev->blink++;

		if (fullchange) 
			fullchange--;
	}

	if (dev->vc == dev->vsyncstart) {
		dev->dispon = 0;
		dev->stat |= 8;
		if (dev->seqregs[1] & 8)
			x = dev->hdisp * ((dev->seqregs[1] & 1) ? 8 : 9) * 2;
		else
			x = dev->hdisp * ((dev->seqregs[1] & 1) ? 8 : 9);

		if (dev->interlace && !dev->oddeven) dev->lastline++;
		if (dev->interlace &&  dev->oddeven) dev->firstline--;

		if ((x != xsize || (dev->lastline - dev->firstline + 1) != ysize) || update_overscan || video_force_resize_get()) {
			xsize = x;
			ysize = dev->lastline - dev->firstline + 1;
			if (xsize < 64) xsize = 640;
			if (ysize < 32) ysize = 200;
			y_add = enable_overscan ? 14 : 0;
			x_add = enable_overscan ? 8 : 0;
			y_add_ex = enable_overscan ? 28 : 0;
			x_add_ex = enable_overscan ? 16 : 0;

			if ((xsize > 2032) || ((ysize + y_add_ex) > 2048)) {
				x_add = x_add_ex = 0;
				y_add = y_add_ex = 0;
				suppress_overscan = 1;
			} else {
				suppress_overscan = 0;
			}

			if (dev->vres)
				set_screen_size(xsize + x_add_ex, (ysize << 1) + y_add_ex);
			else
				set_screen_size(xsize + x_add_ex, ysize + y_add_ex);

			if (video_force_resize_get())
				video_force_resize_set(0);
		}

		if (enable_overscan) {
			if ((x >= 160) && ((dev->lastline - dev->firstline) >= 120)) {
				/* Draw (overscan_size - scroll size) lines of overscan on top. */
				for (i  = 0; i < (y_add - (dev->crtc[8] & 0x1f)); i++) {
					for (j = 0; j < (xsize + x_add_ex); j++) {
						screen->line[i & 0x7ff][32 + j].val = dev->pallook[dev->attrregs[0x11]];
					}
				}

				/* Draw (overscan_size + scroll size) lines of overscan on the bottom. */
				for (i  = 0; i < (y_add + (dev->crtc[8] & 0x1f)); i++) {
					for (j = 0; j < (xsize + x_add_ex); j++) {
						screen->line[(ysize + y_add + i - (dev->crtc[8] & 0x1f)) & 0x7ff][32 + j].val = dev->pallook[dev->attrregs[0x11]];
					}
				}

				for (i = (y_add - (dev->crtc[8] & 0x1f)); i < (ysize + y_add - (dev->crtc[8] & 0x1f)); i ++) {
					for (j = 0; j < x_add; j++) {
						screen->line[(i - (dev->crtc[8] & 0x1f)) & 0x7ff][32 + j].val = dev->pallook[dev->attrregs[0x11]];
						screen->line[(i - (dev->crtc[8] & 0x1f)) & 0x7ff][32 + xsize + x_add + j].val = dev->pallook[dev->attrregs[0x11]];
					}
				}
			}
		} else {
			if (dev->crtc[8] & 0x1f) {
				/* Draw (scroll size) lines of overscan on the bottom. */
				for (i  = 0; i < (dev->crtc[8] & 0x1f); i++) {
					for (j = 0; j < xsize; j++) {
						screen->line[(ysize + i - (dev->crtc[8] & 0x1f)) & 0x7ff][32 + j].val = dev->pallook[dev->attrregs[0x11]];
					}
				}
			}
		}
	 
		video_blit_start(0, 32, 0, dev->firstline, dev->lastline + 1 + y_add_ex, xsize + x_add_ex, dev->lastline - dev->firstline + 1 + y_add_ex);
		frames++;
			
		dev->video_res_x = wx;
		dev->video_res_y = wy + 1;
		if (!(dev->gdcreg[6] & 1)) {	/*Text mode*/
			dev->video_res_x /= (dev->seqregs[1] & 1) ? 8 : 9;
			dev->video_res_y /= (dev->crtc[9] & 31) + 1;
			dev->video_bpp = 0;
		} else {
			if (dev->crtc[9] & 0x80)
			   dev->video_res_y /= 2;
			if (!(dev->crtc[0x17] & 1))
			   dev->video_res_y *= 2;
			dev->video_res_y /= (dev->crtc[9] & 31) + 1;				   
			if (dev->seqregs[1] & 8)
			   dev->video_res_x /= 2;
			dev->video_bpp = (dev->gdcreg[5] & 0x20) ? 2 : 4;
		}

		dev->firstline = 2000;
		dev->lastline = 0;

		dev->maback = dev->ma = (dev->crtc[0xc] << 8)| dev->crtc[0xd];
		dev->ca = (dev->crtc[0xe] << 8) | dev->crtc[0xf];
		dev->ma <<= 2;
		dev->maback <<= 2;
		dev->ca <<= 2;
		changeframecount = 2;
		dev->vslines = 0;
	}

	if (dev->vc == dev->vtotal) {
		dev->vc = 0;
		dev->sc = 0;
		dev->dispon = 1;
		dev->displine = (dev->interlace && dev->oddeven) ? 1 : 0;
		dev->scrollcache = dev->attrregs[0x13] & 7;
	}

	if (dev->sc == (dev->crtc[10] & 31)) 
		dev->con = 1;
    }
}


void
ega_out(uint16_t port, uint8_t val, priv_t priv)
{
    ega_t *dev = (ega_t *)priv;
    uint8_t o, old;
    int c;
	
    if (((port & 0xfff0) == 0x03d0 ||
	 (port & 0xfff0) == 0x03b0) && !(dev->miscout & 1)) port ^= 0x60;
	
    switch (port) {
	case 0x03c0:
	case 0x03c1:
		if (! dev->attrff)
		   dev->attraddr = val & 31;
		else {
			dev->attrregs[dev->attraddr & 31] = val;
			if (dev->attraddr < 16) 
				fullchange = changeframecount;
			if (dev->attraddr == 0x10 || dev->attraddr == 0x14 || dev->attraddr < 0x10) {
				for (c = 0; c < 16; c++) {
					if (dev->attrregs[0x10] & 0x80)
						dev->egapal[c] = (dev->attrregs[c] &  0xf) | ((dev->attrregs[0x14] & 0xf) << 4);
					else
						dev->egapal[c] = (dev->attrregs[c] & 0x3f) | ((dev->attrregs[0x14] & 0xc) << 4);
				}
			}
		}
		dev->attrff ^= 1;
		break;

	case 0x03c2:
                egaswitchread = (val & 0xc) >> 2;
		dev->vres = !(val & 0x80);
		dev->pallook = dev->vres ? pallook16 : pallook64;
		dev->vidclock = val & 4;
		dev->miscout = val;
		break;

	case 0x03c4: 
		dev->seqaddr = val; 
		break;

	case 0x03c5:
		o = dev->seqregs[dev->seqaddr & 0x0f];
		dev->seqregs[dev->seqaddr & 0x0f] = val;
		if (o != val && (dev->seqaddr & 0x0f) == 1) 
			ega_recalctimings(dev);
		switch (dev->seqaddr & 0x0f) {
			case 1: 
				if (dev->scrblank && !(val & 0x20)) 
					fullchange = 3; 
				dev->scrblank = (dev->scrblank & ~0x20) | (val & 0x20); 
				break;

			case 2: 
				dev->writemask = val & 0x0f; 
				break;

			case 3:
				dev->charsetb = (((val >> 2) & 3) * 0x10000) + 2;
				dev->charseta = ((val & 3)	* 0x10000) + 2;
				break;

			case 4:
				dev->chain2_write = !(val & 4);
				break;
		}
		break;

	case 0x03ce: 
		dev->gdcaddr = val; 
		break;

	case 0x03cf:
		dev->gdcreg[dev->gdcaddr & 15] = val;
		switch (dev->gdcaddr & 15) {
			case 2: 
				dev->colourcompare = val; 
				break;

			case 4: 
				dev->readplane = val & 3; 
				break;

			case 5: 
				dev->writemode = val & 3;
				dev->readmode = val & 8; 
				dev->chain2_read = val & 0x10;
				break;

			case 6:
				switch (val & 0x0c) {
					case 0x00: /*128K at A0000*/
						mem_map_set_addr(&dev->mapping, 0xa0000, 0x20000);
						break;

					case 0x04: /*64K at A0000*/
						mem_map_set_addr(&dev->mapping, 0xa0000, 0x10000);
						break;

					case 0x08: /*32K at B0000*/
						mem_map_set_addr(&dev->mapping, 0xb0000, 0x08000);
						break;

					case 0x0c: /*32K at B8000*/
						mem_map_set_addr(&dev->mapping, 0xb8000, 0x08000);
					break;
				}
				break;

			case 7: 
				dev->colournocare = val; 
				break;
		}
		break;

	case 0x03d0:
	case 0x03d4:
		dev->crtcreg = val & 31;
		return;

	case 0x03d1:
	case 0x03d5:
		if (dev->crtcreg <= 7 && dev->crtc[0x11] & 0x80) return;
		old = dev->crtc[dev->crtcreg];
		dev->crtc[dev->crtcreg] = val;
		if (old != val) {
			if (dev->crtcreg < 0x0e || dev->crtcreg > 0x10) {
				fullchange = changeframecount;
				ega_recalctimings(dev);
			}
		}
		break;
    }
}


uint8_t
ega_in(uint16_t port, priv_t priv)
{
    ega_t *dev = (ega_t *)priv;
    uint8_t ret = 0xff;

    if (((port & 0xfff0) == 0x03d0 ||
	 (port & 0xfff0) == 0x03b0) && !(dev->miscout & 1)) port ^= 0x60;

    switch (port) {
	case 0x03c0: 
		ret = dev->attraddr;
		break;

	case 0x03c1: 
		ret = dev->attrregs[dev->attraddr];
		break;

	case 0x03c2:
		return (egaswitches & (8 >> egaswitchread)) ? 0x10 : 0x00;
		break;

	case 0x03c4: 
		ret = dev->seqaddr;
		break;

	case 0x03c5:
		ret = dev->seqregs[dev->seqaddr & 0xf];
		break;

	case 0x03c8:
		ret = 2;
		break;

	case 0x03cc: 
		ret = dev->miscout;
		break;

	case 0x03ce: 
		ret = dev->gdcaddr;
		break;

	case 0x03cf:
		ret = dev->gdcreg[dev->gdcaddr & 0x0f];
		break;

	case 0x03d0:
	case 0x03d4:
		ret = dev->crtcreg;
		break;

	case 0x03d1:
	case 0x03d5:
		ret = dev->crtc[dev->crtcreg];
		break;

	case 0x03da:
		dev->attrff = 0;
		dev->stat ^= 0x30; /*Fools IBM EGA video BIOS self-test*/
		ret = dev->stat;
		break;
    }

    return(ret);
}


void
ega_write(uint32_t addr, uint8_t val, priv_t priv)
{
    ega_t *dev = (ega_t *)priv;
    uint8_t vala, valb, valc, vald;
    int writemask2 = dev->writemask;

    cycles -= video_timing_write_b;
	
    if (addr >= 0xb0000)
	addr &= 0x7fff;
    else
	addr &= 0xffff;

    if (dev->chain2_write) {
	writemask2 &= ~0x0a;
	if (addr & 1)
		writemask2 <<= 1;
	addr &= ~1;
	if (addr & 0x4000)
		addr |= 1;
	addr &= ~0x4000;
    }

    addr <<= 2;

    if (addr >= dev->vram_limit)
	return;

    if (!(dev->gdcreg[6] & 1)) 
	fullchange = 2;

    switch (dev->writemode) {
	case 1:
		if (writemask2 & 1) dev->vram[addr]       = dev->la;
		if (writemask2 & 2) dev->vram[addr | 0x1] = dev->lb;
		if (writemask2 & 4) dev->vram[addr | 0x2] = dev->lc;
		if (writemask2 & 8) dev->vram[addr | 0x3] = dev->ld;
		break;

	case 0:
		if (dev->gdcreg[3] & 7) 
			val = ega_rotate[dev->gdcreg[3] & 7][val];
			
		if (dev->gdcreg[8] == 0xff && !(dev->gdcreg[3] & 0x18) && !dev->gdcreg[1]) {
			if (writemask2 & 1) dev->vram[addr]       = val;
			if (writemask2 & 2) dev->vram[addr | 0x1] = val;
			if (writemask2 & 4) dev->vram[addr | 0x2] = val;
			if (writemask2 & 8) dev->vram[addr | 0x3] = val;
		} else {
			if (dev->gdcreg[1] & 1)
				vala = (dev->gdcreg[0] & 1) ? 0xff : 0;
			else
				vala = val;
			if (dev->gdcreg[1] & 2)
				valb = (dev->gdcreg[0] & 2) ? 0xff : 0;
			else
				valb = val;
			if (dev->gdcreg[1] & 4)
				valc = (dev->gdcreg[0] & 4) ? 0xff : 0;
			else
				valc = val;
			if (dev->gdcreg[1] & 8)
				vald = (dev->gdcreg[0] & 8) ? 0xff : 0;
			else
				vald = val;

			switch (dev->gdcreg[3] & 0x18) {
				case 0: /*Set*/
					if (writemask2 & 1)
						dev->vram[addr]       = (vala & dev->gdcreg[8]) | (dev->la & ~dev->gdcreg[8]);
					if (writemask2 & 2) dev->vram[addr | 0x1] = (valb & dev->gdcreg[8]) | (dev->lb & ~dev->gdcreg[8]);
					if (writemask2 & 4)
						dev->vram[addr | 0x2] = (valc & dev->gdcreg[8]) | (dev->lc & ~dev->gdcreg[8]);
					if (writemask2 & 8)
						dev->vram[addr | 0x3] = (vald & dev->gdcreg[8]) | (dev->ld & ~dev->gdcreg[8]);
					break;

				case 8: /*AND*/
					if (writemask2 & 1)
						dev->vram[addr]       = (vala | ~dev->gdcreg[8]) & dev->la;
					if (writemask2 & 2)
						dev->vram[addr | 0x1] = (valb | ~dev->gdcreg[8]) & dev->lb;
					if (writemask2 & 4)
						dev->vram[addr | 0x2] = (valc | ~dev->gdcreg[8]) & dev->lc;
					if (writemask2 & 8)
						dev->vram[addr | 0x3] = (vald | ~dev->gdcreg[8]) & dev->ld;
					break;

				case 0x10: /*OR*/
					if (writemask2 & 1)
						dev->vram[addr]       = (vala & dev->gdcreg[8]) | dev->la;
					if (writemask2 & 2)
						dev->vram[addr | 0x1] = (valb & dev->gdcreg[8]) | dev->lb;
					if (writemask2 & 4)
						dev->vram[addr | 0x2] = (valc & dev->gdcreg[8]) | dev->lc;
					if (writemask2 & 8)
						dev->vram[addr | 0x3] = (vald & dev->gdcreg[8]) | dev->ld;
					break;

				case 0x18: /*XOR*/
					if (writemask2 & 1)
						dev->vram[addr]       = (vala & dev->gdcreg[8]) ^ dev->la;
					if (writemask2 & 2)
						dev->vram[addr | 0x1] = (valb & dev->gdcreg[8]) ^ dev->lb;
					if (writemask2 & 4)
						dev->vram[addr | 0x2] = (valc & dev->gdcreg[8]) ^ dev->lc;
					if (writemask2 & 8)
						dev->vram[addr | 0x3] = (vald & dev->gdcreg[8]) ^ dev->ld;
					break;
			}
		}
		break;

	case 2:
		if (!(dev->gdcreg[3] & 0x18) && !dev->gdcreg[1]) {
			if (writemask2 & 1)
				dev->vram[addr]       = (((val & 1) ? 0xff : 0) & dev->gdcreg[8]) | (dev->la & ~dev->gdcreg[8]);
			if (writemask2 & 2)
				dev->vram[addr | 0x1] = (((val & 2) ? 0xff : 0) & dev->gdcreg[8]) | (dev->lb & ~dev->gdcreg[8]);
			if (writemask2 & 4)
				dev->vram[addr | 0x2] = (((val & 4) ? 0xff : 0) & dev->gdcreg[8]) | (dev->lc & ~dev->gdcreg[8]);
			if (writemask2 & 8)
				dev->vram[addr | 0x3] = (((val & 8) ? 0xff : 0) & dev->gdcreg[8]) | (dev->ld & ~dev->gdcreg[8]);
		} else {
			vala = ((val & 1) ? 0xff : 0);
			valb = ((val & 2) ? 0xff : 0);
			valc = ((val & 4) ? 0xff : 0);
			vald = ((val & 8) ? 0xff : 0);

			switch (dev->gdcreg[3] & 0x18) {
				case 0: /*Set*/
					if (writemask2 & 1)
						dev->vram[addr]       = (vala & dev->gdcreg[8]) | (dev->la & ~dev->gdcreg[8]);
					if (writemask2 & 2)
						dev->vram[addr | 0x1] = (valb & dev->gdcreg[8]) | (dev->lb & ~dev->gdcreg[8]);
					if (writemask2 & 4)
						dev->vram[addr | 0x2] = (valc & dev->gdcreg[8]) | (dev->lc & ~dev->gdcreg[8]);
					if (writemask2 & 8)
						dev->vram[addr | 0x3] = (vald & dev->gdcreg[8]) | (dev->ld & ~dev->gdcreg[8]);
					break;

				case 8: /*AND*/
					if (writemask2 & 1)
						dev->vram[addr]       = (vala | ~dev->gdcreg[8]) & dev->la;
					if (writemask2 & 2)
						dev->vram[addr | 0x1] = (valb | ~dev->gdcreg[8]) & dev->lb;
					if (writemask2 & 4)
						dev->vram[addr | 0x2] = (valc | ~dev->gdcreg[8]) & dev->lc;
					if (writemask2 & 8)
						dev->vram[addr | 0x3] = (vald | ~dev->gdcreg[8]) & dev->ld;
					break;

				case 0x10: /*OR*/
					if (writemask2 & 1)
						dev->vram[addr]       = (vala & dev->gdcreg[8]) | dev->la;
					if (writemask2 & 2)
						dev->vram[addr | 0x1] = (valb & dev->gdcreg[8]) | dev->lb;
					if (writemask2 & 4)
						dev->vram[addr | 0x2] = (valc & dev->gdcreg[8]) | dev->lc;
					if (writemask2 & 8)
						dev->vram[addr | 0x3] = (vald & dev->gdcreg[8]) | dev->ld;
					break;

				case 0x18: /*XOR*/
					if (writemask2 & 1)
						dev->vram[addr]       = (vala & dev->gdcreg[8]) ^ dev->la;
					if (writemask2 & 2)
						dev->vram[addr | 0x1] = (valb & dev->gdcreg[8]) ^ dev->lb;
					if (writemask2 & 4)
						dev->vram[addr | 0x2] = (valc & dev->gdcreg[8]) ^ dev->lc;
					if (writemask2 & 8)
						dev->vram[addr | 0x3] = (vald & dev->gdcreg[8]) ^ dev->ld;
					break;
			}
		}
		break;
    }
}


uint8_t
ega_read(uint32_t addr, priv_t priv)
{
    ega_t *dev = (ega_t *)priv;
    uint8_t temp, temp2, temp3, temp4;
    int readplane = dev->readplane;
	
    cycles -= video_timing_read_b;

    if (addr >= 0xb0000)
	addr &= 0x7fff;
    else
	addr &= 0xffff;

    if (dev->chain2_read) {
	readplane = (readplane & 2) | (addr & 1);
	addr &= ~1;
	if (addr & 0x4000)
		addr |= 1;
	addr &= ~0x4000;
    }

    addr <<= 2;

    if (addr >= dev->vram_limit)
	return 0xff;

    dev->la = dev->vram[addr];
    dev->lb = dev->vram[addr | 0x1];
    dev->lc = dev->vram[addr | 0x2];
    dev->ld = dev->vram[addr | 0x3];
    if (dev->readmode) {
	temp   = dev->la;
	temp  ^= (dev->colourcompare & 1) ? 0xff : 0;
	temp  &= (dev->colournocare & 1)  ? 0xff : 0;
	temp2  = dev->lb;
	temp2 ^= (dev->colourcompare & 2) ? 0xff : 0;
	temp2 &= (dev->colournocare & 2)  ? 0xff : 0;
	temp3  = dev->lc;
	temp3 ^= (dev->colourcompare & 4) ? 0xff : 0;
	temp3 &= (dev->colournocare & 4)  ? 0xff : 0;
	temp4  = dev->ld;
	temp4 ^= (dev->colourcompare & 8) ? 0xff : 0;
	temp4 &= (dev->colournocare & 8)  ? 0xff : 0;
	return ~(temp | temp2 | temp3 | temp4);
    }

    return dev->vram[addr | readplane];
}


void
ega_init(ega_t *dev, int monitor_type, int is_mono)
{
    int c, d, e;
	
    dev->vram = (uint8_t *)mem_alloc(0x40000);
    dev->vram_limit = 256 * 1024;
    dev->vrammask = dev->vram_limit - 1;

    for (c = 0; c < 256; c++) {
	e = c;
	for (d = 0; d < 8; d++) {
		ega_rotate[d][c] = e;
		e = (e >> 1) | ((e & 1) ? 0x80 : 0);
	}
    }

    for (c = 0; c < 4; c++) {
	for (d = 0; d < 4; d++) {
		edatlookup[c][d] = 0;
		if (c & 1) edatlookup[c][d] |= 1;
		if (d & 1) edatlookup[c][d] |= 2;
		if (c & 2) edatlookup[c][d] |= 0x10;
		if (d & 2) edatlookup[c][d] |= 0x20;
	}
    }

    if (is_mono) {
	for (c = 0; c < 256; c++) {
		switch (monitor_type >> 4) {
			case DISPLAY_GREEN:
				switch ((c >> 3) & 3) {
					case 0:
						pallook64[c] = pallook16[c] = makecol32(0, 0, 0);
						break;

					case 2:
						pallook64[c] = pallook16[c] = makecol32(0x04, 0x8a, 0x20);
						break;

					case 1:
						pallook64[c] = pallook16[c] = makecol32(0x08, 0xc7, 0x2c);
						break;

					case 3:
						pallook64[c] = pallook16[c] = makecol32(0x34, 0xff, 0x5d);
						break;
				}
				break;

			case DISPLAY_AMBER:
				switch ((c >> 3) & 3) {
					case 0:
						pallook64[c] = pallook16[c] = makecol32(0, 0, 0);
						break;

					case 2:
						pallook64[c] = pallook16[c] = makecol32(0xb2, 0x4d, 0x00);
						break;

					case 1:
						pallook64[c] = pallook16[c] = makecol32(0xef, 0x79, 0x00);
						break;

					case 3:
						pallook64[c] = pallook16[c] = makecol32(0xff, 0xe3, 0x34);
						break;
				}
				break;

			case DISPLAY_WHITE:
			default:
				switch ((c >> 3) & 3) {
					case 0:
						pallook64[c] = pallook16[c] = makecol32(0, 0, 0);
						break;

					case 2:
						pallook64[c] = pallook16[c] = makecol32(0x7a, 0x81, 0x83);
						break;

					case 1:
						pallook64[c] = pallook16[c] = makecol32(0xaf, 0xb3, 0xb0);
						break;

					case 3:
						pallook64[c] = pallook16[c] = makecol32(0xff, 0xfd, 0xed);
						break;
				}
				break;
		}
	}
    } else {
	for (c = 0; c < 256; c++) {
		pallook64[c]  = makecol32(((c >> 2) & 1) * 0xaa, ((c >> 1) & 1) * 0xaa, (c & 1) * 0xaa);
		pallook64[c] += makecol32(((c >> 5) & 1) * 0x55, ((c >> 4) & 1) * 0x55, ((c >> 3) & 1) * 0x55);
		pallook16[c]  = makecol32(((c >> 2) & 1) * 0xaa, ((c >> 1) & 1) * 0xaa, (c & 1) * 0xaa);
		pallook16[c] += makecol32(((c >> 4) & 1) * 0x55, ((c >> 4) & 1) * 0x55, ((c >> 4) & 1) * 0x55);
		if ((c & 0x17) == 6) 
			pallook16[c] = makecol32(0xaa, 0x55, 0);
	}
    }

    dev->pallook = pallook16;

    egaswitches = monitor_type & 0xf;

    old_overscan_color = 0;

    dev->miscout |= 0x22;
    dev->oddeven_page = 0;

    dev->seqregs[4] |= 2;
    dev->extvram = 1;

    update_overscan = 0;

    dev->crtc[0] = 63;
    dev->crtc[6] = 255;
}


static void
ega_close(priv_t priv)
{
    ega_t *dev = (ega_t *)priv;

    free(dev->vram);

    free(dev);
}


static void
speed_changed(priv_t priv)
{
    ega_t *dev = (ega_t *)priv;
	
    ega_recalctimings(dev);
}


static priv_t
ega_standalone_init(const device_t *info, UNUSED(void *parent))
{
    ega_t *dev;
    uint8_t temp;
    int c;

    dev = (ega_t *)mem_alloc(sizeof(ega_t));
    memset(dev, 0x00, sizeof(ega_t));
    dev->type = info->local;

    overscan_x = 16;
    overscan_y = 28;

    switch(dev->type) {
#ifdef JEGA
	case EGA_JEGA:
		LoadFontxFile(L"video/ibm/ega/jpnhn19x.fnt");
		LoadFontxFile(L"video/ibm/ega/jpnzn16x.fnt");
		break;
#endif

	default:
		break;
    }

    if (info->path != NULL) {
	rom_init(&dev->bios_rom, info->path,
		 0xc0000, 0x8000, 0x7fff, 0, MEM_MAPPING_EXTERNAL);

	if (dev->bios_rom.rom[0x3ffe] == 0xaa &&
	    dev->bios_rom.rom[0x3fff] == 0x55) {
		/*
		 * This ROM seems to have its data in reverse,
		 * which happens if it used inverted address
		 * lines. Quickly convert it back to regular
		 * order!
		 */
		ERRLOG("EGA: ROM (%ls) has reversed data!", info->path);
		for (c = 0; c < 0x2000; c++) {
			temp = dev->bios_rom.rom[c];
			dev->bios_rom.rom[c] = dev->bios_rom.rom[0x3fff - c];
			dev->bios_rom.rom[0x3fff - c] = temp;
		}
	}
    }

    c = device_get_config_int("monitor_type");
    ega_init(dev, c, (c & 0x0f) == 0x0b);

    dev->vram_limit = device_get_config_int("memory") * 1024;
    dev->vrammask = dev->vram_limit - 1;

    mem_map_add(&dev->mapping, 0xa0000, 0x20000,
		ega_read,NULL,NULL, ega_write,NULL,NULL,
		NULL, MEM_MAPPING_EXTERNAL, (priv_t)dev);

    timer_add(ega_poll, (priv_t)dev, &dev->vidtime, TIMER_ALWAYS_ENABLED);

    io_sethandler(0x03a0, 64,
		  ega_in,NULL,NULL, ega_out,NULL,NULL, (priv_t)dev);

    video_inform(DEVICE_VIDEO_GET(info->flags),
		 (const video_timings_t *)info->vid_timing);

    return((priv_t)dev);
}


/*
 * SW1 SW2 SW3 SW4
 * OFF OFF  ON OFF	Monochrome			(5151)	1011	0x0b
 *  ON OFF OFF  ON	Color 40x25			(5153)	0110	0x06
 * OFF OFF OFF  ON	Color 80x25			(5153)	0111	0x07
 *  ON  ON  ON OFF	Enhanced Color - Normal Mode	(5154)	1000	0x08
 * OFF  ON  ON OFF	Enhanced Color - Enhanced Mode	(5154)	1001	0x09
 *
 * 0 = Switch closed (ON);
 * 1 = Switch open   (OFF).
 */
static const device_config_t ega_config[] = {
    {
	"memory", "Memory size", CONFIG_SELECTION, "", 256,
	{
		{
			"64 KB", 64
		},
		{
			"128 KB", 128
		},
		{
			"256 KB", 256
		},
		{
			NULL
		}
	}
    },
    {
	"monitor_type","Monitor type",CONFIG_SELECTION,"",9,
	{
		{
			"Monochrome (5151/MDA) (white)",
                        0x0b | (DISPLAY_WHITE << 4)
                },
                {
                        "Monochrome (5151/MDA) (green)",
                        0x0b | (DISPLAY_GREEN << 4)
		},
		{
                        "Monochrome (5151/MDA) (amber)",
                        0x0b | (DISPLAY_AMBER << 4)
		},
		{
                        "Color 40x25 (5153/CGA)", 0x06
		},
		{
                        "Color 80x25 (5153/CGA)", 0x07
		},
		{
                        "Enhanced Color - Normal Mode (5154/ECD)", 0x08
		},
		{
                        "Enhanced Color - Enhanced Mode (5154/ECD)", 0x09
		},
		{
			NULL
		}
	}
    },
    {
	NULL
    }
};


static const video_timings_t ega_timing = {VID_ISA,8,16,32,8,16,32};
const device_t ega_device = {
    "EGA",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_ISA,
    EGA_IBM,
    BIOS_IBM_PATH,
    ega_standalone_init, ega_close, NULL,
    NULL,
    speed_changed,
    NULL,
    &ega_timing,
    ega_config
};

const device_t ega_onboard_device = {
    "EGA (onboard)",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_ISA,
    EGA_IBM,
    NULL,
    ega_standalone_init, ega_close, NULL,
    NULL,
    speed_changed,
    NULL,
    &ega_timing,
    ega_config
};


static const video_timings_t ega_compaq_timing = {VID_ISA,8,16,32,8,16,32};
const device_t ega_compaq_device = {
    "Compaq EGA",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_ISA,
    EGA_COMPAQ,
    BIOS_CPQ_PATH,
    ega_standalone_init, ega_close, NULL,
    NULL,
    speed_changed,
    NULL,
    &ega_compaq_timing,
    ega_config
};

static const video_timings_t sega_timing = {VID_ISA,8,16,32,8,16,32};
const device_t sega_device = {
    "SuperEGA",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_ISA,
    EGA_SUPEREGA,
    BIOS_SEGA_PATH,
    ega_standalone_init, ega_close, NULL,
    NULL,
    speed_changed,
    NULL,
    &sega_timing,
    ega_config
};

#ifdef JEGA
const device_t jega_device = {
    "AX JEGA",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_ISA,
    EGA_JEGA,
    NULL,
    ega_standalone_init, ega_close, NULL,
    NULL,			//FIXME: check JEGA roms/fonts?? --FvK
    speed_changed,
    NULL,
    &sega_timing,		//FIXME: check these??  --FvK
    ega_config
};
#endif

static const video_timings_t pega1a_timing = {VID_ISA,8,16,32,8,16,32};
const device_t pega1a_device = {
    "Paradide PEGA1A",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_ISA,
    EGA_PEGA1A,
    BIOS_PEGA1A_PATH,
    ega_standalone_init, ega_close, NULL,
    NULL,
    speed_changed,
    NULL,
    &pega1a_timing,
    ega_config
};

static const video_timings_t pega2a_timing = {VID_ISA,8,16,32,8,16,32};
const device_t pega2a_device = {
    "Paradide PEGA2A",
    DEVICE_VIDEO(VID_TYPE_SPEC) | DEVICE_ISA,
    EGA_PEGA2A,
    BIOS_PEGA2A_PATH,
    ega_standalone_init, ega_close, NULL,
    NULL,
    speed_changed,
    NULL,
    &pega2a_timing,
    ega_config
};
