/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the IBM EGA driver.
 *
 * Version:	@(#)vid_ega.h	1.0.7	2019/05/17
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		akm,
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
#ifndef VIDEO_EGA_H
# define VIDEO_EGA_H


#define DISPLAY_RGB 0
#define DISPLAY_COMPOSITE 1
#define DISPLAY_RGB_NO_BROWN 2
#define DISPLAY_GREEN 3
#define DISPLAY_AMBER 4
#define DISPLAY_WHITE 5

#ifdef JEGA
# define SBCS		0
# define DBCS		1
# define ID_LEN		6
# define NAME_LEN	8
# define SBCS19_LEN	256 * 19
# define DBCS16_LEN	65536 * 32
#endif


enum {
    EGA_IBM = 0,
    EGA_COMPAQ,
    EGA_SUPEREGA,
    EGA_JEGA,
    EGA_PEGA1A,
    EGA_PEGA2A
};


#if defined(EMU_MEM_H) && defined(EMU_ROM_H)
typedef struct {
    int		type;

    mem_map_t	mapping;

    rom_t	bios_rom;

    uint8_t	crtcreg;
    uint8_t	crtc[32];
    uint8_t	gdcreg[16];
    int		gdcaddr;
    uint8_t	attrregs[32];
    int		attraddr,
		attrff;
    int		attr_palette_enable;
    uint8_t	seqregs[64];
    int		seqaddr;

    uint8_t	miscout;
    int		vidclock;

    uint8_t	la, lb, lc, ld;

    uint8_t	stat;

    int		fast;
    uint8_t	colourcompare,
		colournocare;
    int		readmode,
		writemode,
		readplane;
    int		chain4,
		chain2_read, chain2_write;
    int		oddeven_page, oddeven_chain;
    int		extvram;
    uint8_t	writemask;
    uint32_t	charseta,
		charsetb;

    uint8_t	egapal[16];
    uint32_t	*pallook;

    int		vtotal, dispend, vsyncstart, split, vblankstart;
    int		hdisp,  htotal,  hdisp_time, rowoffset;
    int		lowres, interlace;
    int		linedbl, rowcount;
    double	clock;
    uint32_t	ma_latch;
        
    int		vres;
        
    tmrval_t	dispontime, dispofftime;
    tmrval_t	vidtime;
        
    uint8_t	scrblank;
        
    int		dispon;
    int		hdisp_on;

    uint32_t	ma, maback, ca;
    int		vc;
    int		sc;
    int		linepos, vslines, linecountff, oddeven;
    int		con, cursoron, blink;
    int		scrollcache;
        
    int		firstline, lastline;
    int		firstline_draw, lastline_draw;
    int		displine;
        
    uint8_t	*vram;
    int		vrammask;

    uint32_t	vram_limit;

    int		video_res_x, video_res_y, video_bpp;

#ifdef JEGA
    uint8_t	RMOD1, RMOD2, RDAGS, RDFFB, RDFSB, RDFAP,
		RPESL, RPULP, RPSSC, RPSSU, RPSSL;
    uint8_t	RPPAJ;
    uint8_t	RCMOD, RCCLH, RCCLL, RCCSL, RCCEL, RCSKW,
		ROMSL, RSTAT;
    int		font_index;
    int		chr_left,
		chr_wide;
#endif
} ega_t;
#endif


#ifdef JEGA
extern uint8_t	jfont_sbcs_19[SBCS19_LEN];	/* 256 * 19( * 8) */
extern uint8_t	jfont_dbcs_16[DBCS16_LEN];	/* 65536 * 16 * 2 (* 8) */
#endif


#if defined(EMU_MEM_H) && defined(EMU_ROM_H)
extern void	ega_init(ega_t *ega, int monitor_type, int is_mono);
extern void	ega_recalctimings(ega_t *ega);
#endif

extern void	ega_out(uint16_t addr, uint8_t val, priv_t);
extern uint8_t	ega_in(uint16_t addr, priv_t);
extern void	ega_poll(priv_t);
extern void	ega_write(uint32_t addr, uint8_t val, priv_t);
extern uint8_t	ega_read(uint32_t addr, priv_t);


#endif	/*VIDEO_EGA_H*/
