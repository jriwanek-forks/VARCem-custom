/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		SCSI module definitions.
 *
 * Version:	@(#)scsi.h	1.0.10	2019/04/23
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		TheCollector1995, <mariogplayer@gmail.com>
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
#ifndef EMU_SCSI_H
#define EMU_SCSI_H


#define SCSI_NONE	0	/* no SCSI controller selected */
#define SCSI_INTERNAL	1	/* internal/onboard controller selected */


extern void		scsi_card_log(int level, const char *fmt, ...);

extern int		scsi_card_available(int card);
extern const char	*scsi_card_getname(int card);
#ifdef EMU_DEVICE_H
extern const device_t	*scsi_card_getdevice(int card);
#endif
extern int		scsi_card_has_config(int card);
extern const char	*scsi_card_get_internal_name(int card);
extern int		scsi_card_get_from_internal_name(const char *s);
extern void		scsi_card_init(void);


#endif	/*EMU_SCSI_H*/
