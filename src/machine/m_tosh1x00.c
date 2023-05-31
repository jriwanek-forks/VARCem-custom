/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the Toshiba T1000 and T1200 portables.
 *
 *		The T1000 is the T3100e's little brother -- a real laptop
 *		with a rechargeable battery. 
 *
 *		Features: 80C88 at 4.77MHz
 *		- 512k system RAM
 *		- 640x200 monochrome LCD
 *		- 82-key keyboard 
 *		- Real-time clock. Not the normal 146818, but a TC8521,
 *		   which is a 4-bit chip.
 *		- A ROM drive (128k, 256k or 512k) which acts as a mini
 *		  hard drive and contains a copy of DOS 2.11. 
 *		- 160 bytes of non-volatile RAM for the CONFIG.SYS used
 *		  when booting from the ROM drive. Possibly physically
 *		  located in the keyboard controller RAM.
 *
 *		An optional memory expansion board can be fitted. This adds
 *		768K of RAM, which can be used for up to three purposes:
 *		> Conventional memory -- 128K between 512K and 640K
 *		> HardRAM -- a battery-backed RAM drive.
 *		> EMS
 *
 *		This means that there are up to three different
 *		implementations of non-volatile RAM in the same computer
 *		(52 nibbles in the TC8521, 160 bytes of CONFIG.SYS, and
 *		up to 768K of HardRAM).
 *
 *		The T1200 is a slightly upgraded version with a turbo mode
 *		(double CPU clock, 9.54MHz) and an optional hard drive.
 *
 *		The hard drive is a JVC JD-3824R00-1 (20MB, 615/2/34) RLL
 *		3.5" drive with custom 26-pin interface. This is what is
 *		known about the drive:
 *
 *		  JD-3824G is a 20MB hard disk drive that JVC made as one
 *		  of OEM products. This HDD was shipped to Laptop PC
 *		  manufactures.
 *
 *		  This HDD was discontinued about 10 years ago and JVC
 *		  America does not have detail information because of the
 *		  OEM product.
 *
 *		  This is the information that I have.
 *		  [JD-3824G]
 *		  1. 3.5" 20MB Hard Disk Drive
 *		  2. JVC original interface (26 pin)
 *		     This is not the IDE interface. This drive was made long
 *		     before IDE standard. It required special controller board.
 *		     This interface is not compatible with any other disk
 *		     interface.
 *
 *		  Pin 1 GND			Pin 2 -Read Data
 *		  Pin 3 GND			Pin 4 -Write data
 *		  Pin 5 GND			Pin 6 Reserved
 *		  Pin 7 -Drive Select/+Power Save  Pin 8 -Ship Ready
 *		  Pin 9 GND			Pin 10 +Read/-Write control
 *		  Pin 11 -Motor On	Pin 12 Head Select(+Head 0/ - Head1)
 *		  Pin 13 -Direction In		Pin 14 -Step
 *		  Pin 15 -Write Fault		Pin 16 -Seek Complete
 *		  Pin 17 -Servo Gate		Pin 18 -Index
 *		  Pin 19 -Track 000		Pin 20 -Drive Ready
 *		  Pin 21 GND			Pin 22 +5V
 *		  Pin 23 GND			Pin 24 +5V
 *		  Pin 25 GND			Pin 26 +12V
 *
 *		  3. Parameters( 2 heads, 34 sectors, 615 cylinders)
 *		  4. 2-7 RLL coding
 *		  5. Spindle Rotation: 2597 rpm
 *		  6. Data Transfer: 7.5M bps
 *		  7. Average Access: 78ms
 *		  8. Power Voltage: 5V and 12V
 *
 *		(from Jeff Kishida, JVC Americas, Corp.
 *		 Tel: 714-827-6267 Fax: 714-827-8740
 *		 Email: Jeff.Kishida@worldnet.att.net)
 *
 *		The controller is on a separate PCB, and contains a T7518,
 *		DC2090P166A and a 27256 EPROM (label "036A") with BIOS. The
 *		interface for this is proprietary at the rogramming level.
 *
 *		01F2h: If hard drive is present, low 4 bits are 0Ch [20Mb]
 *			or 0Dh [10Mb]. 
 *
 *		The TC8521 is a 4-bit RTC, so each memory location can only
 *		hold a single BCD digit. Hence everything has 'ones' and
 *		'tens' digits.
 *
 * FIXME:	The ROM drive should be re-done using the "option file".
 *
 * Version:	@(#)m_tosh1x00.c	1.0.26	2021/03/18
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		John Elliott, <jce@seasip.info>
 *
 *		Copyright 2018-2021 Fred N. van Kempen.
 *		Copyright 2018 Miran Grca.
 *		Copyright 2017,2018 John Elliott.
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
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <time.h>
#include <86box/86box.h>
#include <86box/config.h>
#include <86box/timer.h>
#include "../cpu/cpu.h"
#include <86box/io.h>
#include <86box/mem.h>
#include <86box/rom.h>
#include <86box/device.h>
#include <86box/nvr.h>
#include <86box/nmi.h>
#include <86box/pit.h>
#include <86box/parallel.h>
#include <86box/keyboard.h>
#include <86box/fdd.h>
#include <86box/fdc.h>
#include <86box/hdc.h>
#include <86box/video.h>
#include <86box/plat.h>
#include <86box/machine.h>
#include <86box/m_tosh1x00.h>


#define T1000_ROMDOS_SIZE	(512*1024UL)	/* Max romdrive size is 512k */
#define T1000_ROMDOS_PATH	L"machines/toshiba/t1000/t1000dos.rom"
#define T1000_FONT_PATH		L"machines/toshiba/t1000/t1000font.rom"
#define T1200_FONT_PATH		L"machines/toshiba/t1200/t1000font.bin"


enum TC8521_ADDR {
    /* Page 0 registers */
    TC8521_SECOND1 = 0,
    TC8521_SECOND10,
    TC8521_MINUTE1,
    TC8521_MINUTE10,
    TC8521_HOUR1,
    TC8521_HOUR10,
    TC8521_WEEKDAY,
    TC8521_DAY1,
    TC8521_DAY10,
    TC8521_MONTH1,
    TC8521_MONTH10,
    TC8521_YEAR1,
    TC8521_YEAR10,
    TC8521_PAGE,	/* PAGE register */
    TC8521_TEST,	/* TEST register */
    TC8521_RESET,	/* RESET register */

    /* Page 1 registers */
    TC8521_24HR = 0x1A,
    TC8521_LEAPYEAR = 0x1B
};


typedef struct {
    /* ROM drive */
    int8_t	rom_dos,
		is_t1200;

    uint8_t	rom_ctl;
    uint32_t	rom_offset;
    uint8_t	*romdrive;
    mem_map_t	rom_mapping;

    /* CONFIG.SYS drive. */
    wchar_t	cfgsys_fn[128];
    uint16_t	cfgsys_len;
    uint8_t	*cfgsys;

    /* System control registers */
    uint8_t	sys_ctl[16];
    uint8_t	syskeys;
    uint8_t	turbo;

    /* NVRAM control */
    uint8_t	nvr_c0;	
    uint8_t	nvr_tick;
    int		nvr_addr;
    uint8_t	nvr_active;
    mem_map_t	nvr_mapping;		/* T1200 NVRAM mapping */

    /* EMS data */
    uint8_t	ems_reg[4];
    mem_map_t	mapping[4];
    uint32_t	page_exec[4];
    uint8_t	ems_port_index;
    uint16_t	ems_port;
    uint8_t	is_640k;
    uint32_t	ems_base;
    int32_t	ems_pages;

    fdc_t	*fdc;

    nvr_t	nvr;
} t1000_t;


/* Set the chip time. */
static void
tc8521_time_set(uint8_t *regs, const intclk_t *clk)
{
    regs[TC8521_SECOND1] = (clk->tm_sec % 10);
    regs[TC8521_SECOND10] = (clk->tm_sec / 10);
    regs[TC8521_MINUTE1] = (clk->tm_min % 10);
    regs[TC8521_MINUTE10] = (clk->tm_min / 10);
    if (regs[TC8521_24HR] & 0x01) {
	regs[TC8521_HOUR1] = (clk->tm_hour % 10);
	regs[TC8521_HOUR10] = (clk->tm_hour / 10);
    } else {
	regs[TC8521_HOUR1] = ((clk->tm_hour % 12) % 10);
	regs[TC8521_HOUR10] = (((clk->tm_hour % 12) / 10) |
			       ((clk->tm_hour >= 12) ? 2 : 0));
    }
    regs[TC8521_WEEKDAY] = clk->tm_wday;
    regs[TC8521_DAY1] = (clk->tm_mday % 10);
    regs[TC8521_DAY10] = (clk->tm_mday / 10);
    regs[TC8521_MONTH1] = ((clk->tm_mon + 1) % 10);
    regs[TC8521_MONTH10] = ((clk->tm_mon + 1) / 10);
    regs[TC8521_YEAR1] = ((clk->tm_year - 80) % 10);
    regs[TC8521_YEAR10] = (((clk->tm_year - 80) % 100) / 10);
}


/* Get the chip time. */
#define nibbles(a)	(regs[(a##1)] + 10 * regs[(a##10)])
static void
tc8521_time_get(const uint8_t *regs, intclk_t *clk)
{
    clk->tm_sec = nibbles(TC8521_SECOND);
    clk->tm_min = nibbles(TC8521_MINUTE);
    if (regs[TC8521_24HR] & 0x01)
	clk->tm_hour = nibbles(TC8521_HOUR);
      else
	clk->tm_hour = ((nibbles(TC8521_HOUR) % 12) +
		      (regs[TC8521_HOUR10] & 0x02) ? 12 : 0);
    clk->tm_wday = regs[TC8521_WEEKDAY];
    clk->tm_mday = nibbles(TC8521_DAY);
    clk->tm_mon = (nibbles(TC8521_MONTH) - 1);
    clk->tm_year = (nibbles(TC8521_YEAR) + 1980);
}


/* This is called every second through the NVR/RTC hook. */
static void
tc8521_tick(UNUSED(nvr_t *nvr))
{
    DEBUG("TC8521: ping\n");
}


static void
tc8521_start(nvr_t *nvr)
{
    intclk_t clk;

    /* Initialize the internal and chip times. */
    if (config.time_sync != TIME_SYNC_DISABLED) {
	/* Use the internal clock's time. */
	nvr_time_get(nvr, &clk);
	tc8521_time_set(nvr->regs, &clk);
    } else {
	/* Set the internal clock from the chip time. */
	tc8521_time_get(nvr->regs, &clk);
	nvr_time_set(&clk, nvr);
    }

#if 0
    /* Start the RTC - BIOS will do this. */
    nvr->regs[TC8521_PAGE] |= 0x80;
#endif
}


/* Write to one of the chip registers. */
static void
tc8521_write(uint16_t addr, uint8_t val, priv_t priv)
{
    nvr_t *nvr = (nvr_t *)priv;
    uint8_t page;

    /* Get to the correct register page. */
    addr &= 0x0f;
    page = nvr->regs[0x0d] & 0x03;
    if (addr < 0x0d)
	addr += (16 * page);

    if (addr >= 0x10 && nvr->regs[addr] != val)
	nvr_dosave = 1;

    /* Store the new value. */
    nvr->regs[addr] = val;
}


/* Read from one of the chip registers. */
static uint8_t
tc8521_read(uint16_t addr, priv_t priv)
{
    nvr_t *nvr = (nvr_t *)priv;
    uint8_t page;

    /* Get to the correct register page. */
    addr &= 0x0f;
    page = nvr->regs[0x0d] & 0x03;
    if (addr < 0x0d)
	addr += (16 * page);

    /* Grab and return the desired value. */
    return(nvr->regs[addr]);
}


/* Reset the 8521 to a default state. */
static void
tc8521_reset(nvr_t *nvr)
{
    /* Clear the NVRAM. */
    memset(nvr->regs, 0xff, nvr->size);

    /* Reset the RTC registers. */
    memset(nvr->regs, 0x00, 16);
    nvr->regs[TC8521_WEEKDAY] = 0x01;
    nvr->regs[TC8521_DAY1] = 0x01;
    nvr->regs[TC8521_MONTH1] = 0x01;
}


static void
tc8521_init(nvr_t *nvr, int size)
{
    /* This is machine specific. */
    nvr->size = size;
    nvr->irq = -1;

    /* Set up any local handlers here. */
    nvr->reset = tc8521_reset;
    nvr->start = tc8521_start;
    nvr->tick = tc8521_tick;

    /* Initialize the actual NVR. */
    nvr_init(nvr);

    io_sethandler(0x02c0, 16,
		  tc8521_read,NULL,NULL, tc8521_write,NULL,NULL, nvr);
}


/* Given an EMS page ID, return its physical address in RAM. */
static uint32_t
ems_execaddr(t1000_t *dev, UNUSED(int pg), uint16_t val)
{
    if (! (val & 0x80))
	return(0);	/* Bit 7 reset => not mapped */

    if (! dev->ems_pages)
	return(0);	/* No EMS available: all used by 
			 * HardRAM or conventional RAM */
    val &= 0x7f;

    DBGLOG(1, "Select EMS page: %i of %i\n", val, dev->ems_pages);
    if (val < dev->ems_pages) {
	/* EMS is any memory above 512K,
	   with ems_base giving the start address */
	return((512 * 1024) + (dev->ems_base * 0x10000) + (0x4000 * val));
    }

    return(0);
}


static uint8_t
ems_in(uint16_t addr, priv_t priv)
{
    t1000_t *dev = (t1000_t *)priv;
    uint8_t ret;

    ret = dev->ems_reg[(addr >> 14) & 3];
    DBGLOG(1, "ems_in(%04x)=%02x\n", addr, ret);

    return(ret);
}


static void
ems_out(uint16_t addr, uint8_t val, priv_t priv)
{
    t1000_t *dev = (t1000_t *)priv;
    int pg = (addr >> 14) & 3;

    DBGLOG(1, "ems_out(%04x, %02x) pg=%d\n", addr, val, pg);

    dev->ems_reg[pg] = val;
    dev->page_exec[pg] = ems_execaddr(dev, pg, val);
    if (dev->page_exec[pg]) {
	/* Page present */
	mem_map_enable(&dev->mapping[pg]);
	mem_map_set_exec(&dev->mapping[pg], ram + dev->page_exec[pg]);
    } else {
	mem_map_disable(&dev->mapping[pg]);
    }
}


/* Hardram size is in 64K units. */
static void
ems_set_hardram(t1000_t *dev, uint8_t val)
{
    int n;

    val &= 0x1f;	/* mask off pageframe address */
    if (val && mem_size > 512)
	dev->ems_base = val;
      else
	dev->ems_base = 0;

    DEBUG("EMS base set to %02x\n", val);
    dev ->ems_pages = ((mem_size - 512) / 16) - 4 * dev->ems_base;
    if (dev->ems_pages < 0)
	dev->ems_pages = 0;

    /* Recalculate EMS mappings */
    for (n = 0; n < 4; n++)
	ems_out(n << 14, dev->ems_reg[n], dev);
}


static void
ems_set_640k(t1000_t *dev, uint8_t val)
{
    if (val && mem_size >= 640) {
	mem_map_set_addr(&ram_low_mapping, 0, 640 * 1024);
	dev->is_640k = 1;
    } else {
	mem_map_set_addr(&ram_low_mapping, 0, 512 * 1024);
	dev->is_640k = 0;
    }
}


static void
ems_set_port(t1000_t *dev, uint8_t val)
{
    int n;

    DBGLOG(1, "ems_set_port(%d)", val & 0x0f);
    if (dev->ems_port) {
	for (n = 0; n <= 0xc000; n += 0x4000) {
		io_removehandler(dev->ems_port + n, 1, 
				 ems_in,NULL,NULL, ems_out,NULL,NULL, dev);
	}

	dev->ems_port = 0;
    }

    val &= 0x0f;
    dev->ems_port_index = val;
    if (val == 7) {
	/* No EMS */
	dev->ems_port = 0;
    } else {
	dev->ems_port = 0x208 | (val << 4);
	for (n = 0; n <= 0xc000; n += 0x4000) {
		io_sethandler(dev->ems_port + n, 1, 
			      ems_in,NULL,NULL, ems_out,NULL,NULL, dev);
	}

	dev->ems_port = 0;
    }

    DBGLOG(1, " -> %04x\n", dev->ems_port);
}


static int
addr_to_page(uint32_t addr)
{
    return((addr - 0xd0000) / 0x4000);
}


/* Read RAM in the EMS page frame. */
static uint8_t
ems_read(uint32_t addr, priv_t priv)
{
    t1000_t *dev = (t1000_t *)priv;
    int pg = addr_to_page(addr);

    if (pg < 0) return(0xff);
    addr = dev->page_exec[pg] + (addr & 0x3fff);

    return(ram[addr]);	
}


static uint16_t
ems_readw(uint32_t addr, priv_t priv)
{
    t1000_t *dev = (t1000_t *)priv;
    int pg = addr_to_page(addr);
    uint16_t ret;

    if (pg < 0) return(0xff);

    DBGLOG(1, "ems_read_ramw addr=%05x ", addr);
    addr = dev->page_exec[pg] + (addr & 0x3fff);
    ret = *(uint16_t *)&ram[addr];

    DBGLOG(1, "-> %06x val=%04x\n", addr, ret);

    return(ret);
}


static uint32_t
ems_readl(uint32_t addr, priv_t priv)
{
    t1000_t *dev = (t1000_t *)priv;
    int pg = addr_to_page(addr);
    uint32_t ret;

    if (pg < 0) return(0xff);
    addr = dev->page_exec[pg] + (addr & 0x3fff);
    ret = *(uint32_t *)&ram[addr];

    return(ret);
}


/* Write RAM in the EMS page frame. */
static void
ems_write(uint32_t addr, uint8_t val, priv_t priv)
{
    t1000_t *dev = (t1000_t *)priv;
    int pg = addr_to_page(addr);

    if (pg < 0) return;

    addr = dev->page_exec[pg] + (addr & 0x3fff);
    if (ram[addr] != val)
	nvr_dosave = 1;

    ram[addr] = val;
}


static void
ems_writew(uint32_t addr, uint16_t val, priv_t priv)
{
    t1000_t *dev = (t1000_t *)priv;
    int pg = addr_to_page(addr);

    if (pg < 0) return;

    DBGLOG(1, "ems_write_ramw addr=%05x ", addr);
    addr = dev->page_exec[pg] + (addr & 0x3fff);
    DBGLOG(1, "-> %06x val=%04x\n", addr, val);

    if (*(uint16_t *)&ram[addr] != val)
	nvr_dosave = 1;	

    *(uint16_t *)&ram[addr] = val;
}


static void
ems_writel(uint32_t addr, uint32_t val, priv_t priv)
{
    t1000_t *dev = (t1000_t *)priv;
    int pg = addr_to_page(addr);

    if (pg < 0) return;

    addr = dev->page_exec[pg] + (addr & 0x3fff);
    if (*(uint32_t *)&ram[addr] != val)
	nvr_dosave = 1;	

    *(uint32_t *)&ram[addr] = val;
}


/* Load contents of "CONFIG.SYS" device from file. */
static void
cfgsys_load(t1000_t *dev)
{
    char temp[128];
    FILE *fp;

    /* Set up the file's name. */
    sprintf(temp, "%s_cfgsys.nvr", machine_get_internal_name());
    mbstowcs(dev->cfgsys_fn, temp, sizeof_w(dev->cfgsys_fn));

    /* Now attempt to load the file. */
    memset(dev->cfgsys, 0x1a, dev->cfgsys_len);
    fp = plat_fopen(nvr_path(dev->cfgsys_fn), L"rb");
    if (fp != NULL) {
	INFO("NVR: loaded CONFIG.SYS from '%ls'\n", dev->cfgsys_fn);
	(void)fread(dev->cfgsys, dev->cfgsys_len, 1, fp);
	fclose(fp);
    } else
	INFO("NVR: initialized CONFIG.SYS for '%ls'\n", dev->cfgsys_fn);
}


/* Write the contents of "CONFIG.SYS" to file. */
static void
cfgsys_save(t1000_t *dev)
{
    FILE *fp;

    /* Avoids writing empty files. */
    if (dev->cfgsys_len < 160) return;

    fp = plat_fopen(nvr_path(dev->cfgsys_fn), L"wb");
    if (fp != NULL) {
	INFO("NVR: saved CONFIG.SYS to '%ls'\n", dev->cfgsys_fn);
	(void)fwrite(dev->cfgsys, dev->cfgsys_len, 1, fp);
	fclose(fp);
    }
}


#if 0	/*NOT_USED*/
/* All RAM beyond 512K is non-volatile */
static void
emsboard_load(t1000_t *dev)
{
    FILE *fp;

    if (mem_size > 512) {
	fp = plat_fopen(nvr_path(L"t1000_ems.nvr"), L"rb");
	if (fp != NULL) {
		fread(&ram[512 * 1024], 1024, (mem_size - 512), fp);
		fclose(fp);
	}
    }
}


static void
emsboard_save(t1000_t *dev)
{
    FILE *fp;

    if (mem_size > 512) {
	fp = plat_fopen(nvr_path(L"t1000_ems.nvr"), L"wb");
	if (fp != NULL) {
		fwrite(&ram[512 * 1024], 1024, (mem_size - 512), fp);
		fclose(fp);
	}
    }
}
#endif


static void
turbo_set(t1000_t *dev, uint8_t value)
{
    if (value == dev->turbo) return;

    dev->turbo = value;

    pc_set_speed(dev->turbo);
}


static void
ctl_write(uint16_t addr, uint8_t val, priv_t priv)
{
    t1000_t *dev = (t1000_t *)priv;

    dev->sys_ctl[addr & 0x0f] = val;
    switch (addr & 0x0f) {
	case 4:		/* Video control */
		if (dev->sys_ctl[3] == 0x5a) {
			t1000_video_options_set((val & 0x20) ? 1 : 0);
			t1000_display_set((val & 0x40) ? 0 : 1);
			if (dev->is_t1200)
				turbo_set(dev, (val & 0x80) ? 1 : 0);
		}
		break;

	/*
	 * It looks as if the T1200, like the T3100, can disable
	 * its built-in video chipset if it detects the presence
	 * of another video card.
	 */
	case 6:
		if (dev->is_t1200)
			t1000_video_enable(val & 0x01 ? 0 : 1);
		break;		

	case 0x0f:	/* EMS control */
		switch (dev->sys_ctl[0x0e]) {
			case 0x50:
				ems_set_port(dev, val);
				break;

			case 0x51:
				ems_set_hardram(dev, val);
				break;

			case 0x52:
				ems_set_640k(dev, val);
				break;
		}
		break;
    }
}


static uint8_t
ctl_read(uint16_t addr, priv_t priv)
{
    t1000_t *dev = (t1000_t *)priv;
    uint8_t ret = 0xff;

    switch (addr & 0x0f) {
	case 1:
		ret = dev->syskeys;
		break;

	case 0x0f:	/* Detect EMS board */
		switch (dev->sys_ctl[0x0e]) {
			case 0x50:
				if (mem_size > 512)
					ret = (0x90 | dev->ems_port_index);
				break;

			case 0x51:
				/* 0x60 is the page frame address:
				   (0xd000 - 0xc400) / 0x20 */
				ret = (dev->ems_base | 0x60);
				break;

			case 0x52:
				ret = (dev->is_640k ? 0x80 : 0);
				break;
		}
		break;

	default:
		ret = (dev->sys_ctl[addr & 0x0f]); 
    }

    return(ret);
}


/*
 * Ports 0xc0 to 0xc3 appear to have two purposes:
 *
 * > Access to the 160 bytes of non-volatile RAM containing CONFIG.SYS
 * > Reading the floppy changeline. I don't know why the Toshiba doesn't
 *   use the normal port 0x3f7 for this, but it doesn't.
 *
 */
static uint8_t
nvram_read(uint16_t addr, priv_t priv)
{
    t1000_t *dev = (t1000_t *)priv;
    uint8_t tmp = 0xff;

    switch (addr) {
	case 0x00c2:	/* read next byte from NVRAM */
		if (dev->nvr_addr >= 0 && dev->nvr_addr < 160) {
			tmp = dev->cfgsys[dev->nvr_addr];
			dev->nvr_addr++;
		}
		break;

	case 0x00c3:	/* read floppy changeline and NVRAM ready state */
		tmp = fdc_in(0x03f7, dev->fdc);
		tmp = (tmp & 0x80) >> 3;	/* Bit 4 is changeline */
		tmp |= (dev->nvr_active & 0xc0);/* Bits 6,7 are r/w mode */
		tmp |= 0x2e;			/* Bits 5,3,2,1 always 1 */
		tmp |= (dev->nvr_active & 0x40) >> 6;	/* Ready state */
		break;
    }

    return(tmp);
}


static uint8_t
nvram_mem_read(uint32_t addr, priv_t priv)
{
    t1000_t *dev = (t1000_t *)priv;

    return(dev->cfgsys[addr & 0x07ff]);
}


static void
nvram_write(uint16_t addr, uint8_t val, priv_t priv)
{
    t1000_t *dev = (t1000_t *)priv;

    /*
     * On the real T1000, port 0xc1 is only usable as the high byte
     * of a 16-bit write to port 0xc0, with 0x5a in the low byte.
     */
    switch (addr) {
	case 0x00c0:
		dev->nvr_c0 = val;
		break;

	case 0x00c1:	/* write next byte to NVRAM */
		if (dev->nvr_addr > 0 && dev->nvr_addr <= 160) {
			if (dev->cfgsys[dev->nvr_addr-1] != val) 
				nvr_dosave = 1;
			dev->cfgsys[dev->nvr_addr-1] = val;
			dev->nvr_addr++;
		} else {
			/*
			 * We ignore the first byte written after a
			 * RESET pulse, as that seems to indicate the
			 * type of operation (00=READ, FF=WRITE). So,
			 * we "ignore" byte 0 here.
			 */
			if (val == 0xff)
				dev->nvr_addr++;
		}
		break;

	case 0x00c2:
		break;

	case 0x00c3:
		/*
		 * At start of NVRAM read / write, 0x80 is written to
		 * port 0xC3. This seems to reset the NVRAM address.
		 */
		dev->nvr_active = val;
		if (val == 0x80)
			dev->nvr_addr = 0;
		break;
    }
}


static void
nvram_mem_write(uint32_t addr, uint8_t val, priv_t priv)
{
    t1000_t *dev = (t1000_t *)priv;

    if (dev->cfgsys[addr & 0x07ff] != val) {
	nvr_dosave = 1;

	dev->cfgsys[addr & 0x07ff] = val;
    }
}


/* Port 0xc8 controls the ROM drive. */
static uint8_t
ctl_rom_read(UNUSED(uint16_t addr), priv_t priv)
{
    t1000_t *dev = (t1000_t *)priv;

    return(dev->rom_ctl);
}


static void
ctl_rom_write(UNUSED(uint16_t addr), uint8_t val, priv_t priv)
{
    t1000_t *dev = (t1000_t *)priv;

    dev->rom_ctl = val;
    if (dev->romdrive && (val & 0x80)) {
	/* Enable */
	dev->rom_offset = ((val & 0x7f) * 0x10000) % T1000_ROMDOS_SIZE;
	mem_map_set_addr(&dev->rom_mapping, 0xa0000, 0x10000);
	mem_map_set_exec(&dev->rom_mapping, dev->romdrive + dev->rom_offset);
	mem_map_enable(&dev->rom_mapping);	
    } else {
	mem_map_disable(&dev->rom_mapping);	
    }
}


/* Read the ROM drive */
static uint8_t
read_rom(uint32_t addr, priv_t priv)
{
    t1000_t *dev = (t1000_t *)priv;

    if (! dev->romdrive)
	return(0xff);

    return(dev->romdrive[dev->rom_offset + (addr & 0xffff)]);
}


static uint16_t
read_romw(uint32_t addr, priv_t priv)
{
    t1000_t *dev = (t1000_t *)priv;

    if (! dev->romdrive)
	return(0xffff);

    return(*(uint16_t *)(&dev->romdrive[dev->rom_offset + (addr & 0xffff)]));
}


static uint32_t
read_roml(uint32_t addr, priv_t priv)
{
    t1000_t *dev = (t1000_t *)priv;

    if (! dev->romdrive)
	return(0xffffffff);

    return(*(uint32_t *)(&dev->romdrive[dev->rom_offset + (addr & 0xffff)]));
}


static void
t1000_close(priv_t priv)
{
    t1000_t *dev = (t1000_t *)priv;

    if (dev->romdrive != NULL)
	free(dev->romdrive);

    cfgsys_save(dev);

    free(dev);
}


static priv_t
t1000_init(const device_t *info, UNUSED(void *arg))
{
    t1000_t *dev;
    FILE *fp;
    int pg;

    dev = (t1000_t *)mem_alloc(sizeof(t1000_t));
    memset(dev, 0x00, sizeof(t1000_t));
    dev->is_t1200 = info->local;

    dev->turbo = 0xff;
    dev->ems_port_index = 7;	/* EMS disabled */

    /* Add machine device to the system. */
    device_add_ex(info, (priv_t)dev);

    if (dev->is_t1200) {
	/* Non-volatile RAM for CONFIG.SYS */
	dev->cfgsys_len = 2048;
	dev->cfgsys = (uint8_t *)mem_alloc(dev->cfgsys_len);
	mem_map_add(&dev->nvr_mapping, 0x0f0000, dev->cfgsys_len,
		    nvram_mem_read,NULL,NULL,
		    nvram_mem_write,NULL,NULL, NULL, 0, dev);
    } else {
	/*
	 * The ROM drive is optional.
	 *
	 * If the file is missing, continue to boot; the BIOS will
	 * complain 'No ROM drive' but boot normally from floppy.
	 */
	dev->rom_dos = machine_get_config_int("rom_dos");
	if (dev->rom_dos) {
		fp = rom_fopen(T1000_ROMDOS_PATH, L"rb");
		if (fp != NULL) {
			dev->romdrive = (uint8_t *)mem_alloc(T1000_ROMDOS_SIZE);
			if (dev->romdrive) {
				memset(dev->romdrive, 0xff, T1000_ROMDOS_SIZE);
				fread(dev->romdrive, T1000_ROMDOS_SIZE, 1, fp);
			}
			fclose(fp);

			mem_map_add(&dev->rom_mapping, 0xa0000, 0x10000,
				    read_rom,read_romw,read_roml, NULL,NULL,NULL,
				    NULL, MEM_MAPPING_INTERNAL, dev);
			mem_map_disable(&dev->rom_mapping);
		}

		io_sethandler(0xc8, 1,
			ctl_rom_read,NULL,NULL, ctl_rom_write,NULL,NULL, dev);
	}

	/* Non-volatile RAM for CONFIG.SYS */
	dev->cfgsys_len = 160;
	dev->cfgsys = (uint8_t *)mem_alloc(dev->cfgsys_len);
	io_sethandler(0xc0, 4,
		      nvram_read,NULL,NULL, nvram_write,NULL,NULL, dev);
    }

    /* Load or initialize the NVRAM "config.sys" file. */
    cfgsys_load(dev);

    /* Map the EMS page frame. */
    for (pg = 0; pg < 4; pg++) {
	mem_map_add(&dev->mapping[pg], 0xd0000 + (0x4000 * pg), 16384,
		    ems_read,ems_readw,ems_readl,
		    ems_write,ems_writew,ems_writel,
		    NULL, MEM_MAPPING_EXTERNAL, dev);

	/* Start them all off disabled */
	mem_map_disable(&dev->mapping[pg]);
    }

    /* System control functions, and add-on memory board */
    io_sethandler(0xe0, 16,
		  ctl_read,NULL,NULL, ctl_write,NULL,NULL, dev);

    machine_common_init();

    /* Set up our DRAM refresh timer. */
    pit_set_out_func(&pit, 1, m_xt_refresh_timer);

    nmi_init();

    device_add(&keyboard_xt_device);

    tc8521_init(&dev->nvr, machine_get_nvrsize());

    if (dev->is_t1200) {
	dev->fdc = (fdc_t *)device_add(&fdc_toshiba_device);

	if (config.video_card == VID_INTERNAL) {
		/* Load the T1200 CGA Font ROM. */
		video_load_font(T1200_FONT_PATH, FONT_CGA_THICK);

		device_add(&t1200_video_device);
	}

	if (config.hdc_type == HDC_INTERNAL)
		(void)device_add(&xta_t1200_device);
    } else {
	dev->fdc = (fdc_t *)device_add(&fdc_xt_device);

	if (config.video_card == VID_INTERNAL) {
		/* Load the T1000 CGA Font ROM. */
		video_load_font(T1000_FONT_PATH, FONT_CGA_THICK);

		device_add(&t1000_video_device);
	}
    }

    return((priv_t)dev);
}


static const device_config_t t1000_config[] = {
    {
	"rom_dos", "ROM DOS", CONFIG_SELECTION, "", 0,
	{
		{
			"Disabled", 0
		},
		{
			"Enabled", 1
		},
		{
			NULL
		}
	}
    },
    {
	"display_language", "Language", CONFIG_SELECTION, "", 0,
	{
		{ "US English", 0	},
		{ "Danish", 1		},
    		{ NULL			}
	}
    },
    {
	NULL
    }
};

static const device_config_t t1200_config[] = {
    {
	"display_language", "Language", CONFIG_SELECTION, "", 0,
	{
		{ "US English", 0	},
		{ "Danish", 1		},
    		{ NULL			}
	}
    },
    {
	NULL
    }
};


static const machine_t t1000_info = {
    MACHINE_ISA | MACHINE_VIDEO,
    MACHINE_VIDEO,
    512, 768, 128, 64, -1,
    {{"Intel",cpus_8088},{"NEC",cpus_nec}}
};

const device_t m_tosh_1000 = {
    "Toshiba T1000",
    MACHINE_ISA,
    0,
    L"toshiba/t1000",
    t1000_init, t1000_close, NULL,
    NULL, NULL, NULL,
    &t1000_info,
    t1000_config
};


static const machine_t t1200_info = {
    MACHINE_ISA | MACHINE_VIDEO | MACHINE_HDC,
    MACHINE_VIDEO,
    1024, 2048,1024, 64, -1,
    {{"Intel",cpus_8086},{"NEC",cpus_nec}}
};

const device_t m_tosh_1200 = {
    "Toshiba T1200",
    MACHINE_ISA,
    1,
    L"toshiba/t1200",
    t1000_init, t1000_close, NULL,
    NULL, NULL, NULL,
    &t1200_info,
    t1200_config
};


void
t1000_syskey(UNUSED(uint8_t andmask), UNUSED(uint8_t ormask), UNUSED(uint8_t xormask))
{
#if 0
    t1000.syskeys &= ~andmask;
    t1000.syskeys |= ormask;
    t1000.syskeys ^= xormask;
#endif
}
