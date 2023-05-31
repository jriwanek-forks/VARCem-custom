/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the Pro Audio Spectrum sound device.
 *
 *		Original PAS uses
 *		  2 x OPL2
 *		  PIT - sample rate/count
 *		  LMC835N/LMC1982 - mixer
 *		  YM3802 - MIDI Control System
 *
 *		9A01 - IO base
 *			base >> 2
 *
 *		All below + IO base
 *
 *		B89 - interrupt status / clear
 *		  bit 2 - sample rate
 *		  bit 3 - PCM
 *		  bit 4 - MIDI
 *
 *		B88 - Audio mixer control register
 *
 *		B8A - Audio filter control
 *		  bit 5 - mute?
 *
 *		B8B - interrupt mask / board ID
 *		  bits 5-7 - board ID (read only on PAS16)
 *
 *		F88 - PCM data (low)
 *
 *		F89 - PCM data (high)
 *
 *		F8A - PCM control?
 *		  bit 4 - input/output select (1 = output)
 *		  bit 5 - mono/stereo select
 *		  bit 6 - PCM enable
 *
 *		1388-138b - PIT clocked at 1193180 Hz
 *		1388 - sample rate
 *		1389 - sample count
 *
 *		178b - 
 *		2789 - board revision
 *
 *		8389 -
 *		  bit 2 - 8/16 bit
 *
 *		BF88 - wait states
 *
 *		EF8B -
 *		  bit 3 - 16 bits okay ?
 *
 *		F388 - 
 *		  bit 6 - joystick enable
 *
 *		F389 -
 *		  bits 0-2 - DMA
 *
 *		F38A -
 *		  bits 0-3 - IRQ
 *
 *		F788 -
 *		  bit 1 - SB emulation
 *		  bit 0 - MPU401 emulation
 *
 *		F789 - SB base addr
 *		  bits 0-3 - addr bits 4-7
 *
 *		FB8A - SB IRQ/DMA
 *		  bits 3-5 - IRQ
 *		  bits 6-7 - DMA
 *
 *		FF88 - board model
 *		  3 = PAS16
 *
 * Version:	@(#)snd_pas16.c	1.0.17	2020/07/17
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2020 Fred N. van Kempen.
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
#include <stdlib.h>
#include <wchar.h>
#define dbglog sound_card_log
#include <86box/86box.h>
#include <86box/timer.h>
#include "../cpu/cpu.h"
#include <86box/io.h>
#include <86box/device.h>
#include <86box/plat.h>
#include <86box/clk.h>
#include <86box/dma.h>
#include <86box/pic.h>
#include <86box/pit.h>
#include <86box/sound.h>
#include <86box/snd_filters.h>
#include <86box/snd_mpu401.h>
#include <86box/snd_opl.h>
#include <86box/snd_sb.h>
#include <86box/snd_sb_dsp.h>


typedef struct pas16_t
{
        uint16_t base;
        int irq, dma;
        uint8_t audiofilt;
        uint8_t audio_mixer;
        uint8_t compat, compat_base;
        uint8_t enhancedscsi;
        uint8_t io_conf_1, io_conf_2, io_conf_3, io_conf_4;
        uint8_t irq_stat, irq_ena;
        uint8_t pcm_ctrl;
        uint16_t pcm_dat;
        uint16_t pcm_dat_l, pcm_dat_r;
        uint8_t sb_irqdma;
        int stereo_lr;
        uint8_t sys_conf_1, sys_conf_2, sys_conf_3, sys_conf_4;
        
        struct
        {
                uint32_t l[3];
                tmrval_t c[3];
                uint8_t m[3];
                uint8_t ctrl, ctrls[2];
                int wp, rm[3], wm[3];
                uint16_t rl[3];
                int thit[3];
                int delay[3];
                int rereadlatch[3];
                tmrval_t enable[3];
        } pit;

        opl_t    opl;
        sb_dsp_t dsp;

        int16_t pcm_buffer[2][SOUNDBUFLEN];

        int pos;
} pas16_t;

static uint8_t pas16_pit_in(uint16_t, priv_t);
static void pas16_pit_out(uint16_t, uint8_t, priv_t);
static void pas16_update(pas16_t *);

static int pas16_dmas[8] = {4, 1, 2, 3, 0, 5, 6, 7};
static int pas16_irqs[16] = {0, 2, 3, 4, 5, 6, 7, 10, 11, 12, 14, 15, 0, 0, 0, 0};
static int pas16_sb_irqs[8] = {0, 2, 3, 5, 7, 10, 11, 12};
static int pas16_sb_dmas[8] = {0, 1, 2, 3};

enum
{
        PAS16_INT_SAMP = 0x04,
        PAS16_INT_PCM  = 0x08
};

enum
{
        PAS16_PCM_MONO = 0x20,
        PAS16_PCM_ENA  = 0x40
};

enum
{
        PAS16_SC2_16BIT = 0x04,
        PAS16_SC2_MSBINV = 0x10
};

enum
{
        PAS16_FILT_MUTE = 0x20
};

static uint8_t pas16_in(uint16_t port, priv_t priv)
{
        pas16_t *pas16 = (pas16_t *)priv;
        uint8_t temp;

        switch ((port - pas16->base) + 0x388)
        {
                case 0x388: case 0x389: case 0x38a: case 0x38b:
                temp = opl3_read((port - pas16->base) + 0x388, &pas16->opl);
                break;
                
                case 0xb88:
                temp = pas16->audio_mixer;
                break;
                
                case 0xb89:
                temp = pas16->irq_stat;
                break;
        
                case 0xb8a:
                temp = pas16->audiofilt;
                break;
                        
                case 0xb8b:
                temp = (pas16->irq_ena & ~0xe0) | 0x20;
                break;
                
                case 0xf8a:
                temp = pas16->pcm_ctrl;
                break;
                                
                case 0x1388: case 0x1389: case 0x138a: case 0x138b:
                temp = pas16_pit_in(port, pas16);
                break;

                case 0x2789: /*Board revision*/
                temp = 0;
                break;
                
                case 0x7f89:
                temp = pas16->enhancedscsi & ~1;
                break;
                
                case 0x8388:
                temp = pas16->sys_conf_1;
                break;
                case 0x8389:
                temp = pas16->sys_conf_2;
                break;
                case 0x838b:
                temp = pas16->sys_conf_3;
                break;
                case 0x838c:
                temp = pas16->sys_conf_4;
                break;
                
                case 0xef8b:
                temp = 0x0c;
                break;

                case 0xf388:
                temp = pas16->io_conf_1;
                break;
                case 0xf389:
                temp = pas16->io_conf_2;
                break;
                case 0xf38b:
                temp = pas16->io_conf_3;
                break;
                case 0xf38c:
                temp = pas16->io_conf_4;
                break;

                case 0xf788:
                temp = pas16->compat;
                break;
                case 0xf789:
                temp = pas16->compat_base;
                break;
                
                case 0xfb8a:
                temp = pas16->sb_irqdma;
                break;

                case 0xff88: /*Board model*/
                temp = 4; /*PAS16*/
                break;
                case 0xff8b: /*Master mode read*/
                temp = 0x20 | 0x10 | 0x01; /*AT bus, XT/AT timing*/
                break;

		default:
		temp = 0xff;
		break;
        }

        return temp;
}

static void pas16_out(uint16_t port, uint8_t val, priv_t priv)
{
        pas16_t *pas16 = (pas16_t *)priv;

        switch ((port - pas16->base) + 0x388)
        {
                case 0x388: case 0x389: case 0x38a: case 0x38b:
                opl3_write((port - pas16->base) + 0x388, val, &pas16->opl);
                break;
                
                case 0xb88:
                pas16->audio_mixer = val;
                break;

                case 0xb89:
                pas16->irq_stat &= ~val;
                break;

                case 0xb8a:
                pas16_update(pas16);
                pas16->audiofilt = val;
                break;

                case 0xb8b:
                pas16->irq_ena = val;
                break;

                case 0xf88:
                pas16_update(pas16);
                pas16->pcm_dat = (pas16->pcm_dat & 0xff00) | val;
                break;
                case 0xf89:
                pas16_update(pas16);
                pas16->pcm_dat = (pas16->pcm_dat & 0x00ff) | (val << 8);
                break;               
                case 0xf8a:
                if ((val & PAS16_PCM_ENA) && !(pas16->pcm_ctrl & PAS16_PCM_ENA)) /*Guess*/
                        pas16->stereo_lr = 0;
                pas16->pcm_ctrl = val;
                break;
                
                case 0x1388: case 0x1389: case 0x138a: case 0x138b:
                pas16_pit_out(port, val, pas16);
                break;

                case 0x7f89:
                pas16->enhancedscsi = val;
                break;

                case 0x8388:
                pas16->sys_conf_1 = val;
                break;
                case 0x8389:
                pas16->sys_conf_2 = val;
                break;
                case 0x838a:
                pas16->sys_conf_3 = val;
                break;
                case 0x838b:
                pas16->sys_conf_4 = val;
                break;

                case 0xf388:
                pas16->io_conf_1 = val;
                break;
                case 0xf389:
                pas16->io_conf_2 = val;
                pas16->dma = pas16_dmas[val & 0x7];
                DEBUG("pas16_out : set PAS DMA %i\n", pas16->dma);
                break;
                case 0xf38a:
                pas16->io_conf_3 = val;
                pas16->irq = pas16_irqs[val & 0xf];
                DEBUG("pas16_out : set PAS IRQ %i\n", pas16->irq);
                break;
                case 0xf38b:
                pas16->io_conf_4 = val;
                break;

                case 0xf788:
                pas16->compat = val;
                if (pas16->compat & 0x02)
                        sb_dsp_setaddr(&pas16->dsp, ((pas16->compat_base & 0xf) << 4) | 0x200);
                else
                        sb_dsp_setaddr(&pas16->dsp, 0);
                break;
                case 0xf789:
                pas16->compat_base = val;
                if (pas16->compat & 0x02)
                        sb_dsp_setaddr(&pas16->dsp, ((pas16->compat_base & 0xf) << 4) | 0x200);
                break;
                
                case 0xfb8a:
                pas16->sb_irqdma = val;
                sb_dsp_setirq(&pas16->dsp, pas16_sb_irqs[(val >> 3) & 7]);
                sb_dsp_setdma8(&pas16->dsp, pas16_sb_dmas[(val >> 6) & 3]);
                DEBUG("pas16_out : set SB IRQ %i DMA %i\n", pas16_sb_irqs[(val >> 3) & 7], pas16_sb_dmas[(val >> 6) & 3]);
                break;
                
                default:
                DEBUG("pas16_out : unknown %04X\n", port);
        }
}

static void pas16_pit_out(uint16_t port, uint8_t val, priv_t priv)
{
        pas16_t *pas16 = (pas16_t *)priv;
        int t;
        switch (port & 3)
        {
                case 3: /*CTRL*/
                if ((val & 0xC0) == 0xC0)
                {
                        if (!(val & 0x20))
                        {
                                if (val & 2) pas16->pit.rl[0] = (int16_t) (pas16->pit.c[0] / (PITCONST * (1 << TIMER_SHIFT)));
                                if (val & 4) pas16->pit.rl[1] = (int16_t) (pas16->pit.c[1]);
                                if (val & 8) pas16->pit.rl[2] = (int16_t) (pas16->pit.c[2]);
                        }
                        return;
                }
                t = val >> 6;
                pas16->pit.ctrls[t] = pas16->pit.ctrl = val;
                if (t == 3)
                {
                        DEBUG("PAS16: bad PIT reg select\n");
                        return;
                }
                if (!(pas16->pit.ctrl & 0x30))
                {
                        pas16->pit.rl[t] = (int16_t)pas16->pit.c[t];
                        if (!t)
                                pas16->pit.rl[t] /= (int16_t) ((PITCONST * (1 << TIMER_SHIFT)));
                        if (pas16->pit.c[t] < 0) 
                                pas16->pit.rl[t] = 0;
                        pas16->pit.ctrl |= 0x30;
                        pas16->pit.rereadlatch[t] = 0;
                        pas16->pit.rm[t] = 3;
                }
                else
                {
                        pas16->pit.rm[t] = pas16->pit.wm[t] = (pas16->pit.ctrl >> 4) & 3;
                        pas16->pit.m[t] = (val >> 1) & 7;
                        if (pas16->pit.m[t] > 5)
                                pas16->pit.m[t] &= 3;
                        if (!pas16->pit.rm[t])
                        {
                                pas16->pit.rm[t] = 3;
                                pas16->pit.rl[t] = (int16_t)pit.c[t];
                                if (!t)
                                        pas16->pit.rl[t] /= (int16_t) ((PITCONST * (1 << TIMER_SHIFT)));
                        }
                        pas16->pit.rereadlatch[t] = 1;
                }
                pas16->pit.wp = 0;
                pas16->pit.thit[t] = 0;
                break;
                case 0: case 1: case 2: /*Timers*/
                t = port & 3;
                switch (pas16->pit.wm[t])
                {
                        case 1:
                        pas16->pit.l[t] = val;
                        pas16->pit.thit[t] = 0;
                        pas16->pit.c[t] = pas16->pit.l[t];
                        if (!t)
                                pas16->pit.c[t] *= (tmrval_t) (PITCONST * (1 << TIMER_SHIFT));
                        pas16->pit.enable[t] = 1;
                        break;
                        case 2:
                        pas16->pit.l[t] = val << 8;
                        pas16->pit.thit[t] = 0;
                        pas16->pit.c[t] = pas16->pit.l[t];
                        if (!t)
                                pas16->pit.c[t] *= (tmrval_t) (PITCONST * (1 << TIMER_SHIFT));
                        pas16->pit.enable[t] = 1;
                        break;
                        case 0:
                        pas16->pit.l[t] &= 0xFF;
                        pas16->pit.l[t] |= (val << 8);
                        pas16->pit.c[t] = pas16->pit.l[t];
                        if (!t)
                                pas16->pit.c[t] *= (tmrval_t) (PITCONST * (1 << TIMER_SHIFT));
                        pas16->pit.thit[t] = 0;
                        pas16->pit.wm[t] = 3;
                        pas16->pit.enable[t] = 1;
                        break;
                        case 3:
                        pas16->pit.l[t] &= 0xFF00;
                        pas16->pit.l[t] |= val;
                        pas16->pit.wm[t] = 0;                        
                        break;
                }
                if (!pas16->pit.l[t])
                {
                        pas16->pit.l[t] |= 0x10000;
                        pas16->pit.c[t] = pas16->pit.l[t];
                        if (!t)
                                pas16->pit.c[t] *= (tmrval_t) (PITCONST * (1 << TIMER_SHIFT));
                }
                break;
        }
}

static uint8_t pas16_pit_in(uint16_t port, priv_t priv)
{
        pas16_t *pas16 = (pas16_t *)priv;
        uint8_t temp = 0xff;
        int t = port & 3;
        switch (port & 3)
        {
                case 0: case 1: case 2: /*Timers*/
                if (pas16->pit.rereadlatch[t])
                {
                        pas16->pit.rereadlatch[t] = 0;
                        if (!t)
                        {
                                pas16->pit.rl[t] = (uint16_t) (pas16->pit.c[t] / (PITCONST * (1 << TIMER_SHIFT)));
                                if ((pas16->pit.c[t] / (PITCONST * (1 << TIMER_SHIFT))) > 65536) 
                                        pas16->pit.rl[t] = 0xFFFF;
                        }
                        else
                        {
                                pas16->pit.rl[t] = (uint16_t)pas16->pit.c[t];
                                if (pas16->pit.c[t] > 65536) 
                                        pas16->pit.rl[t] = 0xFFFF;
                        }
                }
                switch (pas16->pit.rm[t])
                {
                        case 0:
                        temp = pas16->pit.rl[t] >> 8;
                        pas16->pit.rm[t] = 3;
                        pas16->pit.rereadlatch[t] = 1;
                        break;
                        case 1:
                        temp = (pas16->pit.rl[t]) & 0xFF;
                        pas16->pit.rereadlatch[t] = 1;
                        break;
                        case 2:
                        temp = (pas16->pit.rl[t]) >> 8;
                        pas16->pit.rereadlatch[t] = 1;
                        break;
                        case 3:
                        temp = (pas16->pit.rl[t]) & 0xFF;
                        if (pas16->pit.m[t] & 0x80) pas16->pit.m[t] &= 7;
                        else pas16->pit.rm[t] = 0;
                        break;
                }
                break;
                case 3: /*Control*/
                temp = pas16->pit.ctrl;
                break;
        }
        return temp;
}

static uint8_t pas16_readdma(pas16_t *pas16)
{
        return dma_channel_read(pas16->dma);
}

static void pas16_pcm_poll(priv_t priv)
{
        pas16_t *pas16 = (pas16_t *)priv;
        
        pas16_update(pas16);
        if (pas16->pit.m[0] & 2)
        {
                if (pas16->pit.l[0]) 
                        pas16->pit.c[0] += (tmrval_t) ((pas16->pit.l[0] * PITCONST * (1 << TIMER_SHIFT)));
                else                
                        pas16->pit.c[0] += (tmrval_t) ((0x10000 * PITCONST * (1 << TIMER_SHIFT)));
        }
        else
        {
                pas16->pit.c[0] = -1;
                pas16->pit.enable[0] = 0;
        }

        pas16->irq_stat |= PAS16_INT_SAMP;
        if (pas16->irq_ena & PAS16_INT_SAMP)
                picint(1 << pas16->irq);
        
        /*Update sample rate counter*/
        if (pas16->pit.enable[1])
        {               
                if (pas16->pcm_ctrl & PAS16_PCM_ENA)
                {
                        uint16_t temp;
                        
                        if (pas16->sys_conf_2 & PAS16_SC2_16BIT)
                        {
                                temp = pas16_readdma(pas16) << 8;
                                temp |= pas16_readdma(pas16);
                        }
                        else
                                temp = (pas16_readdma(pas16) ^ 0x80) << 8;
                        
                        if (pas16->sys_conf_2 & PAS16_SC2_MSBINV)
                                temp ^= 0x8000;
                        if (pas16->pcm_ctrl & PAS16_PCM_MONO)
                                pas16->pcm_dat_l = pas16->pcm_dat_r = temp;
                        else
                        {
                                if (pas16->stereo_lr)
                                        pas16->pcm_dat_r = temp;
                                else
                                        pas16->pcm_dat_l = temp;
                                        
                                pas16->stereo_lr = !pas16->stereo_lr;
                        }
                }
                if (pas16->sys_conf_2 & PAS16_SC2_16BIT)
                        pas16->pit.c[1] -= 2;
                else
                        pas16->pit.c[1]--;
                if (pas16->pit.c[1] == 0)
                {
                        if (pas16->pit.m[1] & 2)
                        {
                                if (pas16->pit.l[1]) 
                                        pas16->pit.c[1] += pas16->pit.l[1];
                                else                
                                        pas16->pit.c[1] += 0x10000;
                        }
                        else
                        {
                                pas16->pit.c[1] = -1;
                                pas16->pit.enable[1] = 0;
                        }
                
                        pas16->irq_stat |= PAS16_INT_PCM;
                        if (pas16->irq_ena & PAS16_INT_PCM)
                        {
                                DEBUG("pas16_pcm_poll : cause IRQ %i %02X\n", pas16->irq, 1 << pas16->irq);
                                picint(1 << pas16->irq);
                        }
                }
        }
}

static void pas16_out_base(uint16_t port, uint8_t val, priv_t priv)
{
        pas16_t *pas16 = (pas16_t *)priv;

        io_removehandler((pas16->base - 0x388) + 0x0388, 0x0004, pas16_in, NULL, NULL, pas16_out, NULL, NULL,  pas16);
        io_removehandler((pas16->base - 0x388) + 0x0788, 0x0004, pas16_in, NULL, NULL, pas16_out, NULL, NULL,  pas16);
        io_removehandler((pas16->base - 0x388) + 0x0b88, 0x0004, pas16_in, NULL, NULL, pas16_out, NULL, NULL,  pas16);
        io_removehandler((pas16->base - 0x388) + 0x0f88, 0x0004, pas16_in, NULL, NULL, pas16_out, NULL, NULL,  pas16);
        io_removehandler((pas16->base - 0x388) + 0x1388, 0x0004, pas16_in, NULL, NULL, pas16_out, NULL, NULL,  pas16);
        io_removehandler((pas16->base - 0x388) + 0x1788, 0x0004, pas16_in, NULL, NULL, pas16_out, NULL, NULL,  pas16);
        io_removehandler((pas16->base - 0x388) + 0x2788, 0x0004, pas16_in, NULL, NULL, pas16_out, NULL, NULL,  pas16);
        io_removehandler((pas16->base - 0x388) + 0x7f88, 0x0004, pas16_in, NULL, NULL, pas16_out, NULL, NULL,  pas16);
        io_removehandler((pas16->base - 0x388) + 0x8388, 0x0004, pas16_in, NULL, NULL, pas16_out, NULL, NULL,  pas16);
        io_removehandler((pas16->base - 0x388) + 0xbf88, 0x0004, pas16_in, NULL, NULL, pas16_out, NULL, NULL,  pas16);
        io_removehandler((pas16->base - 0x388) + 0xe388, 0x0004, pas16_in, NULL, NULL, pas16_out, NULL, NULL,  pas16);
        io_removehandler((pas16->base - 0x388) + 0xe788, 0x0004, pas16_in, NULL, NULL, pas16_out, NULL, NULL,  pas16);
        io_removehandler((pas16->base - 0x388) + 0xeb88, 0x0004, pas16_in, NULL, NULL, pas16_out, NULL, NULL,  pas16);
        io_removehandler((pas16->base - 0x388) + 0xef88, 0x0004, pas16_in, NULL, NULL, pas16_out, NULL, NULL,  pas16);
        io_removehandler((pas16->base - 0x388) + 0xf388, 0x0004, pas16_in, NULL, NULL, pas16_out, NULL, NULL,  pas16);
        io_removehandler((pas16->base - 0x388) + 0xf788, 0x0004, pas16_in, NULL, NULL, pas16_out, NULL, NULL,  pas16);
        io_removehandler((pas16->base - 0x388) + 0xfb88, 0x0004, pas16_in, NULL, NULL, pas16_out, NULL, NULL,  pas16);
        io_removehandler((pas16->base - 0x388) + 0xff88, 0x0004, pas16_in, NULL, NULL, pas16_out, NULL, NULL,  pas16);

        pas16->base = val << 2;
        DEBUG("pas16_write_base : PAS16 base now at %04X\n", pas16->base);

        io_sethandler((pas16->base - 0x388) + 0x0388, 0x0004, pas16_in, NULL, NULL, pas16_out, NULL, NULL,  pas16);
        io_sethandler((pas16->base - 0x388) + 0x0788, 0x0004, pas16_in, NULL, NULL, pas16_out, NULL, NULL,  pas16);
        io_sethandler((pas16->base - 0x388) + 0x0b88, 0x0004, pas16_in, NULL, NULL, pas16_out, NULL, NULL,  pas16);
        io_sethandler((pas16->base - 0x388) + 0x0f88, 0x0004, pas16_in, NULL, NULL, pas16_out, NULL, NULL,  pas16);
        io_sethandler((pas16->base - 0x388) + 0x1388, 0x0004, pas16_in, NULL, NULL, pas16_out, NULL, NULL,  pas16);
        io_sethandler((pas16->base - 0x388) + 0x1788, 0x0004, pas16_in, NULL, NULL, pas16_out, NULL, NULL,  pas16);
        io_sethandler((pas16->base - 0x388) + 0x2788, 0x0004, pas16_in, NULL, NULL, pas16_out, NULL, NULL,  pas16);
        io_sethandler((pas16->base - 0x388) + 0x7f88, 0x0004, pas16_in, NULL, NULL, pas16_out, NULL, NULL,  pas16);
        io_sethandler((pas16->base - 0x388) + 0x8388, 0x0004, pas16_in, NULL, NULL, pas16_out, NULL, NULL,  pas16);
        io_sethandler((pas16->base - 0x388) + 0xbf88, 0x0004, pas16_in, NULL, NULL, pas16_out, NULL, NULL,  pas16);
        io_sethandler((pas16->base - 0x388) + 0xe388, 0x0004, pas16_in, NULL, NULL, pas16_out, NULL, NULL,  pas16);
        io_sethandler((pas16->base - 0x388) + 0xe788, 0x0004, pas16_in, NULL, NULL, pas16_out, NULL, NULL,  pas16);
        io_sethandler((pas16->base - 0x388) + 0xeb88, 0x0004, pas16_in, NULL, NULL, pas16_out, NULL, NULL,  pas16);
        io_sethandler((pas16->base - 0x388) + 0xef88, 0x0004, pas16_in, NULL, NULL, pas16_out, NULL, NULL,  pas16);
        io_sethandler((pas16->base - 0x388) + 0xf388, 0x0004, pas16_in, NULL, NULL, pas16_out, NULL, NULL,  pas16);
        io_sethandler((pas16->base - 0x388) + 0xf788, 0x0004, pas16_in, NULL, NULL, pas16_out, NULL, NULL,  pas16);
        io_sethandler((pas16->base - 0x388) + 0xfb88, 0x0004, pas16_in, NULL, NULL, pas16_out, NULL, NULL,  pas16);
        io_sethandler((pas16->base - 0x388) + 0xff88, 0x0004, pas16_in, NULL, NULL, pas16_out, NULL, NULL,  pas16);
}


static void pas16_update(pas16_t *pas16)
{
        if (!(pas16->audiofilt & PAS16_FILT_MUTE))
        {
                for (; pas16->pos < sound_pos_global; pas16->pos++)
                {
                        pas16->pcm_buffer[0][pas16->pos] = 0;
                        pas16->pcm_buffer[1][pas16->pos] = 0;
                }
        }
        else
        {
                for (; pas16->pos < sound_pos_global; pas16->pos++)
                {
                        pas16->pcm_buffer[0][pas16->pos] = (int16_t)pas16->pcm_dat_l;
                        pas16->pcm_buffer[1][pas16->pos] = (int16_t)pas16->pcm_dat_r;
                }
        }
}

void pas16_get_buffer(int32_t *buffer, int len, priv_t priv)
{
        pas16_t *pas16 = (pas16_t *)priv;
        int c;

        opl3_update(&pas16->opl);
        sb_dsp_update(&pas16->dsp);
        pas16_update(pas16);
        for (c = 0; c < len * 2; c++)
        {
                buffer[c] += pas16->opl.buffer[c];
                buffer[c] += (int16_t)(sb_iir(c & 1, (float)pas16->dsp.buffer[c]) / 1.3) / 2;
                buffer[c] += (pas16->pcm_buffer[c & 1][c >> 1] / 2);
        }

        pas16->pos = 0;
        pas16->opl.pos = 0;
        pas16->dsp.pos = 0;
}


static priv_t
pas16_init(const device_t *info, UNUSED(void *parent))
{
        pas16_t *pas16 = (pas16_t *)mem_alloc(sizeof(pas16_t));
        memset(pas16, 0, sizeof(pas16_t));

        opl3_init(&pas16->opl);
        sb_dsp_init(&pas16->dsp, SB2);

        io_sethandler(0x9a01, 0x0001, NULL, NULL, NULL, pas16_out_base, NULL, NULL,  pas16);

        timer_add(pas16_pcm_poll, pas16,
		  &pas16->pit.c[0], &pas16->pit.enable[0]);

        sound_add_handler(pas16_get_buffer, pas16);

        return (priv_t)pas16;
}

static void pas16_close(priv_t priv)
{
        pas16_t *pas16 = (pas16_t *)priv;

        free(pas16);
}


const device_t pas16_device = {
    "Pro Audio Spectrum 16",
    DEVICE_ISA | DEVICE_UNSTABLE,
    0,
    NULL,
    pas16_init, pas16_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};
