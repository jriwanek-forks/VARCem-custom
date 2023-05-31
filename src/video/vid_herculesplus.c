/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Hercules Plus emulation.
 *
 * Version:	@(#)vid_hercules_plus.c	1.0.23	2019/05/17
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
#include <86box/config.h>
#include <86box/timer.h>
#include <86box/io.h>
#include <86box/mem.h>
#include <86box/rom.h>
#include <86box/device.h>
#include <86box/plat.h>
#include <86box/clk.h>
#include <86box/parallel.h>
#include <86box/video.h>


/* extended CRTC registers */
#define HERCULESPLUS_CRTC_XMODE   20 /* xMode register */
#define HERCULESPLUS_CRTC_UNDER   21	/* Underline */
#define HERCULESPLUS_CRTC_OVER    22 /* Overstrike */

/* character width */
#define HERCULESPLUS_CW    ((dev->crtc[HERCULESPLUS_CRTC_XMODE] & HERCULESPLUS_XMODE_90COL) ? 8 : 9)

/* mode control register */
#define HERCULESPLUS_CTRL_GRAPH   0x02
#define HERCULESPLUS_CTRL_ENABLE  0x08
#define HERCULESPLUS_CTRL_BLINK   0x20
#define HERCULESPLUS_CTRL_PAGE1   0x80

/* CRTC status register */
#define HERCULESPLUS_STATUS_HSYNC 0x01		/* horizontal sync */
#define HERCULESPLUS_STATUS_LIGHT 0x02
#define HERCULESPLUS_STATUS_VIDEO 0x08
#define HERCULESPLUS_STATUS_ID    0x10		/* Card identification */
#define HERCULESPLUS_STATUS_VSYNC 0x80		/* -vertical sync */

/* configuration switch register */
#define HERCULESPLUS_CTRL2_GRAPH 0x01
#define HERCULESPLUS_CTRL2_PAGE1 0x02

/* extended mode register */
#define HERCULESPLUS_XMODE_RAMFONT 0x01
#define HERCULESPLUS_XMODE_90COL   0x02


typedef struct {
    mem_map_t	mapping;

    uint8_t	crtc[32];
    int		crtcreg;

    uint8_t	ctrl, ctrl2, stat;

    tmrval_t	dispontime, dispofftime;
    tmrval_t	vidtime;
    tmrval_t	vsynctime;

    int		firstline, lastline;

    int		linepos, displine;
    int		vc, sc;
    uint16_t	ma, maback;
    int		con, coff, cursoron;
    int		dispon, blink;
    int		vadj;
    int		blend;

    uint8_t	cols[256][2][2];

    uint8_t	*vram;
} herculesplus_t;


static void
recalc_timings(herculesplus_t *dev)
{
    double disptime;
    double _dispontime, _dispofftime;

    disptime = dev->crtc[0] + 1;
    _dispontime  = dev->crtc[1];
    _dispofftime = disptime - _dispontime;
    _dispontime  *= MDACONST;
    _dispofftime *= MDACONST;

    dev->dispontime  = (tmrval_t)(_dispontime  * (1 << TIMER_SHIFT));
    dev->dispofftime = (tmrval_t)(_dispofftime * (1 << TIMER_SHIFT));
}


static void
herculesplus_out(uint16_t port, uint8_t val, priv_t priv)
{
    herculesplus_t *dev = (herculesplus_t *)priv;
    uint8_t old;

    switch (port) {
	case 0x03b0:
	case 0x03b2:
	case 0x03b4:
	case 0x03b6:
		dev->crtcreg = val & 31;
		return;

	case 0x03b1:
	case 0x03b3:
	case 0x03b5:
	case 0x03b7:
		if (dev->crtcreg > 22) return;
		old = dev->crtc[dev->crtcreg];
		dev->crtc[dev->crtcreg] = val;
		if (dev->crtc[10] == 6 && dev->crtc[11] == 7) {
			/*Fix for Generic Turbo XT BIOS,
			 *which sets up cursor registers wrong*/
			dev->crtc[10] = 0xb;
			dev->crtc[11] = 0xc;
		}
		if (old != val)
			recalc_timings(dev);
		return;

	case 0x03b8:
		old = dev->ctrl;
		dev->ctrl = val;
		if (old != val)
			recalc_timings(dev);
		return;

	case 0x03bf:
		dev->ctrl2 = val;
		if (val & 2)
			mem_map_set_addr(&dev->mapping, 0xb0000, 0x10000);
		else
			mem_map_set_addr(&dev->mapping, 0xb0000, 0x08000);
		return;
    }
}


static uint8_t
herculesplus_in(uint16_t port, priv_t priv)
{
    herculesplus_t *dev = (herculesplus_t *)priv;
    uint8_t ret = 0xff;

    switch (port) {
	case 0x03b0:
	case 0x03b2:
	case 0x03b4:
	case 0x03b6:
		ret = dev->crtcreg;
		break;

	case 0x03b1:
	case 0x03b3:
	case 0x03b5:
	case 0x03b7:
		if (dev->crtcreg <= 22)
			ret = dev->crtc[dev->crtcreg];
		break;

	case 0x03ba:
		/* 0x10: HerculesPlus card identity */
		ret = (dev->stat & 0x0f) | ((dev->stat & 8) << 4) | 0x10;
		break;
    }

    return ret;
}


static void
herculesplus_write(uint32_t addr, uint8_t val, priv_t priv)
{
    herculesplus_t *dev = (herculesplus_t *)priv;

    dev->vram[addr & 0xffff] = val;
}


static uint8_t
herculesplus_read(uint32_t addr, priv_t priv)
{
    herculesplus_t *dev = (herculesplus_t *)priv;

    return dev->vram[addr & 0xffff];
}


static void
draw_char_rom(herculesplus_t *dev, int x, uint8_t chr, uint8_t attr)
{
    unsigned ull, val, ifg, ibg;
    const uint8_t *fnt;
    int i, elg, blk;
    int cw = HERCULESPLUS_CW;

    blk = 0;
    if (dev->ctrl & HERCULESPLUS_CTRL_BLINK) {
	if (attr & 0x80) 
		blk = (dev->blink & 16);
	attr &= 0x7f;
    }

    /* MDA-compatible attributes */
    ibg = 0;
    ifg = 7;
    if ((attr & 0x77) == 0x70) {	/* Invert */
	ifg = 0;
	ibg = 7;
    }	
    if (attr & 8) 
	ifg |= 8;			/* High intensity FG */
    if (attr & 0x80) 
	ibg |= 8;			/* High intensity BG */
    if ((attr & 0x77) == 0)		/* Blank */
	ifg = ibg;
    ull = ((attr & 0x07) == 1) ? 13 : 0xffff;

    if (dev->crtc[HERCULESPLUS_CRTC_XMODE] & HERCULESPLUS_XMODE_90COL) 
	elg = 0;
	else 
	elg = ((chr >= 0xc0) && (chr <= 0xdf));

    fnt = &(fontdatm[chr][dev->sc]);

    if (blk) {
	val = 0x000;		/* Blinking, draw all background */
    } else if (dev->sc == ull)  {
	val = 0x1ff;		/* Underscore, draw all foreground */
    } else {
	val = fnt[0] << 1;
	
	if (elg) 
		val |= (val >> 1) & 1;
    }

    for (i = 0; i < cw; i++) {
	screen->line[dev->displine][x * cw + i].pal = (val & 0x100) ? ifg : ibg;
	val = val << 1;
    }
}


static void
draw_char_ram4(herculesplus_t *dev, int x, uint8_t chr, uint8_t attr)
{
    unsigned ull, val, ifg, ibg, cfg;
    const uint8_t *fnt;
    int i, elg, blk;
    int cw = HERCULESPLUS_CW;
    int blink = dev->ctrl & HERCULESPLUS_CTRL_BLINK;

    blk = 0;
    if (blink) {
	if (attr & 0x80) 
		blk = (dev->blink & 16);
	attr &= 0x7f;
    }

    /* MDA-compatible attributes */
    ibg = 0;
    ifg = 7;
    if ((attr & 0x77) == 0x70) {	/* Invert */
	ifg = 0;
	ibg = 7;
    }	
    if (attr & 8) 
	ifg |= 8;			/* High intensity FG */
    if (attr & 0x80) 
	ibg |= 8;			/* High intensity BG */
    if ((attr & 0x77) == 0)		/* Blank */
	ifg = ibg;
    ull = ((attr & 0x07) == 1) ? 13 : 0xffff;
    if (dev->crtc[HERCULESPLUS_CRTC_XMODE] & HERCULESPLUS_XMODE_90COL) 
	elg = 0;
    else 
	elg = ((chr >= 0xc0) && (chr <= 0xdf));
    fnt = dev->vram + 0x4000 + 16 * chr + dev->sc;

    if (blk) {
	/* Blinking, draw all background */
	val = 0x000;	
    } else if (dev->sc == ull) {
	/* Underscore, draw all foreground */
	val = 0x1ff;
    } else {
	val = fnt[0x00000] << 1;
	
	if (elg) 
		val |= (val >> 1) & 1;
    }

    for (i = 0; i < cw; i++) {
	/* Generate pixel colour */
	cfg = 0;

	/* cfg = colour of foreground pixels */
	if ((attr & 0x77) == 0)
		cfg = ibg;	/* 'blank' attribute */

	screen->line[dev->displine][x * cw + i].pal = dev->cols[attr][blink][cfg];
	val = val << 1;
    }
}


static void
draw_char_ram48(herculesplus_t *dev, int x, uint8_t chr, uint8_t attr)
{
    int i, elg, blk, ul, ol, bld;
    unsigned ull, oll, ulc = 0, olc = 0;
    unsigned val, ibg, cfg;
    const unsigned char *fnt;
    int cw = HERCULESPLUS_CW;
    int blink = dev->ctrl & HERCULESPLUS_CTRL_BLINK;
    int font = (attr & 0x0F);

    if (font >= 12) font &= 7;

    blk = 0;
    if (blink) {
	if (attr & 0x40) 
		blk = (dev->blink & 16);
	attr &= 0x7f;
    }

    /* MDA-compatible attributes */
    if (blink) {
	ibg = (attr & 0x80) ? 8 : 0;
	bld = 0;
	ol  = (attr & 0x20) ? 1 : 0;
	ul  = (attr & 0x10) ? 1 : 0;
    } else {
	bld = (attr & 0x80) ? 1 : 0;
	ibg = (attr & 0x40) ? 0x0F : 0;
	ol  = (attr & 0x20) ? 1 : 0;
	ul  = (attr & 0x10) ? 1 : 0;
    }

    if (ul) { 
	ull = dev->crtc[HERCULESPLUS_CRTC_UNDER] & 0x0F;
	ulc = (dev->crtc[HERCULESPLUS_CRTC_UNDER] >> 4) & 0x0F;
	if (ulc == 0) ulc = 7;
    } else {
	ull = 0xFFFF;
    }

    if (ol) { 
	oll = dev->crtc[HERCULESPLUS_CRTC_OVER] & 0x0F;
	olc = (dev->crtc[HERCULESPLUS_CRTC_OVER] >> 4) & 0x0F;
	if (olc == 0) olc = 7;
    } else {
	oll = 0xFFFF;
    }

    if (dev->crtc[HERCULESPLUS_CRTC_XMODE] & HERCULESPLUS_XMODE_90COL) 
	elg = 0;
    else 
	elg = ((chr >= 0xc0) && (chr <= 0xdf));
    fnt = dev->vram + 0x4000 + 16 * chr + 4096 * font + dev->sc;

    if (blk) { /* Blinking, draw all background */
		val = 0x000;	
    } else if (dev->sc == ull) {
	/* Underscore, draw all foreground */
	val = 0x1ff;
    } else {
	val = fnt[0x00000] << 1;
	
	if (elg) 
		val |= (val >> 1) & 1;
	if (bld) 
		val |= (val >> 1);
    }

    for (i = 0; i < cw; i++) {
	/* Generate pixel colour */
	cfg = val & 0x100;
	if (dev->sc == oll)
		cfg = olc ^ ibg;	/* Strikethrough */
	else if (dev->sc == ull)
		cfg = ulc ^ ibg;	/* Underline */
	else
	   	cfg |= ibg;
		
	screen->line[dev->displine][(x * cw) + i].pal = dev->cols[attr][blink][cfg];
	val = val << 1;
    }
}


static void
text_line(herculesplus_t *dev, uint16_t ca)
{
    int drawcursor;
    int x, c;
    uint8_t chr, attr;
    uint32_t col;

    for (x = 0; x < dev->crtc[1]; x++) {
	if (dev->ctrl & 8) {
		chr  = dev->vram[(dev->ma << 1) & 0xfff];
		attr = dev->vram[((dev->ma << 1) + 1) & 0xfff];
	} else
		chr  = attr = 0;

	drawcursor = ((dev->ma == ca) && dev->con && dev->cursoron);

	switch (dev->crtc[HERCULESPLUS_CRTC_XMODE] & 5) {
		case 0:
		case 4:	/* ROM font */
			draw_char_rom(dev, x, chr, attr);
			break;

		case 1: /* 4k RAMfont */
			draw_char_ram4(dev, x, chr, attr);
			break;

		case 5: /* 48k RAMfont */
			draw_char_ram48(dev, x, chr, attr);
			break;
	}
	++dev->ma;

	if (drawcursor) {
		int cw = HERCULESPLUS_CW;

		col = dev->cols[attr][0][1];
		for (c = 0; c < cw; c++)
			screen->line[dev->displine][x * cw + c].pal = col;
	}
    }
}


static void
graphics_line(herculesplus_t *dev)
{
    uint16_t ca;
    int x, c, plane = 0;
    uint16_t val;

    /* Graphics mode. */
    ca = (dev->sc & 3) * 0x2000;
    if ((dev->ctrl & HERCULESPLUS_CTRL_PAGE1) && (dev->ctrl2 & HERCULESPLUS_CTRL2_PAGE1))
	ca += 0x8000;

    for (x = 0; x < dev->crtc[1]; x++) {
	if (dev->ctrl & 8)
		val = (dev->vram[((dev->ma << 1) & 0x1fff) + ca + 0x10000 * plane] << 8)
		    | dev->vram[((dev->ma << 1) & 0x1fff) + ca + 0x10000 * plane + 1];
	else
		val = 0;

	dev->ma++;
	for (c = 0; c < 16; c++) {
		val >>= 1;

		screen->line[dev->displine][(x << 4) + c].pal = (val & 1) ? 7 : 0;
	}

	if (dev->blend) {
		for (c = 0; c < 16; c += 8)
			video_blend((x << 4) + c, dev->displine);
	}
    }
}


static void
herculesplus_poll(priv_t priv)
{
    herculesplus_t *dev = (herculesplus_t *)priv;
    uint16_t ca = (dev->crtc[15] | (dev->crtc[14] << 8)) & 0x3fff;
    int x, oldvc, oldsc;

    if (! dev->linepos) {
	dev->vidtime += dev->dispofftime;
	dev->stat |= 1;
	dev->linepos = 1;
	oldsc = dev->sc;
	if ((dev->crtc[8] & 3) == 3) 
		dev->sc = (dev->sc << 1) & 7;
	if (dev->dispon) {
		if (dev->displine < dev->firstline) {
			dev->firstline = dev->displine;
			video_blit_wait_buffer();
		}
		dev->lastline = dev->displine;

		if ((dev->ctrl & HERCULESPLUS_CTRL_GRAPH) && (dev->ctrl2 & HERCULESPLUS_CTRL2_GRAPH))
			graphics_line(dev);
		else
			text_line(dev, ca);
	}
	dev->sc = oldsc;
	if (dev->vc == dev->crtc[7] && !dev->sc)
		dev->stat |= 8;
	dev->displine++;
	if (dev->displine >= 500) 
		dev->displine = 0;
    } else {
	dev->vidtime += dev->dispontime;
	if (dev->dispon) 
		dev->stat &= ~1;
	dev->linepos = 0;
	if (dev->vsynctime) {
		dev->vsynctime--;
		if (! dev->vsynctime)
			dev->stat &= ~8;
	}

	if (dev->sc == (dev->crtc[11] & 31) || ((dev->crtc[8] & 3) == 3 && dev->sc == ((dev->crtc[11] & 31) >> 1))) { 
		dev->con = 0; 
		dev->coff = 1; 
	}
	if (dev->vadj) {
		dev->sc++;
		dev->sc &= 31;
		dev->ma = dev->maback;
		dev->vadj--;
		if (! dev->vadj) {
			dev->dispon = 1;
			dev->ma = dev->maback = (dev->crtc[13] | (dev->crtc[12] << 8)) & 0x3fff;
			dev->sc = 0;
		}
	} else if (dev->sc == dev->crtc[9] || ((dev->crtc[8] & 3) == 3 && dev->sc == (dev->crtc[9] >> 1))) {
		dev->maback = dev->ma;
		dev->sc = 0;
		oldvc = dev->vc;
		dev->vc++;
		dev->vc &= 127;
		if (dev->vc == dev->crtc[6]) 
			dev->dispon = 0;
		if (oldvc == dev->crtc[4]) {
			dev->vc = 0;
			dev->vadj = dev->crtc[5];
			if (! dev->vadj)
				dev->dispon = 1;
			if (! dev->vadj)
				dev->ma = dev->maback = (dev->crtc[13] | (dev->crtc[12] << 8)) & 0x3fff;
			if ((dev->crtc[10] & 0x60) == 0x20)
				dev->cursoron = 0;
			else
				dev->cursoron = dev->blink & 16;
		}
		if (dev->vc == dev->crtc[7]) {
			dev->dispon = 0;
			dev->displine = 0;
			dev->vsynctime = 16;
			if (dev->crtc[7]) {
				if ((dev->ctrl & HERCULESPLUS_CTRL_GRAPH) && (dev->ctrl2 & HERCULESPLUS_CTRL2_GRAPH)) 
					x = dev->crtc[1] << 4;
				else
					x = dev->crtc[1] * 9;
				dev->lastline++;
				if ((dev->ctrl & 8) &&
				    ((x != xsize) || ((dev->lastline - dev->firstline) != ysize) || video_force_resize_get())) {
					xsize = x;
					ysize = dev->lastline - dev->firstline;
					if (xsize < 64) xsize = 656;
					if (ysize < 32) ysize = 200;
					set_screen_size(xsize, ysize);

					if (video_force_resize_get())
						video_force_resize_set(0);
				}

				video_blit_start(1, 0, dev->firstline, 0, dev->lastline - dev->firstline, xsize, dev->lastline - dev->firstline);
				frames++;

				if ((dev->ctrl & HERCULESPLUS_CTRL_GRAPH) && (dev->ctrl2 & HERCULESPLUS_CTRL2_GRAPH)) {
					video_res_x = dev->crtc[1] * 16;
					video_res_y = dev->crtc[6] * 4;
					video_bpp = 1;
				} else {
					video_res_x = dev->crtc[1];
					video_res_y = dev->crtc[6];
					video_bpp = 0;
				}
			}
			dev->firstline = 1000;
			dev->lastline = 0;
			dev->blink++;
		}
	} else {
		dev->sc++;
		dev->sc &= 31;
		dev->ma = dev->maback;
	}

	if ((dev->sc == (dev->crtc[10] & 31) || ((dev->crtc[8] & 3) == 3 && dev->sc == ((dev->crtc[10] & 31) >> 1))))
		dev->con = 1;
    }
}


static priv_t
herculesplus_init(const device_t *info, UNUSED(void *parent))
{
    herculesplus_t *dev;
    int c;

    dev = (herculesplus_t *)mem_alloc(sizeof(herculesplus_t));
    memset(dev, 0, sizeof(herculesplus_t));

    dev->blend = device_get_config_int("blend");

    cga_palette = device_get_config_int("rgb_type") << 1;
    if (cga_palette > 6)
	cga_palette = 0;
    video_palette_rebuild();

    dev->vram = (uint8_t *)mem_alloc(0x10000);	/* 64k VRAM */

    timer_add(herculesplus_poll, (priv_t)dev, &dev->vidtime, TIMER_ALWAYS_ENABLED);

    mem_map_add(&dev->mapping, 0xb0000, 0x10000,
		herculesplus_read,NULL,NULL,
		herculesplus_write,NULL,NULL,
		dev->vram, MEM_MAPPING_EXTERNAL, (priv_t)dev);

    io_sethandler(0x03b0, 16,
		  herculesplus_in,NULL, NULL, herculesplus_out,NULL,NULL, (priv_t)dev);

    for (c = 0; c < 256; c++) {
	dev->cols[c][0][0] = dev->cols[c][1][0] = dev->cols[c][1][1] = 16;
	if (c & 8)
		dev->cols[c][0][1] = 15 + 16;
	else
		dev->cols[c][0][1] =  7 + 16;
    }
    dev->cols[0x70][0][1] = 16;
    dev->cols[0x70][0][0] = dev->cols[0x70][1][0] =
		dev->cols[0x70][1][1] = 16 + 15;
    dev->cols[0xF0][0][1] = 16;
    dev->cols[0xF0][0][0] = dev->cols[0xF0][1][0] =
		dev->cols[0xF0][1][1] = 16 + 15;
    dev->cols[0x78][0][1] = 16 + 7;
    dev->cols[0x78][0][0] = dev->cols[0x78][1][0] =
		dev->cols[0x78][1][1] = 16 + 15;
    dev->cols[0xF8][0][1] = 16 + 7;
    dev->cols[0xF8][0][0] = dev->cols[0xF8][1][0] =
		dev->cols[0xF8][1][1] = 16 + 15;
    dev->cols[0x00][0][1] = dev->cols[0x00][1][1] = 16;
    dev->cols[0x08][0][1] = dev->cols[0x08][1][1] = 16;
    dev->cols[0x80][0][1] = dev->cols[0x80][1][1] = 16;
    dev->cols[0x88][0][1] = dev->cols[0x88][1][1] = 16;

    video_inform(DEVICE_VIDEO_GET(info->flags),
		 (const video_timings_t *)info->vid_timing);

    /* Force the LPT3 port to be enabled. */
    config.parallel_enabled[2] = 1;
    parallel_setup(2, 0x03bc);

    return (priv_t)dev;
}


static void
herculesplus_close(priv_t priv)
{
    herculesplus_t *dev = (herculesplus_t *)priv;

    if (! dev)
	return;

    if (dev->vram)
	free(dev->vram);

    free(dev);
}


static void
speed_changed(priv_t priv)
{
    herculesplus_t *dev = (herculesplus_t *)priv;

    recalc_timings(dev);
}



static const device_config_t herculesplus_config[] = {
    {
	"rgb_type", "Display type", CONFIG_SELECTION, "", 0,
	{
		{
			"Default", 0
		},
		{
			"Green", 1
		},
		{
			"Amber", 2
		},
		{
			"Gray", 3
		},
		{
			NULL
		}
	}
    },
    {
	"blend", "Blend", CONFIG_BINARY, "", 1
    },
    {
	NULL
    }
};


static const video_timings_t herculesplus_timings = { VID_ISA,8,16,32,8,16,32 };

const device_t herculesplus_device = {
    "Hercules Plus",
    DEVICE_VIDEO(VID_TYPE_MDA) | DEVICE_ISA,
    0,
    NULL,
    herculesplus_init, herculesplus_close, NULL,
    NULL,
    speed_changed,
    NULL,
    &herculesplus_timings,
    herculesplus_config
};
