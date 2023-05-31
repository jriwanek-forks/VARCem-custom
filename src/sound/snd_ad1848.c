/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Emulation of the AD1848 (Windows Sound System) CODEC.
 *
 * Version:	@(#)snd_ad1848.c	1.0.8	2019/05/17
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
#include <math.h>
#define dbglog sound_card_log
#include <86box/86box.h>
#include <86box/timer.h>
#include <86box/dma.h>
#include <86box/pic.h>
#include <86box/sound.h>
#include <86box/snd_ad1848.h>


static int ad1848_vols[64];


void ad1848_setirq(ad1848_t *ad1848, int irq_ch)
{
        ad1848->irq = irq_ch;
}

void ad1848_setdma(ad1848_t *ad1848, int dma_ch)
{
        ad1848->dma = dma_ch;
}

uint8_t ad1848_read(uint16_t addr, priv_t priv)
{
        ad1848_t *ad1848 = (ad1848_t *)priv;
        uint8_t temp = 0xff;
        switch (addr & 3)
        {
                case 0: /*Index*/
                temp = ad1848->index | ad1848->trd | ad1848->mce;
                break;
                case 1:
                temp = ad1848->regs[ad1848->index];
                break;
                case 2:
                temp = ad1848->status;
                break;
        }
        return temp;
}

void ad1848_write(uint16_t addr, uint8_t val, priv_t priv)
{
        ad1848_t *ad1848 = (ad1848_t *)priv;
        double freq;
        switch (addr & 3)
        {
                case 0: /*Index*/
                ad1848->index = val & 0xf;
                ad1848->trd   = val & 0x20;
                ad1848->mce   = val & 0x40;
                break;
                case 1:
                switch (ad1848->index)
                {
                        case 8:
                        freq = (double) ((val & 1) ? 16934400LL : 24576000LL);
                        switch ((val >> 1) & 7)
                        {
                                case 0: freq /= 3072; break;
                                case 1: freq /= 1536; break;
                                case 2: freq /= 896;  break;
                                case 3: freq /= 768;  break;
                                case 4: freq /= 448;  break;
                                case 5: freq /= 384;  break;
                                case 6: freq /= 512;  break;
                                case 7: freq /= 2560; break;
                        }
                        ad1848->freq = (int64_t)freq;
                        ad1848->timer_latch = (int64_t)((double)TIMER_USEC * (1000000.0 / (double)ad1848->freq));
                        break;
                        
                        case 9:
                        ad1848->enable = ((val & 0x41) == 0x01);
                        if (!ad1848->enable)
                                ad1848->out_l = ad1848->out_r = 0;
                        break;
                                
                        case 12:
                        return;
                        
                        case 14:
                        ad1848->count = ad1848->regs[15] | (val << 8);
                        break;
                }
                ad1848->regs[ad1848->index] = val;
                break;
                case 2:
                ad1848->status &= 0xfe;
                break;              
        }
}

void ad1848_speed_changed(ad1848_t *ad1848)
{
        ad1848->timer_latch = (int64_t)((double)TIMER_USEC * (1000000.0 / (double)ad1848->freq));
}

void ad1848_update(ad1848_t *ad1848)
{
        for (; ad1848->pos < sound_pos_global; ad1848->pos++)
        {
                ad1848->buffer[ad1848->pos*2]     = ad1848->out_l;
                ad1848->buffer[ad1848->pos*2 + 1] = ad1848->out_r;
        }
}

static void ad1848_poll(priv_t priv)
{
        ad1848_t *ad1848 = (ad1848_t *)priv;
 
        if (ad1848->timer_latch)
                ad1848->timer_count += ad1848->timer_latch;
        else
                ad1848->timer_count = TIMER_USEC;
        
        ad1848_update(ad1848);
        
        if (ad1848->enable)
        {
                int32_t temp;
                
                switch (ad1848->regs[8] & 0x70)
                {
                        case 0x00: /*Mono, 8-bit PCM*/
                        ad1848->out_l = ad1848->out_r = (dma_channel_read(ad1848->dma) ^ 0x80) * 256;
                        break;
                        case 0x10: /*Stereo, 8-bit PCM*/
                        ad1848->out_l = (dma_channel_read(ad1848->dma) ^ 0x80)  * 256;
                        ad1848->out_r = (dma_channel_read(ad1848->dma) ^ 0x80)  * 256;
                        break;
                
                        case 0x40: /*Mono, 16-bit PCM*/
                        temp = dma_channel_read(ad1848->dma);
                        ad1848->out_l = ad1848->out_r = (dma_channel_read(ad1848->dma) << 8) | temp;
                        break;
                        case 0x50: /*Stereo, 16-bit PCM*/
                        temp = dma_channel_read(ad1848->dma);
                        ad1848->out_l = (dma_channel_read(ad1848->dma) << 8) | temp;
                        temp = dma_channel_read(ad1848->dma);
                        ad1848->out_r = (dma_channel_read(ad1848->dma) << 8) | temp;
                        break;
                }

                if (ad1848->regs[6] & 0x80)
                        ad1848->out_l = 0;
                else
                        ad1848->out_l = (ad1848->out_l * ad1848_vols[ad1848->regs[6] & 0x3f]) >> 16;

                if (ad1848->regs[7] & 0x80)
                        ad1848->out_r = 0;
                else
                        ad1848->out_r = (ad1848->out_r * ad1848_vols[ad1848->regs[7] & 0x3f]) >> 16;
                
                if (ad1848->count < 0)
                {
                        ad1848->count = ad1848->regs[15] | (ad1848->regs[14] << 8);
                        if (!(ad1848->status & 0x01))
                        {
                                ad1848->status |= 0x01;
                                if (ad1848->regs[0xa] & 2)
                                        picint(1 << ad1848->irq);
                        }                                
                }

                ad1848->count--;
        }
        else
        {
                ad1848->out_l = ad1848->out_r = 0;
        }
}

void ad1848_init(ad1848_t *ad1848)
{
        int c;
        double attenuation;

        ad1848->enable = 0;
                        
        ad1848->status = 0xcc;
        ad1848->index = ad1848->trd = 0;
        ad1848->mce = 0x40;
        
        ad1848->regs[0] = ad1848->regs[1] = 0;
        ad1848->regs[2] = ad1848->regs[3] = 0x80;
        ad1848->regs[4] = ad1848->regs[5] = 0x80;
        ad1848->regs[6] = ad1848->regs[7] = 0x80;
        ad1848->regs[8] = 0;
        ad1848->regs[9] = 0x08;
        ad1848->regs[10] = ad1848->regs[11] = 0;
        ad1848->regs[12] = 0xa;
        ad1848->regs[13] = 0;
        ad1848->regs[14] = ad1848->regs[15] = 0;
        
        ad1848->out_l = 0;
        ad1848->out_r = 0;
        
        for (c = 0; c < 64; c++)
        {
                attenuation = 0.0;
                if (c & 0x01) attenuation -= 1.5;
                if (c & 0x02) attenuation -= 3.0;
                if (c & 0x04) attenuation -= 6.0;
                if (c & 0x08) attenuation -= 12.0;
                if (c & 0x10) attenuation -= 24.0;
                if (c & 0x20) attenuation -= 48.0;
                
                attenuation = pow(10, attenuation / 10);
                
                ad1848_vols[c] = (int)(attenuation * 65536);
        }
        
        timer_add(ad1848_poll, ad1848, &ad1848->timer_count, &ad1848->enable);
}
