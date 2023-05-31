/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of SCSI fixed disks.
 *
 * **NOTE**	Some weirdness is still going on with the sense_read code;
 *		until this is fixed, we return the actual device properties,
 *		and keep the sense data unmodifyable.
 *
 * Version:	@(#)scsi_disk.c	1.0.24	2019/05/17
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2017-2019 Fred N. van Kempen.
 *		Copyright 2016-2018 Miran Grca.
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
#include <stdarg.h>
#include <wchar.h>
#define HAVE_STDARG_H
#define dbglog scsi_disk_log
#include <86box/86box.h>
#include <86box/version.h>
#include <86box/timer.h>
#include <86box/device.h>
#include <86box/nvr.h>
#include <86box/ui.h>
#include <86box/plat.h>
#include <86box/hdd.h>
#include <86box/hdc.h>
#include <86box/hdc_ide.h>
#include <86box/scsi_device.h>
#include <86box/scsi_disk.h>


/* Bits of 'status' */
#define ERR_STAT		0x01
#define DRQ_STAT		0x08 /* Data request */
#define DSC_STAT                0x10
#define SERVICE_STAT            0x10
#define READY_STAT		0x40
#define BUSY_STAT		0x80

/* Bits of 'error' */
#define ABRT_ERR		0x04 /* Command aborted */
#define MCR_ERR			0x08 /* Media change request */

#define MAX_BLOCKS_AT_ONCE	340


/* Table of all SCSI commands and their flags, needed for the new disc change / not ready handler. */
static const uint8_t command_flags[0x100] = {
    IMPLEMENTED | CHECK_READY | NONDATA,			/* 0x00 */
    IMPLEMENTED | ALLOW_UA | NONDATA | SCSI_ONLY,		/* 0x01 */
    0,
    IMPLEMENTED | ALLOW_UA,					/* 0x03 */
    IMPLEMENTED | CHECK_READY | ALLOW_UA | NONDATA | SCSI_ONLY,	/* 0x04 */
    0, 0, 0,
    IMPLEMENTED | CHECK_READY,					/* 0x08 */
    0,
    IMPLEMENTED | CHECK_READY,					/* 0x0A */
    IMPLEMENTED | CHECK_READY | NONDATA,			/* 0x0B */
    0, 0, 0, 0, 0, 0,
    IMPLEMENTED | ALLOW_UA,					/* 0x12 */
    IMPLEMENTED | CHECK_READY | NONDATA | SCSI_ONLY,		/* 0x13 */
    0,
    IMPLEMENTED,						/* 0x15 */
    IMPLEMENTED | SCSI_ONLY,					/* 0x16 */
    IMPLEMENTED | SCSI_ONLY,					/* 0x17 */
    0, 0,
    IMPLEMENTED,						/* 0x1A */
    0, 0,
    IMPLEMENTED,						/* 0x1D */
    IMPLEMENTED | CHECK_READY,					/* 0x1E */
    0, 0, 0, 0, 0, 0,
    IMPLEMENTED | CHECK_READY,					/* 0x25 */
    0, 0,
    IMPLEMENTED | CHECK_READY,					/* 0x28 */
    0,
    IMPLEMENTED | CHECK_READY,					/* 0x2A */
    IMPLEMENTED | CHECK_READY | NONDATA,			/* 0x2B */
    0, 0,
    IMPLEMENTED | CHECK_READY,					/* 0x2E */
    IMPLEMENTED | CHECK_READY | NONDATA | SCSI_ONLY,		/* 0x2F */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,
    IMPLEMENTED | CHECK_READY,					/* 0x41 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    IMPLEMENTED,						/* 0x55 */
    0, 0, 0, 0,
    IMPLEMENTED,						/* 0x5A */
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    IMPLEMENTED | CHECK_READY,					/* 0xA8 */
    0,
    IMPLEMENTED | CHECK_READY,					/* 0xAA */
    0, 0, 0,
    IMPLEMENTED | CHECK_READY,					/* 0xAE */
    IMPLEMENTED | CHECK_READY | NONDATA | SCSI_ONLY,		/* 0xAF */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    IMPLEMENTED,						/* 0xBD */
    0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};


static uint64_t mode_sense_page_flags = (GPMODEP_FORMAT_DEVICE_PAGE |
					 GPMODEP_RIGID_DISK_PAGE |
					 GPMODEP_UNK_VENDOR_PAGE |
					 GPMODEP_ALL_PAGES);

/* This should be done in a better way but for time being, it's been done this way so it's not as huge and more readable. */
static const mode_sense_pages_t mode_sense_pages_default =
{	{	[GPMODE_FORMAT_DEVICE_PAGE] = {	GPMODE_FORMAT_DEVICE_PAGE, 0x16, 0,    1, 0,  1, 0, 1, 0, 1, 1, 0, 2, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0 },
#if 1
		[GPMODE_RIGID_DISK_PAGE   ] = {	GPMODE_RIGID_DISK_PAGE, 0x16, 0, 0x10, 0, 64, 0, 0, 0, 0, 0, 0, 0, 200, 0xff, 0xff, 0xff, 0, 0, 0, 0x15, 0x18, 0, 0 },
#else
		[GPMODE_RIGID_DISK_PAGE   ] = {	GPMODE_RIGID_DISK_PAGE, 0x16, 0, 0x10, 0, 64, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x10, 0, 0, 0 },
#endif
		[GPMODE_UNK_VENDOR_PAGE   ] = {	0xB0, 0x16, '8', '6', 'B', 'o', 'x', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ' }
}	};

static const mode_sense_pages_t mode_sense_pages_changeable =
#if 1
{	{	[GPMODE_FORMAT_DEVICE_PAGE] = {	GPMODE_FORMAT_DEVICE_PAGE, 0x16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		[GPMODE_RIGID_DISK_PAGE   ] = {	GPMODE_RIGID_DISK_PAGE, 0x16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
#else
{	{	[GPMODE_FORMAT_DEVICE_PAGE] = {	GPMODE_FORMAT_DEVICE_PAGE, 0x16, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0, 0, 0 },
		[GPMODE_RIGID_DISK_PAGE   ] = {	GPMODE_RIGID_DISK_PAGE, 0x16, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0, 0 },
#endif
		[GPMODE_UNK_VENDOR_PAGE   ] = {	0xB0, 0x16, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }
}	};


static void	do_callback(void *p);


#ifdef ENABLE_SCSI_DISK_LOG
int scsi_disk_do_log = ENABLE_SCSI_DISK_LOG;
#endif


#ifdef _LOGGING
static void
scsi_disk_log(int level, const char *fmt, ...)
{
# ifdef ENABLE_SCSI_DISK_LOG
    va_list ap;

    if (scsi_disk_do_log >= level) {
	va_start(ap, fmt);
	pclog_ex(fmt, ap);
	va_end(ap);
    }
# endif
}
#endif


/* Translates ATAPI status (ERR_STAT flag) to SCSI status. */
static int
err_stat_to_scsi(void *p)
{
    scsi_disk_t *dev = (scsi_disk_t *)p;

    if (dev->status & ERR_STAT)
	return SCSI_STATUS_CHECK_CONDITION;

    return SCSI_STATUS_OK;
}


static void
mode_sense_load(scsi_disk_t *dev)
{
    wchar_t temp[512];
    FILE *fp;

#if 0
    // FIXME:
    // This seems to make NextStep, OpenStep and maybe other
    // systems not like the SCSI Disk device we present to
    // them.  Being investigated.
    /* Start out with a default set of pages. */
    memcpy(&dev->ms_pages_saved,
	   &mode_sense_pages_default, sizeof(mode_sense_pages_t));
#endif

    /* Create the pathname for this data. */
    swprintf(temp, sizeof_w(temp),
	     L"scsi_disk_%02i_mode_sense.bin", dev->id);

    fp = plat_fopen(nvr_path(temp), L"rb");
    if (fp != NULL) {
	(void)fread(&dev->ms_pages_saved.pages[0x30], 1, 0x18, fp);
	(void)fclose(fp);
    }
}


static void
mode_sense_save(scsi_disk_t *dev)
{
    wchar_t temp[512];
    FILE *fp;

    /* Create the pathname for this data. */
    swprintf(temp, sizeof_w(temp),
	     L"scsi_disk_%02i_mode_sense.bin", dev->id);

    fp = plat_fopen(nvr_path(temp), L"wb");
    if (fp != NULL) {
	(void)fwrite(&dev->ms_pages_saved.pages[0x30], 1, 0x18, fp);
	(void)fclose(fp);
    }
}


static int
read_capcity(void *p, uint8_t *cdb, uint8_t *buffer, uint32_t *len)
{
    scsi_disk_t *dev = (scsi_disk_t *)p;
    int size;

    size = hdd_image_get_last_sector(dev->id);

    memset(buffer, 0, 8);
    buffer[0] = (size >> 24) & 0xff;
    buffer[1] = (size >> 16) & 0xff;
    buffer[2] = (size >> 8) & 0xff;
    buffer[3] = size & 0xff;
    buffer[6] = 2;				/* 512 = 0x0200 */

    *len = 8;

    return 1;
}


/*SCSI Mode Sense 6/10*/
static uint8_t
mode_sense_read(scsi_disk_t *dev, uint8_t page_control, uint8_t page, uint8_t pos)
{
    uint8_t ret = 0x00;

INFO("SCSI disk%i sense_read(%i, %i, %i)\n", dev->id, page_control, page, pos);

#if 1
    if (page_control == 1)
	return mode_sense_pages_changeable.pages[page][pos];

    if (page == GPMODE_RIGID_DISK_PAGE) switch (page_control) {
	/* Rigid disk geometry page. */
	case 0:
	case 2:
	case 3:
#else
    switch (page_control) {
	case 0:
	case 3:
#endif
#if 1
 		switch(pos) {
 			case 0:
 			case 1:
 			default:
 				return mode_sense_pages_default.pages[page][pos];

 			case 2:
 			case 6:
 			case 9:
INFO("SCSI: sense returning %i cyls (%02x)\n", dev->drv->tracks, (dev->drv->tracks >> 16) & 0xff);
 				return (dev->drv->tracks >> 16) & 0xff;

 			case 3:
 			case 7:
 			case 10:
INFO("SCSI: sense returning %i cyls (%02x)\n", dev->drv->tracks, (dev->drv->tracks >> 8) & 0xff);
 				return (dev->drv->tracks >> 8) & 0xff;

 			case 4:
 			case 8:
 			case 11:
INFO("SCSI: sense returning %i cyls (%02x)\n", dev->drv->tracks, dev->drv->tracks & 0xff);
 				return dev->drv->tracks & 0xff;

 			case 5:
INFO("SCSI: sense returning %i heads\n", dev->drv->hpc);
 				return dev->drv->hpc & 0xff;
 		}
#else
		ret = dev->ms_pages_saved.pages[page][pos];
#endif
		break;

#if 1
    } else if (page == GPMODE_FORMAT_DEVICE_PAGE) switch (page_control) {
	/* Format device page. */
	case 0:
	case 2:
	case 3:
		switch(pos) {
			case 0:
			case 1:
			default:
				return mode_sense_pages_default.pages[page][pos];

			case 10:
INFO("SCSI: sense returning %i spt (%02x)\n", (dev->drv->spt + 1), ((dev->drv->spt + 1) >> 8) & 0xff);
				return ((dev->drv->spt + 1) >> 8) & 0xff;

			case 11:
INFO("SCSI: sense returning %i spt (%02x)\n", (dev->drv->spt + 1), (dev->drv->spt + 1) & 0xff);
				return (dev->drv->spt + 1) & 0xff;
		}
		break;
   } else switch (page_control) {
	case 0:
	case 3:
		return dev->ms_pages_saved.pages[page][pos];
#else
	case 1:
		ret = mode_sense_pages_changeable.pages[page][pos];
		break;
#endif

	case 2:
		ret = mode_sense_pages_default.pages[page][pos];
		break;
    }

    return ret;
}


static uint32_t
mode_sense(scsi_disk_t *dev, uint8_t *buf, uint32_t pos, uint8_t page, uint8_t block_descriptor_len)
{
    uint8_t msplen, page_control = (page >> 6) & 3;
    int i, j, size;

    page &= 0x3f;

    size = hdd_image_get_last_sector(dev->id);

    if (block_descriptor_len) {
	buf[pos++] = 1;		/* Density code. */
	buf[pos++] = (size >> 16) & 0xff;	/* Number of blocks (0 = all). */
	buf[pos++] = (size >> 8) & 0xff;
	buf[pos++] = size & 0xff;
	buf[pos++] = 0;		/* Reserved. */
	buf[pos++] = 0;		/* Block length (0x200 = 512 bytes). */
	buf[pos++] = 2;
	buf[pos++] = 0;
    }

    for (i = 0; i < 0x40; i++) {
        if ((page == GPMODE_ALL_PAGES) || (page == i)) {
		if (mode_sense_page_flags & (1LL << (uint64_t) page)) {
			buf[pos++] = mode_sense_read(dev, page_control, i, 0);
			msplen = mode_sense_read(dev, page_control, i, 1);
			buf[pos++] = msplen;
			DEBUG("SCSI HDD %i: MODE SENSE: Page [%02X] length %i\n", dev->id, i, msplen);
			for (j = 0; j < msplen; j++)
				buf[pos++] = mode_sense_read(dev, page_control, i, 2 + j);
		}
	}
    }

    return pos;
}


static void
command_common(scsi_disk_t *dev)
{
    dev->status = BUSY_STAT;
    dev->phase = 1;

    if (dev->packet_status == PHASE_COMPLETE) {
	do_callback(dev);
	dev->callback = 0LL;
    } else
	dev->callback = -1LL;	/* Speed depends on SCSI controller */
}


static void
command_complete(scsi_disk_t *dev)
{
    dev->packet_status = PHASE_COMPLETE;

    command_common(dev);
}


static void
command_read_dma(scsi_disk_t *dev)
{
    dev->packet_status = PHASE_DATA_IN_DMA;

    command_common(dev);
}


static void
command_write_dma(scsi_disk_t *dev)
{
    dev->packet_status = PHASE_DATA_OUT_DMA;

    command_common(dev);
}


static void
data_command_finish(scsi_disk_t *dev, int len, int block_len, int alloc_len, int direction)
{
    DEBUG("SCSI HD %i: Finishing command (%02X): %i, %i, %i, %i, %i\n",
	  dev->id, dev->current_cdb[0], len, block_len, alloc_len, direction, dev->request_length);

    if (alloc_len >= 0) {
	if (alloc_len < len)
		len = alloc_len;
    }

    if (len == 0)
	command_complete(dev);
    else {
	if (direction == 0)
		command_read_dma(dev);
	else
		command_write_dma(dev);
    }
}


static void
sense_clear(scsi_disk_t *dev, int command)
{
    dev->sense[2] = dev->sense[12] = dev->sense[13] = 0;
}


static void
set_phase(scsi_disk_t *dev, uint8_t phase)
{
    if (dev->drv->bus != HDD_BUS_SCSI)
	return;

    scsi_devices[dev->drv->bus_id.scsi.id][dev->drv->bus_id.scsi.lun].phase = phase;
}


static void
cmd_error(scsi_disk_t *dev)
{
    set_phase(dev, SCSI_PHASE_STATUS);

    dev->error = ((dev->sense[2] & 0xf) << 4) | ABRT_ERR;
    dev->status = READY_STAT | ERR_STAT;
    dev->phase = 3;
    dev->packet_status = 0x80;
    dev->callback = 50 * SCSI_TIME;

    DEBUG("SCSI HD %i: ERROR: %02X/%02X/%02X\n",
	  dev->id, dev->sense[2], dev->sense[12], dev->sense[13]);
}


static void
invalid_lun(scsi_disk_t *dev)
{
    dev->sense[2] = SENSE_ILLEGAL_REQUEST;
    dev->sense[12] = ASC_INV_LUN;
    dev->sense[13] = 0;
    set_phase(dev, SCSI_PHASE_STATUS);

    cmd_error(dev);
}


static void
illegal_opcode(scsi_disk_t *dev)
{
    dev->sense[2] = SENSE_ILLEGAL_REQUEST;
    dev->sense[12] = ASC_ILLEGAL_OPCODE;
    dev->sense[13] = 0;

    cmd_error(dev);
}


static void
lba_out_of_range(scsi_disk_t *dev)
{
    dev->sense[2] = SENSE_ILLEGAL_REQUEST;
    dev->sense[12] = ASC_LBA_OUT_OF_RANGE;
    dev->sense[13] = 0;

    cmd_error(dev);
}


static void
invalid_field(scsi_disk_t *dev)
{
    dev->sense[2] = SENSE_ILLEGAL_REQUEST;
    dev->sense[12] = ASC_INV_FIELD_IN_CMD_PACKET;
    dev->sense[13] = 0;

    cmd_error(dev);

    dev->status = 0x53;
}


static void
invalid_field_pl(scsi_disk_t *dev)
{
    dev->sense[2] = SENSE_ILLEGAL_REQUEST;
    dev->sense[12] = ASC_INV_FIELD_IN_PARAMETER_LIST;
    dev->sense[13] = 0;

    cmd_error(dev);

    dev->status = 0x53;
}


static void
data_phase_error(scsi_disk_t *dev)
{
    dev->sense[2] = SENSE_ILLEGAL_REQUEST;
    dev->sense[12] = ASC_DATA_PHASE_ERROR;
    dev->sense[13] = 0;

    cmd_error(dev);
}


static int
pre_execution_check(scsi_disk_t *dev, uint8_t *cdb)
{
    if ((cdb[0] != GPCMD_REQUEST_SENSE) && (cdb[1] & 0xe0)) {
	DEBUG("SCSI DISK %i: Attempting to execute a unknown command targeted at SCSI LUN %i\n",
		dev->id, ((dev->request_length >> 5) & 7));
	invalid_lun(dev);
	return 0;
    }

    if (! (command_flags[cdb[0]] & IMPLEMENTED)) {
	DEBUG("SCSI DISK %i: Attempting to execute unknown command %02X\n",
	      dev->id, cdb[0]);
	illegal_opcode(dev);
	return 0;
    }

    /* Unless the command is REQUEST SENSE, clear the sense. This will *NOT*
       the UNIT ATTENTION condition if it's set. */
    if (cdb[0] != GPCMD_REQUEST_SENSE)
	sense_clear(dev, cdb[0]);

    DEBUG("SCSI DISK %i: Continuing with command\n", dev->id);

    return 1;
}


static void
disk_seek(scsi_disk_t *dev, uint32_t pos)
{
    DBGLOG(1, "SCSI DISK %i: Seek %08X\n", dev->id, pos);

    hdd_image_seek(dev->id, pos);
}


static void
disk_rezero(scsi_disk_t *dev)
{
    if (dev->id == 0xff)
	return;

    dev->sector_pos = dev->sector_len = 0;

    disk_seek(dev, 0);
}


static void
disk_reset(void *p)
{
    scsi_disk_t *dev = (scsi_disk_t *)p;

    disk_rezero(dev);

    dev->status = 0;
    dev->callback = 0;
    dev->packet_status = 0xff;
}


void
request_sense(scsi_disk_t *dev, uint8_t *buffer, uint8_t alloc_length, int desc)
{				
    /*Will return 18 bytes of 0*/
    if (alloc_length != 0) {
	memset(buffer, 0, alloc_length);

	if (! desc)
		memcpy(buffer, dev->sense, alloc_length);
	else {
		buffer[1] = dev->sense[2];
		buffer[2] = dev->sense[12];
		buffer[3] = dev->sense[13];
	}
    } else
	return;

    buffer[0] = 0x70;

    DEBUG("SCSI DISK %i: Reporting sense: %02X %02X %02X\n",
	  dev->id, buffer[2], buffer[12], buffer[13]);

    /* Clear the sense stuff as per the spec. */
    sense_clear(dev, GPCMD_REQUEST_SENSE);
}


static void
request_sense_scsi(void *p, uint8_t *buffer, uint8_t alloc_length)
{
    scsi_disk_t *dev = (scsi_disk_t *)p;

    request_sense(dev, buffer, alloc_length, 0);
}


static void
set_buf_len(scsi_disk_t *dev, int32_t *BufLen, int32_t *src_len)
{
    if (*BufLen == -1)
	*BufLen = *src_len;
    else {
	*BufLen = MIN(*src_len, *BufLen);
	*src_len = *BufLen;
    }

    DEBUG("SCSI DISK %i: Actual transfer length: %i\n", dev->id, *BufLen);
}


static void
disk_command(void *p, uint8_t *cdb)
{
    scsi_disk_t *dev = (scsi_disk_t *)p;
#ifdef _LOGGING
    uint8_t *hdbufferb;
#endif
    int32_t *BufLen;
    int32_t len, max_len, alloc_length;
    int pos = 0;
    int idx = 0;
    unsigned size_idx, preamble_len;
    uint32_t last_sector = 0;
    char device_identify[9] = { 'E', 'M', 'U', '_', 'H', 'D', '0', '0', 0 };
    char device_identify_ex[15] = { 'E', 'M', 'U', '_', 'H', 'D', '0', '0', ' ', 'v', '0'+EMU_VER_MAJOR, '.', '0'+EMU_VER_MINOR, '0'+EMU_VER_REV, 0 };
    int block_desc = 0;

#ifdef _LOGGING
    hdbufferb = scsi_devices[dev->drv->bus_id.scsi.id][dev->drv->bus_id.scsi.lun].cmd_buffer;
#endif
    BufLen = &scsi_devices[dev->drv->bus_id.scsi.id][dev->drv->bus_id.scsi.lun].buffer_length;

    last_sector = hdd_image_get_last_sector(dev->id);

    dev->status &= ~ERR_STAT;
    dev->packet_len = 0;

    device_identify[6] = (dev->id / 10) + 0x30;
    device_identify[7] = (dev->id % 10) + 0x30;

    device_identify_ex[6] = (dev->id / 10) + 0x30;
    device_identify_ex[7] = (dev->id % 10) + 0x30;
    device_identify_ex[10] = EMU_VERSION[0];
    device_identify_ex[12] = EMU_VERSION[2];
    device_identify_ex[13] = EMU_VERSION[3];

    memcpy(dev->current_cdb, cdb, 12);

    if (cdb[0] != 0) {
	DEBUG("SCSI DISK %i: Command 0x%02X, Sense Key %02X, Asc %02X, Ascq %02X\n",
	      dev->id, cdb[0], dev->sense[2], dev->sense[12], dev->sense[13]);
	DEBUG("SCSI DISK %i: Request length: %04X\n", dev->id, dev->request_length);

	DEBUG("SCSI DISK %i: CDB: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n", dev->id,
		    cdb[0], cdb[1], cdb[2], cdb[3], cdb[4], cdb[5], cdb[6], cdb[7],
		    cdb[8], cdb[9], cdb[10], cdb[11]);
    }

    dev->sector_len = 0;

    set_phase(dev, SCSI_PHASE_STATUS);

    /* This handles the Not Ready/Unit Attention check if it has to be handled at this point. */
    if (pre_execution_check(dev, cdb) == 0)
	return;

    switch (cdb[0]) {
	case GPCMD_SEND_DIAGNOSTIC:
		if (! (cdb[1] & (1 << 2))) {
			invalid_field(dev);
			return;
		}
		/*FALLTHROUGH*/

	case GPCMD_SCSI_RESERVE:
	case GPCMD_SCSI_RELEASE:
	case GPCMD_TEST_UNIT_READY:
	case GPCMD_FORMAT_UNIT:
		set_phase(dev, SCSI_PHASE_STATUS);
		command_complete(dev);
		break;

	case GPCMD_REZERO_UNIT:
		dev->sector_pos = dev->sector_len = 0;
		disk_seek(dev, 0);
		set_phase(dev, SCSI_PHASE_STATUS);
		break;

	case GPCMD_REQUEST_SENSE:
		/* If there's a unit attention condition and there's a buffered not ready, a standalone REQUEST SENSE
		   should forget about the not ready, and report unit attention straight away. */
		len = cdb[4];

		if (! len) {
			set_phase(dev, SCSI_PHASE_STATUS);
			dev->packet_status = PHASE_COMPLETE;
			dev->callback = 20 * SCSI_TIME;
			break;
		}

		set_buf_len(dev, BufLen, &len);

		if (*BufLen < cdb[4])
			cdb[4] = *BufLen;

		len = (cdb[1] & 1) ? 8 : 18;

		set_phase(dev, SCSI_PHASE_DATA_IN);
		data_command_finish(dev, len, len, cdb[4], 0);
		break;

	case GPCMD_MECHANISM_STATUS:
		len = (cdb[7] << 16) | (cdb[8] << 8) | cdb[9];
		set_buf_len(dev, BufLen, &len);
		set_phase(dev, SCSI_PHASE_DATA_IN);
		data_command_finish(dev, 8, 8, len, 0);
		break;

	case GPCMD_READ_6:
	case GPCMD_READ_10:
	case GPCMD_READ_12:
		switch(cdb[0]) {
			case GPCMD_READ_6:
				dev->sector_len = cdb[4];
				dev->sector_pos = ((((uint32_t) cdb[1]) & 0x1f) << 16) | (((uint32_t) cdb[2]) << 8) | ((uint32_t) cdb[3]);
				break;

			case GPCMD_READ_10:
				dev->sector_len = (cdb[7] << 8) | cdb[8];
				dev->sector_pos = (cdb[2] << 24) | (cdb[3] << 16) | (cdb[4] << 8) | cdb[5];
				break;

			case GPCMD_READ_12:
				dev->sector_len = (((uint32_t) cdb[6]) << 24) | (((uint32_t) cdb[7]) << 16) | (((uint32_t) cdb[8]) << 8) | ((uint32_t) cdb[9]);
				dev->sector_pos = (((uint32_t) cdb[2]) << 24) | (((uint32_t) cdb[3]) << 16) | (((uint32_t) cdb[4]) << 8) | ((uint32_t) cdb[5]);
				break;
		}

		if (dev->sector_pos > last_sector) {
			lba_out_of_range(dev);
			return;
		}

		if ((!dev->sector_len) || (*BufLen == 0)) {
			set_phase(dev, SCSI_PHASE_STATUS);
			DEBUG("SCSI DISK %i: All done - callback set\n", dev);
			dev->packet_status = PHASE_COMPLETE;
			dev->callback = 20 * SCSI_TIME;
			break;
		}

		max_len = dev->sector_len;
		dev->requested_blocks = max_len;

		alloc_length = dev->packet_len = max_len << 9;

		set_buf_len(dev, BufLen, &alloc_length);
		set_phase(dev, SCSI_PHASE_DATA_IN);

		if (dev->requested_blocks > 1)
			data_command_finish(dev, alloc_length, alloc_length / dev->requested_blocks, alloc_length, 0);
		else
			data_command_finish(dev, alloc_length, alloc_length, alloc_length, 0);
		return;

	case GPCMD_VERIFY_6:
	case GPCMD_VERIFY_10:
	case GPCMD_VERIFY_12:
		if (!(cdb[1] & 2)) {
			set_phase(dev, SCSI_PHASE_STATUS);
			command_complete(dev);
			break;
		}
		/*FALLTHROUGH*/

	case GPCMD_WRITE_6:
	case GPCMD_WRITE_10:
	case GPCMD_WRITE_AND_VERIFY_10:
	case GPCMD_WRITE_12:
	case GPCMD_WRITE_AND_VERIFY_12:
		switch(cdb[0]) {
			case GPCMD_VERIFY_6:
			case GPCMD_WRITE_6:
				dev->sector_len = cdb[4];
				dev->sector_pos = ((((uint32_t) cdb[1]) & 0x1f) << 16) | (((uint32_t) cdb[2]) << 8) | ((uint32_t) cdb[3]);
				DEBUG("SCSI DISK %i: Length: %i, LBA: %i\n", dev->id, dev->sector_len, dev->sector_pos);
				break;

			case GPCMD_VERIFY_10:
			case GPCMD_WRITE_10:
			case GPCMD_WRITE_AND_VERIFY_10:
				dev->sector_len = (cdb[7] << 8) | cdb[8];
				dev->sector_pos = (cdb[2] << 24) | (cdb[3] << 16) | (cdb[4] << 8) | cdb[5];
				DEBUG("SCSI DISK %i: Length: %i, LBA: %i\n", dev->id, dev->sector_len, dev->sector_pos);
				break;

			case GPCMD_VERIFY_12:
			case GPCMD_WRITE_12:
			case GPCMD_WRITE_AND_VERIFY_12:
				dev->sector_len = (((uint32_t) cdb[6]) << 24) | (((uint32_t) cdb[7]) << 16) | (((uint32_t) cdb[8]) << 8) | ((uint32_t) cdb[9]);
				dev->sector_pos = (((uint32_t) cdb[2]) << 24) | (((uint32_t) cdb[3]) << 16) | (((uint32_t) cdb[4]) << 8) | ((uint32_t) cdb[5]);
				break;
		}

		if (dev->sector_pos > last_sector) {
			lba_out_of_range(dev);
			return;
		}

		if ((!dev->sector_len) || (*BufLen == 0)) {
			set_phase(dev, SCSI_PHASE_STATUS);
			DEBUG("SCSI DISK %i: All done - callback set\n", dev->id);
			dev->packet_status = PHASE_COMPLETE;
			dev->callback = 20 * SCSI_TIME;
			break;
		}

		max_len = dev->sector_len;
		dev->requested_blocks = max_len;

		alloc_length = dev->packet_len = max_len << 9;

		set_buf_len(dev, BufLen, &alloc_length);
		set_phase(dev, SCSI_PHASE_DATA_OUT);

		if (dev->requested_blocks > 1)
			data_command_finish(dev, alloc_length, alloc_length / dev->requested_blocks, alloc_length, 1);
		else
			data_command_finish(dev, alloc_length, alloc_length, alloc_length, 1);
		return;

	case GPCMD_WRITE_SAME_10:
		if ((cdb[1] & 6) == 6) {
			invalid_field(dev);
			return;
		}

		dev->sector_len = (cdb[7] << 8) | cdb[8];
		dev->sector_pos = (cdb[2] << 24) | (cdb[3] << 16) | (cdb[4] << 8) | cdb[5];
		DEBUG("SCSI DISK %i: Length: %i, LBA: %i\n",
		      dev->id, dev->sector_len, dev->sector_pos);

		if (dev->sector_pos > last_sector) {
			lba_out_of_range(dev);
			return;
		}

		if ((!dev->sector_len) || (*BufLen == 0)) {
			set_phase(dev, SCSI_PHASE_STATUS);
			DEBUG("SCSI DISK %i: All done - callback set\n",
			      dev->id);
			dev->packet_status = PHASE_COMPLETE;
			dev->callback = 20 * SCSI_TIME;
			break;
		}

		max_len = 1;
		dev->requested_blocks = max_len;

		alloc_length = dev->packet_len = max_len << 9;

		set_buf_len(dev, BufLen, &alloc_length);
		set_phase(dev, SCSI_PHASE_DATA_OUT);

		if (dev->requested_blocks > 1)
			data_command_finish(dev, alloc_length, alloc_length / dev->requested_blocks, alloc_length, 1);
		else
			data_command_finish(dev, alloc_length, alloc_length, alloc_length, 1);
		return;

	case GPCMD_MODE_SENSE_6:
	case GPCMD_MODE_SENSE_10:
		set_phase(dev, SCSI_PHASE_DATA_IN);

		block_desc = ((cdb[1] >> 3) & 1) ? 0 : 1;

		if (cdb[0] == GPCMD_MODE_SENSE_6)
			len = cdb[4];
		else
			len = (cdb[8] | (cdb[7] << 8));

		alloc_length = len;

		dev->temp_buffer = (uint8_t *) malloc(65536);
		memset(dev->temp_buffer, 0, 65536);

		if (cdb[0] == GPCMD_MODE_SENSE_6) {
			len = mode_sense(dev, dev->temp_buffer, 4, cdb[2], block_desc);
			if (len > alloc_length)
				len = alloc_length;
			dev->temp_buffer[0] = len - 1;
			dev->temp_buffer[1] = 0;
			if (block_desc)
				dev->temp_buffer[3] = 8;
		} else {
			len = mode_sense(dev, dev->temp_buffer, 8, cdb[2], block_desc);
			if (len > alloc_length)
				len = alloc_length;
			dev->temp_buffer[0]=(len - 2) >> 8;
			dev->temp_buffer[1]=(len - 2) & 255;
			dev->temp_buffer[2] = 0;
			if (block_desc) {
				dev->temp_buffer[6] = 0;
				dev->temp_buffer[7] = 8;
			}
		}

		if (len > alloc_length)
			len = alloc_length;
		else if (len < alloc_length)
			alloc_length = len;

		set_buf_len(dev, BufLen, &alloc_length);
		DEBUG("SCSI DISK %i: Reading mode page: %02X...\n",
		      dev->id, cdb[2]);

		data_command_finish(dev, len, len, alloc_length, 0);
		return;

	case GPCMD_MODE_SELECT_6:
	case GPCMD_MODE_SELECT_10:
		set_phase(dev, SCSI_PHASE_DATA_OUT);

		if (cdb[0] == GPCMD_MODE_SELECT_6)
			len = cdb[4];
		else
			len = (cdb[7] << 8) | cdb[8];

		set_buf_len(dev, BufLen, &len);
		dev->total_length = len;
		dev->do_page_save = cdb[1] & 1;
		data_command_finish(dev, len, len, len, 1);
		return;

	case GPCMD_INQUIRY:
		max_len = cdb[3];
		max_len <<= 8;
		max_len |= cdb[4];

		if ((!max_len) || (*BufLen == 0)) {
			set_phase(dev, SCSI_PHASE_STATUS);
			DBGLOG(1, "SCSI DISK %i: All done - callback set\n", dev->id);
			dev->packet_status = PHASE_COMPLETE;
			dev->callback = 20 * SCSI_TIME;
			break;
		}			

		dev->temp_buffer = mem_alloc(1024);

		if (cdb[1] & 1) {
			preamble_len = 4;
			size_idx = 3;

			dev->temp_buffer[idx++] = 05;
			dev->temp_buffer[idx++] = cdb[2];
			dev->temp_buffer[idx++] = 0;

			idx++;

			switch (cdb[2]) {
				case 0x00:
					dev->temp_buffer[idx++] = 0x00;
					dev->temp_buffer[idx++] = 0x83;
					break;

				case 0x83:
					if (idx + 24 > max_len) {
						free(dev->temp_buffer);
						dev->temp_buffer = NULL;
						data_phase_error(dev);
						return;
					}

					dev->temp_buffer[idx++] = 0x02;
					dev->temp_buffer[idx++] = 0x00;
					dev->temp_buffer[idx++] = 0x00;
					dev->temp_buffer[idx++] = 20;
					ide_padstr8(dev->temp_buffer + idx, 20, "53R141");	/* Serial */
					idx += 20;

					if (idx + 72 > cdb[4])
						goto atapi_out;
					dev->temp_buffer[idx++] = 0x02;
					dev->temp_buffer[idx++] = 0x01;
					dev->temp_buffer[idx++] = 0x00;
					dev->temp_buffer[idx++] = 68;
					ide_padstr8(dev->temp_buffer + idx, 8, EMU_NAME); /* Vendor */
					idx += 8;
					ide_padstr8(dev->temp_buffer + idx, 40, device_identify_ex); /* Product */
					idx += 40;
					ide_padstr8(dev->temp_buffer + idx, 20, "53R141"); /* Product */
					idx += 20;
					break;
				default:
					DEBUG("INQUIRY: Invalid page: %02X\n", cdb[2]);
					free(dev->temp_buffer);
					dev->temp_buffer = NULL;
					invalid_field(dev);
					return;
			}
		} else {
			preamble_len = 5;
			size_idx = 4;

			memset(dev->temp_buffer, 0, 8);
			dev->temp_buffer[0] = 0; /*SCSI HD*/
			dev->temp_buffer[1] = 0; /*Fixed*/
			dev->temp_buffer[2] = 0x02; /*SCSI-2 compliant*/
			dev->temp_buffer[3] = 0x02;
			dev->temp_buffer[4] = 31;
			dev->temp_buffer[6] = 1;	/* 16-bit transfers supported */
			dev->temp_buffer[7] = 0x20;	/* Wide bus supported */

			ide_padstr8(dev->temp_buffer + 8, 8, EMU_NAME); /* Vendor */
			ide_padstr8(dev->temp_buffer + 16, 16, device_identify); /* Product */
			ide_padstr8(dev->temp_buffer + 32, 4, EMU_VERSION); /* Revision */
			idx = 36;

			if (max_len == 96) {
				dev->temp_buffer[4] = 91;
				idx = 96;
			}
		}

atapi_out:
		dev->temp_buffer[size_idx] = idx - preamble_len;
		len=idx;

		DEBUG("scsi_disk_command(): Inquiry (%08X, %08X)\n",
		      hdbufferb, dev->temp_buffer);
		
		if (len > max_len)
			len = max_len;

		set_buf_len(dev, BufLen, &len);

		if (len > *BufLen)
			len = *BufLen;

		set_phase(dev, SCSI_PHASE_DATA_IN);
		data_command_finish(dev, len, len, max_len, 0);
		break;

	case GPCMD_PREVENT_REMOVAL:
		set_phase(dev, SCSI_PHASE_STATUS);
		command_complete(dev);
		break;

	case GPCMD_SEEK_6:
	case GPCMD_SEEK_10:
		switch(cdb[0]) {
			case GPCMD_SEEK_6:
				pos = (cdb[2] << 8) | cdb[3];
				break;

			case GPCMD_SEEK_10:
				pos = (cdb[2] << 24) | (cdb[3]<<16) | (cdb[4]<<8) | cdb[5];
				break;
		}
		disk_seek(dev, pos);

		set_phase(dev, SCSI_PHASE_STATUS);
		command_complete(dev);
		break;

	case GPCMD_READ_CDROM_CAPACITY:
		dev->temp_buffer = (uint8_t *) malloc(8);

		if (read_capcity(dev, dev->current_cdb, dev->temp_buffer, (uint32_t *) &len) == 0) {
			set_phase(dev, SCSI_PHASE_STATUS);
			return;
		}

		set_buf_len(dev, BufLen, &len);

		set_phase(dev, SCSI_PHASE_DATA_IN);
		data_command_finish(dev, len, len, len, 0);
		break;

	default:
		illegal_opcode(dev);
		break;
    }

    DBGLOG(1, "SCSI DISK %i: Phase: %02X, request length: %i\n",
	   dev->id, dev->phase, dev->request_length);
}


static void
phase_data_in(scsi_disk_t *dev)
{
    uint8_t *hdbufferb = scsi_devices[dev->drv->bus_id.scsi.id][dev->drv->bus_id.scsi.lun].cmd_buffer;
    int32_t *BufLen = &scsi_devices[dev->drv->bus_id.scsi.id][dev->drv->bus_id.scsi.lun].buffer_length;

    if (!*BufLen) {
	DEBUG("scsi_disk_phase_data_in(): Buffer length is 0\n");
	set_phase(dev, SCSI_PHASE_STATUS);

	return;
    }

    switch (dev->current_cdb[0]) {
	case GPCMD_REQUEST_SENSE:
		DEBUG("SCSI DISK %i: %08X, %08X\n", dev->id, hdbufferb, *BufLen);
		request_sense(dev, hdbufferb, *BufLen, dev->current_cdb[1] & 1);
		break;

	case GPCMD_MECHANISM_STATUS:
		memset(hdbufferb, 0, *BufLen);
		hdbufferb[5] = 1;
		break;

	case GPCMD_READ_6:
	case GPCMD_READ_10:
	case GPCMD_READ_12:
		if ((dev->requested_blocks > 0) && (*BufLen > 0)) {
			if (dev->packet_len > (uint32_t) *BufLen)
				hdd_image_read(dev->id, dev->sector_pos, *BufLen >> 9, hdbufferb);
			else
				hdd_image_read(dev->id, dev->sector_pos, dev->requested_blocks, hdbufferb);
		}
		break;

	case GPCMD_MODE_SENSE_6:
	case GPCMD_MODE_SENSE_10:
	case GPCMD_INQUIRY:
	case GPCMD_READ_CDROM_CAPACITY:
		DEBUG("scsi_disk_phase_data_in(): Filling buffer (%08X, %08X)\n", hdbufferb, dev->temp_buffer);
		memcpy(hdbufferb, dev->temp_buffer, *BufLen);
		free(dev->temp_buffer);
		dev->temp_buffer = NULL;
		DEBUG("%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
			    hdbufferb[0], hdbufferb[1], hdbufferb[2], hdbufferb[3], hdbufferb[4], hdbufferb[5], hdbufferb[6], hdbufferb[7],
			    hdbufferb[8], hdbufferb[9], hdbufferb[10], hdbufferb[11], hdbufferb[12], hdbufferb[13], hdbufferb[14], hdbufferb[15]);
		break;

	default:
		fatal("SCSI HDD %i: Bad Command for phase 2 (%02X)\n", dev->id, dev->current_cdb[0]);
		break;
    }

    set_phase(dev, SCSI_PHASE_STATUS);
}


static void
phase_data_out(scsi_disk_t *dev)
{
    uint8_t *hdbufferb = scsi_devices[dev->drv->bus_id.scsi.id][dev->drv->bus_id.scsi.lun].cmd_buffer;
    int i;
    int32_t *BufLen = &scsi_devices[dev->drv->bus_id.scsi.id][dev->drv->bus_id.scsi.lun].buffer_length;
    uint32_t last_sector = hdd_image_get_last_sector(dev->id);
    uint32_t c, h, s, last_to_write = 0;
    uint16_t block_desc_len, pos;
    uint8_t hdr_len, val, old_val, ch, error = 0;
    uint8_t page, page_len;

    if (!*BufLen) {
	set_phase(dev, SCSI_PHASE_STATUS);

	return;
    }

    switch (dev->current_cdb[0]) {
	case GPCMD_VERIFY_6:
	case GPCMD_VERIFY_10:
	case GPCMD_VERIFY_12:
		break;

	case GPCMD_WRITE_6:
	case GPCMD_WRITE_10:
	case GPCMD_WRITE_AND_VERIFY_10:
	case GPCMD_WRITE_12:
	case GPCMD_WRITE_AND_VERIFY_12:
		if ((dev->requested_blocks > 0) && (*BufLen > 0)) {
			if (dev->packet_len > (uint32_t) *BufLen)
				hdd_image_write(dev->id, dev->sector_pos, *BufLen >> 9, hdbufferb);
			else
				hdd_image_write(dev->id, dev->sector_pos, dev->requested_blocks, hdbufferb);
		}
		break;

	case GPCMD_WRITE_SAME_10:
		if (!dev->current_cdb[7] && !dev->current_cdb[8])
			last_to_write = last_sector;
		else
			last_to_write = dev->sector_pos + dev->sector_len - 1;

		for (i = dev->sector_pos; i <= (int) last_to_write; i++) {
			if (dev->current_cdb[1] & 2) {
				hdbufferb[0] = (i >> 24) & 0xff;
				hdbufferb[1] = (i >> 16) & 0xff;
				hdbufferb[2] = (i >> 8) & 0xff;
				hdbufferb[3] = i & 0xff;
			} else if (dev->current_cdb[1] & 4) {
				s = (i % dev->drv->spt);
				h = ((i - s) / dev->drv->spt) % dev->drv->hpc;
				c = ((i - s) / dev->drv->spt) / dev->drv->hpc;
				hdbufferb[0] = (c >> 16) & 0xff;
				hdbufferb[1] = (c >> 8) & 0xff;
				hdbufferb[2] = c & 0xff;
				hdbufferb[3] = h & 0xff;
				hdbufferb[4] = (s >> 24) & 0xff;
				hdbufferb[5] = (s >> 16) & 0xff;
				hdbufferb[6] = (s >> 8) & 0xff;
				hdbufferb[7] = s & 0xff;
			}
			hdd_image_write(dev->id, i, 1, hdbufferb);
		}
		break;

	case GPCMD_MODE_SELECT_6:
	case GPCMD_MODE_SELECT_10:
		if (dev->current_cdb[0] == GPCMD_MODE_SELECT_10)
			hdr_len = 8;
		else
			hdr_len = 4;

		if (dev->current_cdb[0] == GPCMD_MODE_SELECT_6) {
			block_desc_len = hdbufferb[2];
			block_desc_len <<= 8;
			block_desc_len |= hdbufferb[3];
		} else {
			block_desc_len = hdbufferb[6];
			block_desc_len <<= 8;
			block_desc_len |= hdbufferb[7];
		}

		pos = hdr_len + block_desc_len;

		while(1) {
			if (pos >= dev->current_cdb[4]) {
				DEBUG("SCSI DISK %i: Buffer has only block descriptor\n", dev->id);
				break;
			}

			page = hdbufferb[pos] & 0x3F;
			page_len = hdbufferb[pos + 1];

			pos += 2;

			if (!(mode_sense_page_flags & (1LL << ((uint64_t) page))))
				error |= 1;
			else {
				for (i = 0; i < page_len; i++) {
					ch = mode_sense_pages_changeable.pages[page][i + 2];
					val = hdbufferb[pos + i];
					old_val = dev->ms_pages_saved.pages[page][i + 2];
					if (val != old_val) {
						if (ch)
							dev->ms_pages_saved.pages[page][i + 2] = val;
						else
							error |= 1;
					}
				}
			}

			pos += page_len;

			val = mode_sense_pages_default.pages[page][0] & 0x80;
			if (dev->do_page_save && val)
				mode_sense_save(dev);

			if (pos >= dev->total_length)
				break;
		}

		if (error)
			invalid_field_pl(dev);
		break;

	default:
		fatal("SCSI DISK %i: Bad Command for phase 2 (%02X)\n", dev->id, dev->current_cdb[0]);
		break;
    }

    set_phase(dev, SCSI_PHASE_STATUS);
}


/* If the result is 1, issue an IRQ, otherwise not. */
static void
do_callback(void *p)
{
    scsi_disk_t *dev = (scsi_disk_t *)p;

    switch(dev->packet_status) {
	case PHASE_IDLE:
		DEBUG("SCSI DISK %i: PHASE_IDLE\n", dev->id);
		dev->phase = 1;
		dev->status = READY_STAT | DRQ_STAT | (dev->status & ERR_STAT);
		return;

	case PHASE_COMPLETE:
		DEBUG("SCSI DISK %i: PHASE_COMPLETE\n", dev->id);
		dev->status = READY_STAT;
		dev->phase = 3;
		dev->packet_status = 0xFF;
		return;

	case PHASE_DATA_OUT_DMA:
		DEBUG("SCSI DISK %i: PHASE_DATA_OUT_DMA\n", dev->id);
		phase_data_out(dev);
		dev->packet_status = PHASE_COMPLETE;
		dev->status = READY_STAT;
		dev->phase = 3;
		return;

	case PHASE_DATA_IN_DMA:
		DEBUG("SCSI DISK %i: PHASE_DATA_IN_DMA\n", dev->id);
		phase_data_in(dev);
		dev->packet_status = PHASE_COMPLETE;
		dev->status = READY_STAT;
		dev->phase = 3;
		return;

	case PHASE_ERROR:
		DEBUG("SCSI DISK %i: PHASE_ERROR\n", dev->id);
		dev->status = READY_STAT | ERR_STAT;
		dev->phase = 3;
		return;
    }
}


void
scsi_disk_hard_reset(void)
{
    scsi_device_t *sd;
    scsi_disk_t *dev;
    int c;

    for (c = 0; c < HDD_NUM; c++) {
	if (hdd[c].bus != HDD_BUS_SCSI) continue;

	DEBUG("SCSI disk hard_reset drive=%d\n", c);

	/* Make sure to ignore any SCSI disk that has an out of range ID. */
	if (hdd[c].bus_id.scsi.id >= SCSI_ID_MAX)
		continue;
	if (hdd[c].bus_id.scsi.lun >= SCSI_LUN_MAX)
		continue;

	/* Make sure to ignore any SCSI disk whose image file name is empty. */
	if (wcslen(hdd[c].fn) == 0)
		continue;

	/* Make sure to ignore any SCSI disk whose image fails to load. */
	if (! hdd_image_load(c))
		continue;

	dev = (scsi_disk_t *)hdd[c].priv;
	if (dev == NULL) {
		dev = (scsi_disk_t *)mem_alloc(sizeof(scsi_disk_t));
	}
	memset(dev, 0x00, sizeof(scsi_disk_t));
	dev->id = c;
	dev->drv = &hdd[c];

	/* SCSI disk, attach to the SCSI bus. */
	sd = &scsi_devices[hdd[c].bus_id.scsi.id][hdd[c].bus_id.scsi.lun];

	sd->p = dev;
	sd->command = disk_command;
	sd->callback = do_callback;
	sd->err_stat_to_scsi = err_stat_to_scsi;
	sd->request_sense = request_sense_scsi;
	sd->reset = disk_reset;
	sd->read_capacity = read_capcity;
	sd->type = SCSI_FIXED_DISK;

	mode_sense_load(dev);

	DEBUG("SCSI disk %i attached to ID %i LUN %i\n",
	      c, hdd[c].bus_id.scsi.id, hdd[c].bus_id.scsi.lun);
    }
}


void
scsi_disk_close(void)
{
    scsi_disk_t *dev;
    int c;

    for (c = 0; c < HDD_NUM; c++) {
	dev = (scsi_disk_t *)hdd[c].priv;

	if (dev != NULL) {
		hdd_image_close(c);

		free(dev);

		hdd[c].priv = NULL;
	}
    }
}
