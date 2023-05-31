/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Definitions for the Sound Emulation core.
 *
 * Version:	@(#)sound.h	1.0.13	2021/06/03
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2021 Fred N. van Kempen.
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
#ifndef EMU_SOUND_H
# define EMU_SOUND_H


#define SOUNDBUFLEN	(48000/50)

#define CD_FREQ		44100
#define CD_BUFLEN	(CD_FREQ / 10)

#define SOUND_NONE	0
#define SOUND_INTERNAL	1


#ifdef __cplusplus
extern "C" {
#endif

extern int	sound_pos_global;

#ifdef EMU_DEVICE_H
/* Sound card devices. */
extern const device_t adlib_device;
extern const device_t adlib_mca_device;
extern const device_t adgold_device;
extern const device_t es1371_device;
extern const device_t sbpci128_device;
extern const device_t cms_device;
extern const device_t gus_device;
#if defined(DEV_BRANCH) && defined(USE_GUSMAX)
extern const device_t gusmax_device;
#endif
#if defined(DEV_BRANCH) && defined(USE_PAS16)
extern const device_t pas16_device;
#endif
extern const device_t sb_1_device;
extern const device_t sb_15_device;
extern const device_t sb_mcv_device;
extern const device_t sb_2_device;
extern const device_t sb_pro_v1_device;
extern const device_t sb_pro_v2_device;
extern const device_t sb_pro_mcv_device;
extern const device_t sb_16_device;
extern const device_t sb_awe32_device;
#ifdef USE_RESID
extern const device_t ssi2001_device;
#endif
extern const device_t wss_device;
extern const device_t ncr_business_audio_device;
#endif

extern void	sound_log(int level, const char *fmt, ...);

extern void	sound_add_handler(void (*get_buffer)(int32_t *buffer, \
				  int len, void *p), void *p);

extern void	sound_card_log(int level, const char *fmt, ...);
extern int	sound_card_available(int card);
extern const char	*sound_card_getname(int card);
#ifdef EMU_DEVICE_H
extern const device_t	*sound_card_getdevice(int card);
#endif
extern int	sound_card_has_config(int card);
extern const char	*sound_card_get_internal_name(int card);
extern int	sound_card_get_from_internal_name(const char *s);
extern void	sound_card_init(void);
extern void	sound_card_reset(void);

extern void	sound_speed_changed(void);

extern void	sound_reset(void);
extern void	sound_init(void);
extern void	sound_close(void);

extern void	sound_cd_stop(void);
extern void	sound_cd_set_volume(unsigned int vol_l, unsigned int vol_r);

extern void	openal_close(void);
extern void	openal_init(void);
extern void	openal_reset(void);
extern void	openal_buffer(void *buf);
extern void	openal_buffer_cd(void *buf);
extern void	openal_buffer_midi(void *buf, uint32_t size);
extern void	openal_set_midi(int freq, int buf_size);

extern void	resid_init(void);

#ifdef __cplusplus
}
#endif


#endif	/*EMU_SOUND_H*/
