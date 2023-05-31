/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of Bus Mouse devices.
 *
 *		These devices were made by both Microsoft and Logitech. At
 *		first, Microsoft used the same protocol as Logitech, but did
 *		switch to their new protocol for their InPort interface. So,
 *		although alike enough to be handled in the same driver, they
 *		are not the same.
 *
 * NOTES:	Ported from Bochs with extensive modifications per testing
 *		of the real hardware, testing of drivers, and the old code.
 *
 *		Logitech Bus Mouse verified with:
 *		  Logitech LMouse.com 3.12
 *		  Logitech LMouse.com 3.30
 *		  Logitech LMouse.com 3.41
 *		  Logitech LMouse.com 3.42
 *		  Logitech LMouse.com 4.00
 *		  Logitech LMouse.com 5.00
 *		  Logitech LMouse.com 6.00
 *		  Logitech LMouse.com 6.02 Beta
 *		  Logitech LMouse.com 6.02
 *		  Logitech LMouse.com 6.12
 *		  Logitech LMouse.com 6.20
 *		  Logitech LMouse.com 6.23
 *		  Logitech LMouse.com 6.30
 *		  Logitech LMouse.com 6.31E
 *		  Logitech LMouse.com 6.34
 *		  Logitech Mouse.exe 6.40
 *		  Logitech Mouse.exe 6.41
 *		  Logitech Mouse.exe 6.44
 *		  Logitech Mouse.exe 6.46
 *		  Logitech Mouse.exe 6.50
 *		  Microsoft Mouse.com 2.00
 *		  Microsoft Mouse.sys 3.00
 *		  Microsoft Mouse.com 7.04
 *		  Microsoft Mouse.com 8.21J
 *		  Microsoft Windows 1.00 DR5
 *		  Microsoft Windows 3.10.026
 *		  Microsoft Windows NT 3.1
 *		  Microsoft Windows 95
 *		  Linux kernel 1.2.13-ELF
 *
 *		InPort verified with:
 *		  Logitech LMouse.com 6.12
 *		  Logitech LMouse.com 6.41
 *		  Microsoft Windows NT 3.1
 *		  Microsoft Windows 98 SE
 *		  Linux kernel 1.2.13-ELF
 *
 * Version:	@(#)mouse_bus.c	1.1.12	2021/03/18
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2017-2021 Fred N. van Kempen.
 *		Copyright 2017,2019 Miran Grca.
 *		Copyright 200?-2018 Bochs.
 *
 *		Redistribution and  use  in source  and binary forms, with
 *		or  without modification, are permitted  provided that the
 *		following conditions are met:
 *
 *		1. Redistributions of  source  code must retain the entire
 *		   above notice, this list of conditions and the following
 *		   disclaimer.
 *
 *		2. Redistributions in binary form must reproduce the above
 *		   copyright  notice,  this list  of  conditions  and  the
 *		   following disclaimer in  the documentation and/or other
 *		   materials provided with the distribution.
 *
 *		3. Neither the  name of the copyright holder nor the names
 *		   of  its  contributors may be used to endorse or promote
 *		   products  derived from  this  software without specific
 *		   prior written permission.
 *
 * THIS SOFTWARE  IS  PROVIDED BY THE  COPYRIGHT  HOLDERS AND CONTRIBUTORS
 * "AS IS" AND  ANY EXPRESS  OR  IMPLIED  WARRANTIES,  INCLUDING, BUT  NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE  ARE  DISCLAIMED. IN  NO  EVENT  SHALL THE COPYRIGHT
 * HOLDER OR  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL,  EXEMPLARY,  OR  CONSEQUENTIAL  DAMAGES  (INCLUDING,  BUT  NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE  GOODS OR SERVICES;  LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED  AND ON  ANY
 * THEORY OF  LIABILITY, WHETHER IN  CONTRACT, STRICT  LIABILITY, OR  TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING  IN ANY  WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#define dbglog mouse_log
#include <86box/86box.h>
#include <86box/io.h>
#include <86box/timer.h>
#include <86box/device.h>
#include <86box/random.h>
#include <86box/plat.h>
#include <86box/pic.h>
#include <86box/mouse.h>


#define IRQ_MASK		((1 << 5) >> dev->irq)

/* Microsoft Inport Bus Mouse Adapter. */
#define INP_PORT_CONTROL	0
# define INP_CTRL_READ_BUTTONS	0x00
# define INP_CTRL_READ_X	0x01
# define INP_CTRL_READ_Y	0x02
# define INP_CTRL_COMMAND	0x07
# define INP_CTRL_RAISE_IRQ	0x16
# define INP_ENABLE_DATA_IRQ	0x08
# define INP_ENABLE_TIMER_IRQ	0x10
# define INP_HOLD_COUNTER	0x20
# define INP_CTRL_RESET		0x80

#define INP_PORT_DATA		1

#define INP_PORT_SIGNATURE	2

#define INP_PERIOD_MASK		0x07


/* Logitech/Microsoft Standard Bus Mouse Adapter. */
#define BUSM_PORT_DATA		0

#define BUSM_PORT_SIGNATURE	1

#define BUSM_PORT_CONTROL	2
# define CTRL_DISABLE_IRQ	(1 << 4)
# define CTRL_READ_LOW		(0 << 5)
# define CTRL_READ_HIGH		(1 << 5)
# define CTRL_READ_X		(0 << 6)
# define CTRL_READ_Y		(1 << 6)
# define CTRL_HOLD_COUNTER	(1 << 7)

# define CTRL_READ_X_LOW	(CTRL_READ_X | CTRL_READ_LOW)
# define CTRL_READ_X_HIGH	(CTRL_READ_X | CTRL_READ_HIGH)
# define CTRL_READ_Y_LOW	(CTRL_READ_Y | CTRL_READ_LOW)
# define CTRL_READ_Y_HIGH	(CTRL_READ_Y | CTRL_READ_HIGH)

#define BUSM_PORT_CONFIG	3
# define CONF_DEVICE_ACTIVE	0x80


/* Our mouse device. */
typedef struct mouse {
    const char	*name;

    uint16_t	base;
    int8_t	irq,
		bn;
    uint8_t	flags;
# define FLAG_INPORT	0x01
# define FLAG_ENABLED	0x02
# define FLAG_HOLD	0x04
# define FLAG_TIMER_INT	0x08
# define FLAG_DATA_INT	0x10

    int8_t	curr_x, curr_y;
    uint8_t	curr_b,
		buttons,
		buttons_last;

    uint8_t	ctrl, conf, sig,
		cmd, toggle;

    int		delayed_dx,
		delayed_dy;

    double	period;
    int64_t	timer,
		timer_enabled;
} mouse_t;


static const double periods[4] = { 30.0, 50.0, 100.0, 200.0 };


/* Handle a READ operation from one of the Logitech registers. */
static uint8_t
lt_read(uint16_t port, priv_t priv)
{
    mouse_t *dev = (mouse_t *)priv;
    uint8_t ret = 0xff;

    switch (port & 0x0003) {
	case BUSM_PORT_DATA:
		/*
		 * Testing and another source confirm that the
		 * buttons are *ALWAYS* present, so I'm going
		 * to change this a bit.
		 */
		switch (dev->ctrl & 0x60) {
			case CTRL_READ_X_LOW:
				ret = dev->curr_x & 0x0f;
				dev->curr_x &= 0xf0;
				break;

			case CTRL_READ_X_HIGH:
				ret = (dev->curr_x >> 4) & 0x0f;
				dev->curr_x &= 0x0f;
				break;

			case CTRL_READ_Y_LOW:
				ret = dev->curr_y & 0x0f;
				dev->curr_y &= 0xf0;
				break;

			case CTRL_READ_Y_HIGH:
				ret = (dev->curr_y >> 4) & 0x0f;
				dev->curr_y &= 0x0f;
				break;

			default:
				ERRLOG("MOUSE: READ data in mode %02x\n", dev->ctrl);
		}
		ret |= ((dev->curr_b ^ 7) << 5);
		break;

	case BUSM_PORT_SIGNATURE:
		ret = dev->sig;
		break;

	case BUSM_PORT_CONTROL:
		ret = dev->ctrl;
		dev->ctrl |= 0x0f;

		/*
		 * If the conditions are right, simulate the
		 * flakiness of the correct IRQ bit.
		 */
		if (dev->flags & FLAG_TIMER_INT)
			dev->ctrl = (dev->ctrl & ~IRQ_MASK) | \
				    (random_generate() & IRQ_MASK);
		break;

	case BUSM_PORT_CONFIG:
		/*
		 * Read from config port returns ctrl in the upper 4 bits
		 * when enabled, possibly solid interrupt readout in the
		 * lower 4 bits, 0xff when not (at power-up).
		 */
		if (dev->flags & FLAG_ENABLED)
			ret = (dev->ctrl | 0x0f) & ~IRQ_MASK;
		break;
    }

    DBGLOG(2, "MOUSE: read(%04x) = %02x\n", port, ret);

    return(ret);
}


/* Handle a WRITE operation to one of the Logitech registers. */
static void
lt_write(uint16_t port, uint8_t val, priv_t priv)
{
    mouse_t *dev = (mouse_t *)priv;
    uint8_t bit;

    DBGLOG(2, "MOUSE: write(%04x, %02x)\n", port, val);

    switch (port & 0x0003) {
	case BUSM_PORT_DATA:
		ERRLOG("MOUSE: R/O reg %04x write (value %02x)\n", port, val);
		break;

	case BUSM_PORT_SIGNATURE:
		dev->sig = val;
		break;

	case BUSM_PORT_CONTROL:
		dev->ctrl = val | 0x0f;
		if (! (val & CTRL_DISABLE_IRQ))
			dev->flags |= FLAG_TIMER_INT;
		else
			dev->flags &= ~FLAG_TIMER_INT;

		if (val & CTRL_HOLD_COUNTER)
			dev->flags |= FLAG_HOLD;
		  else
			dev->flags &= ~FLAG_HOLD;

		if (dev->irq != -1)
			picintc(1 << dev->irq);
		break;

	case BUSM_PORT_CONFIG:
		/*
		 * The original Logitech design was based on using a
		 * 8255 parallel I/O chip. This chip has to be set up
		 * for proper operation, and this configuration data
		 * is what is programmed into this register.
		 *
		 * A snippet of code found in the FreeBSD kernel source
		 * explains the value:
		 *
		 * D7    =  Mode set flag (1 = active)
		 *	    This indicates the mode of operation of D7:
		 *	    1 = Mode set, 0 = Bit set/reset
		 * D6,D5 =  Mode selection (port A)
		 *		00 = Mode 0 = Basic I/O
		 *		01 = Mode 1 = Strobed I/O
		 * 		10 = Mode 2 = Bi-dir bus
		 * D4    =  Port A direction (1 = input)
		 * D3    =  Port C (upper 4 bits) direction. (1 = input)
		 * D2    =  Mode selection (port B & C)
		 *		0 = Mode 0 = Basic I/O
		 *		1 = Mode 1 = Strobed I/O
		 * D1    =  Port B direction (1 = input)
		 * D0    =  Port C (lower 4 bits) direction. (1 = input)
		 *
		 * So 91 means Basic I/O on all 3 ports, Port A is an input
		 * port, B is an output port, C is split with upper 4 bits
		 * being an output port and lower 4 bits an input port, and
		 * enable the sucker.  Courtesy Intel 8255 databook. Lars
		 *
		 * 1001 1011	9B	1111	Default state
		 * 1001 0001	91	1001	Driver-initialized state
		 * The only difference is - port C upper and port B go from
		 * input to output.
		 */
		if (val & CONF_DEVICE_ACTIVE) {
			/* Mode set/reset - enable this */
			dev->conf = val;
			dev->flags |= (FLAG_ENABLED | FLAG_TIMER_INT);
			dev->ctrl = 0x0f & ~IRQ_MASK;
		} else {
			/* Single-bit set/reset */
			bit = 1 << ((val >> 1) & 0x07);		/* Bits 3-1 specify the target bit */
			if (val & 1)
				dev->ctrl |= bit;	/* Set */
			else
				dev->ctrl &= ~bit;	/* Reset */
		}
		break;
    }
}


/* Handle a READ operation from one of the MS InPort registers. */
static uint8_t
ms_read(uint16_t port, priv_t priv)
{
    mouse_t *dev = (mouse_t *)priv;
    uint8_t ret = 0xff;

    switch (port & 0x0003) {
	case INP_PORT_CONTROL:
		ret = dev->ctrl;
		break;

	case INP_PORT_DATA:
		switch (dev->cmd) {
			case INP_CTRL_READ_BUTTONS:
				ret = dev->curr_b;
				break;

			case INP_CTRL_READ_X:
				ret = dev->curr_x;
				dev->curr_x = 0;
				break;

			case INP_CTRL_READ_Y:
				ret = dev->curr_y;
				dev->curr_y = 0;
				break;

			case INP_CTRL_COMMAND:
				ret = dev->ctrl;
				break;

			default:
				ERRLOG("MOUSE: reading data port in unsupported mode %02x\n", dev->ctrl);
		}
		break;

	case INP_PORT_SIGNATURE:
		if (dev->toggle)
			ret = 0x12;
		else
			ret = 0xde;
		dev->toggle ^= 1;
		break;
    }

    DBGLOG(2, "MOUSE: read(%04x) = %02x\n", port, ret);

    return(ret);
}


/* Handle a WRITE operation to one of the MS InPort registers. */
static void
ms_write(uint16_t port, uint8_t val, priv_t priv)
{
    mouse_t *dev = (mouse_t *)priv;

    DBGLOG(2, "MOUSE: write(%04x, %02x)\n", port, val);

    switch (port & 0x0003) {
	case INP_PORT_CONTROL:
		/* Bit 7 is RESET. */
		if (val & INP_CTRL_RESET)
			dev->ctrl = 0x00;

		/* Bits 2:0 are the internal register index. */
		switch (val & 0x07) {
			case INP_CTRL_COMMAND:
			case INP_CTRL_READ_BUTTONS:
			case INP_CTRL_READ_X:
			case INP_CTRL_READ_Y:
				dev->cmd = val & 0x07;
				break;

			default:
				ERRLOG("MOUSE: write cmd %02x to port %04x\n", val, port);
		}
		break;

	case INP_PORT_DATA:
		if (dev->irq != -1)
			picintc(1 << dev->irq);

		switch (dev->cmd) {
			case INP_CTRL_COMMAND:
				if (val & INP_HOLD_COUNTER)
					dev->flags |= FLAG_HOLD;
				else
					dev->flags &= ~FLAG_HOLD;

				if (val & INP_ENABLE_TIMER_IRQ)
					dev->flags |= FLAG_TIMER_INT;
				else
					dev->flags &= ~FLAG_TIMER_INT;

				if (val & INP_ENABLE_DATA_IRQ)
					dev->flags |= FLAG_DATA_INT;
				else
					dev->flags &= ~FLAG_DATA_INT;

				switch (val & INP_PERIOD_MASK) {
					case 0:
						dev->period = 0.0;
						dev->timer = 0LL;
						dev->timer_enabled = 0LL;
						break;

					case 1:
					case 2:
					case 3:
					case 4:
						dev->period = 1000000.0 / periods[(val & INP_PERIOD_MASK) - 1];
						dev->timer = (int64_t)(dev->period * TIMER_USEC);
						dev->timer_enabled = (val & INP_ENABLE_TIMER_IRQ) ? 1LL : 0LL;
						DBGLOG(1, "MOUSE timer is now %sabled at period %i\n", (val & INP_ENABLE_TIMER_IRQ) ? "en" : "dis", (int32_t)dev->period);
						break;

					case 6:
						if (val & INP_ENABLE_TIMER_IRQ)
							if (dev->irq != -1)
								picint(1 << dev->irq);
						dev->ctrl &= INP_PERIOD_MASK;
						dev->ctrl |= (val & ~INP_PERIOD_MASK);
						break;

					default:
						ERRLOG("MOUSE: WRITE per %02 to %04x\n", port, val);
				}

				dev->ctrl = val;
				break;

			default:
				ERRLOG("MOUSE: WRITE %02x to port %04x\n", val, port);
		}
		break;

	case INP_PORT_SIGNATURE:
		ERRLOG("MOUSE: WRITE %02x to port %04x\n", val, port);
		break;
    }
}


/* The emulator calls us with an update on the host mouse device. */
static int
bm_poll(int x, int y, int z, int b, priv_t priv)
{
    mouse_t *dev = (mouse_t *)priv;
    int xor;

    /* Is the mouse even enabled? */
    if (! (dev->flags & FLAG_ENABLED)) return(1);

    /* Has its state changed? */
    if (!x && !y && !((b ^ dev->buttons_last) & 0x07)) {
	/* No, so clear the 'status changed' bits that are also in there. */
	dev->buttons_last = b;
	return(1);
    }

    /* Convert button states from MRL to LMR. */
    dev->buttons = (uint8_t) (((b & 1) << 2) | ((b & 2) >> 1));
    if (dev->bn == 3)
	dev->buttons |= ((b & 4) >> 1);

    if ((dev->flags & FLAG_INPORT) && !dev->timer_enabled) {
	/*
	 * This is an InPort mouse in data interrupt
	 * mode, so update bits 6-3 here.
	 *
	 * If the mouse has moved, set bit 6.
	 */
	if (x || y)
		dev->buttons |= 0x40;

	/* Set bits 3-5 according to button state changes. */
	xor = ((dev->curr_b ^ dev->buttons) & 0x07) << 3;
	dev->buttons |= xor;
    }

    dev->buttons_last = b;

    /* Clamp x and y to between -128 and 127 (int8_t range). */
    if (x > 127) x = 127;
    if (x < -128) x = -128;
    if (y > 127) y = 127;
    if (y < -128) y = -128;

    if (dev->timer_enabled) {
	/* Update delayed coordinates. */
	dev->delayed_dx += x;
	dev->delayed_dy += y;
    } else {
	/* If the counters are not frozen, update them. */
	if (! (dev->flags & FLAG_HOLD)) {
		dev->curr_x = (int8_t)x;
		dev->curr_y = (int8_t)y;
		dev->curr_b = dev->buttons;
	}

	/* Send interrupt. */
	if (dev->flags & FLAG_DATA_INT) {
		if (dev->irq != -1)
			picint(1 << dev->irq);
		DBGLOG(1, "MOUSE: Data Interrupt fired\n");
	}
    }

    return(1);
}


/*
 * Called at the configured period (InPort mouse) or at
 * 45 times per second (MS/Logitech Bus mouse).
 */
static void
bm_timer(priv_t priv)
{
    mouse_t *dev = (mouse_t *)priv;
    int delta_x, delta_y;
    int xor;

    DBGLOG(1, "MOUSE: Timer Tick (flags=%08x)\n", dev->flags);

    /*
     * The period is configured either via emulator settings
     * (for MS/Logitech Bus mouse) or via software (for
     * InPort mouse).
    */
    dev->timer += ((int64_t) dev->period) * TIMER_USEC;

    if (dev->flags & FLAG_TIMER_INT) {
	picint(1 << dev->irq);
	DBGLOG(1, "MOUSE: Timer Interrupt fired\n");
    }

    /*
     * Update the counters and deltas if the mouse is in timed mode.
     *
     * If the counters are not frozen, update them.
     */
    if (! (dev->flags & FLAG_HOLD)) {
	/* Grab as much 'delta data' as the guest will take (int8_t max.) */
	if (dev->delayed_dx > 127) {
		delta_x = 127;
		dev->delayed_dx -= 127;
	} else if (dev->delayed_dx < -128) {
		delta_x = -128;
		dev->delayed_dx += 128;
	} else {
		delta_x = dev->delayed_dx;
		dev->delayed_dx = 0;
	}

	if (dev->delayed_dy > 127) {
		delta_y = 127;
		dev->delayed_dy -= 127;
	} else if (dev->delayed_dy < -128) {
		delta_y = -128;
		dev->delayed_dy += 128;
	} else {
		delta_y = dev->delayed_dy;
		dev->delayed_dy = 0;
	}

	/* Make this chunk of data available to the guest. */
	dev->curr_x = (int8_t)delta_x;
	dev->curr_y = (int8_t)delta_y;
    } else {
	/* Frozen, so, no deltas to give. */
	delta_x = delta_y = 0;
    }

    if (dev->flags & FLAG_INPORT) {
	/*
	 * This is an InPort mouse in timer mode, so always update
	 * curr_b, and update bits 6:3 (mouse moved and button state
	 * changed) here.
	 */
	xor = ((dev->curr_b ^ dev->buttons) & 0x07) << 3;
	dev->curr_b = (dev->buttons & 0x87) | xor;
	if (delta_x || delta_y)
		dev->curr_b |= 0x40;
    } else if (! (dev->flags & FLAG_HOLD)) {
	/*
	 * This is a MS/Logitech Bus Mouse, so only
	 * update curr_b if the counters are frozen.
	 */
	dev->curr_b = dev->buttons;
    }
}


/* Release all resources held by the device. */
static void
bm_close(priv_t priv)
{
    mouse_t *dev = (mouse_t *)priv;

    if (dev != NULL)
	free(dev);
}


/* Initialize the device for use by the user. */
static priv_t
bm_init(const device_t *info, UNUSED(void *parent))
{
    mouse_t *dev;
    int hz;

    dev = (mouse_t *)mem_alloc(sizeof(mouse_t));
    memset(dev, 0x00, sizeof(mouse_t));
    dev->name = info->name;

    switch(info->local) {
	case 0:		/* original Logitech controller */
		dev->base = device_get_config_hex16("base");
		dev->irq = device_get_config_int("irq");
		break;

	case 1:		/* on-board controller, Logitech compatible */
		dev->base = 0x023c;
		dev->irq = -1;
		dev->bn = 2;
		break;

	case 10:	/* Microsoft InPort controller */
		dev->flags = FLAG_INPORT;
		dev->base = device_get_config_hex16("base");
		dev->irq = device_get_config_int("irq");
		break;

	case 11:	/* Microsoft InPort on-board controller */
		dev->flags = FLAG_INPORT;
		dev->base = 0x023c;
		dev->bn = 2;
		dev->irq = -1;
		break;
    }

    if (dev->bn == 0)
	dev->bn = device_get_config_int("buttons");
    mouse_set_buttons(dev->bn);

    dev->timer_enabled = 0;
    dev->timer = 0LL;

    dev->delayed_dx = 0;
    dev->delayed_dy = 0;
    dev->buttons = 0;
    dev->buttons_last = 0;
    dev->curr_x = 0;
    dev->curr_y = 0;
    dev->curr_b = 0;

    dev->sig = 0;	/* the signature port value */
    dev->cmd = 0;	/* command byte */
    dev->toggle = 0;	/* signature byte / IRQ bit toggle */
    if (dev->flags & FLAG_INPORT) {
	dev->ctrl = 0;	/* the control port value */
	dev->flags |= FLAG_ENABLED;
	dev->period = 0.0;

	dev->timer = 0LL;
	dev->timer_enabled = 0LL;

	io_sethandler(dev->base, 3,
		      ms_read,NULL,NULL, ms_write,NULL,NULL, dev);
    } else {
	dev->ctrl = 0x0f;	/* the control port value */
	dev->conf = 0x9b;	/* the config port value - 0x9b is the
				   default state of the 8255: all ports
				   are set to input */

	hz = device_get_config_int("hz");
	if (hz > 0) {
		dev->period = 1000000.0 / (double)hz;

		dev->timer = ((int64_t) dev->period) * TIMER_USEC;
		dev->timer_enabled = 1LL;
	} else
		dev->flags |= FLAG_DATA_INT;

	io_sethandler(dev->base, 4,
		      lt_read,NULL,NULL, lt_write,NULL,NULL, dev);
    }

    timer_add(bm_timer, dev, &dev->timer, &dev->timer_enabled);

    INFO("MOUSE: %s (I/O=%04x, IRQ=%i, buttons=%i\n",
		dev->name, dev->base, dev->irq, dev->bn);

    return((priv_t)dev);
}


static const device_config_t lt_config[] = {
    {
	"base", "Address", CONFIG_HEX16, "", 0x023c,
	{
		{
			"230H", 0x0230
		},
		{
			"234H", 0x0234
		},
		{
			"238H", 0x0238
		},
		{
			"23CH", 0x023c
		},
		{
			NULL
		}
	}
    },
    {
	"irq", "IRQ", CONFIG_SELECTION, "", 5,
	{
		{
			"IRQ 2", 2
		},
		{
			"IRQ 3", 3
		},
		{
			"IRQ 4", 4
		},
		{
			"IRQ 5", 5
		},
		{
			NULL
		}
	}
    },
    {
	"hz", "Hz", CONFIG_SELECTION, "", 45,
	{
		{
			"Original Mode", 0
		},
		{
			"45 Hz (JMP2 not populated)", 45
		},
		{
			"30 Hz (JMP2 = 1)", 30
		},
		{
			"60 Hz (JMP 2 = 2)", 60
		},
		{
			NULL
		}
	}
    },
    {
	"buttons", "Buttons", CONFIG_SELECTION, "", 2,
	{
		{
			"Two", 2
		},
		{
			"Three", 3
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


static const device_config_t ms_config[] = {
    {
	"base", "Address", CONFIG_HEX16, "", 0x023c,
	{
		{
			"230H", 0x0230
		},
		{
			"234H", 0x0234
		},
		{
			"238H", 0x0238
		},
		{
			"23CH", 0x023c
		},
		{
			NULL
		}
	}
    },
    {
	"irq", "IRQ", CONFIG_SELECTION, "", 5,
	{
		{
			"IRQ 2", 2
		},
		{
			"IRQ 3", 3
		},
		{
			"IRQ 4", 4
		},
		{
			"IRQ 5", 5
		},
		{
			NULL
		}
	}
    },
    {
	"buttons", "Buttons", CONFIG_SELECTION, "", 2,
	{
		{
			"Two", 2
		},
		{
			"Three", 3
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


const device_t mouse_logibus_device = {
    "Logitech Bus Mouse",
    DEVICE_ISA,
    0,
    NULL,
    bm_init, bm_close, NULL,
    bm_poll,
    NULL, NULL, NULL,
    lt_config
};

const device_t mouse_logibus_onboard_device = {
    "Logitech Bus Mouse (Internal)",
    0,
    1,
    NULL,
    bm_init, bm_close, NULL,
    bm_poll,
    NULL, NULL, NULL,
    NULL
};

const device_t mouse_msinport_device = {
    "Microsoft Bus Mouse (InPort)",
    DEVICE_ISA,
    10,
    NULL,
    bm_init, bm_close, NULL,
    bm_poll,
    NULL, NULL, NULL,
    ms_config
};

const device_t mouse_msinport_onboard_device = {
    "Microsoft InPort Mouse (Internal)",
    0,
    11,
    NULL,
    bm_init, bm_close, NULL,
    bm_poll,
    NULL, NULL, NULL,
    NULL
};


/*
 * *** THIS IS A TEMPORARY FUNCTION ***
 *
 * Allows setting of the mouse device's IRQ value,
 * currently needed for onboard devices.
 */
void
mouse_bus_set_irq(priv_t priv, int irq)
{
    mouse_t *dev = (mouse_t *)priv;

    DBGLOG(1, "MOUSE: Set_IRQ(%i)\n", irq);

    dev->irq = irq;
}
