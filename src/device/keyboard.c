/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		General keyboard driver interface.
 *
 * Version:	@(#)keyboard.c	1.0.14	2019/04/26
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
#include <stdarg.h>
#include <wchar.h>
#define HAVE_STDARG_H
#define dbglog kbd_log
#include <86box/86box.h>
#include <86box/plat.h>
#include <86box/ui.h>
#include <86box/keyboard.h>


#ifdef ENABLE_KEYBOARD_LOG
int	keyboard_do_log = ENABLE_KEYBOARD_LOG;
#endif
int64_t	keyboard_delay;
int	keyboard_scan;
void	(*keyboard_send)(uint16_t val);


static int	recv_key[512];		/* keyboard input buffer */
static int	oldkey[512];
static const scancode_t	*scan_table;	/* scancode table for keyboard */

static uint8_t	caps_lock = 0;
static uint8_t	num_lock = 0;
static uint8_t	scroll_lock = 0;
static uint8_t  shift = 0;


#ifdef _LOGGING
void
kbd_log(int level, const char *fmt, ...)
{
# ifdef ENABLE_KEYBOARD_LOG
    va_list ap;

    if (keyboard_do_log >= level) {
	va_start(ap, fmt);
	pclog_ex(fmt, ap);
	va_end(ap);
    }
# endif
}
#endif


static uint8_t
fake_shift_needed(uint16_t scan)
{
    switch(scan) {
	case 0x0147:
	case 0x0148:
	case 0x0149:
	case 0x014a:
	case 0x014b:
	case 0x014d:
	case 0x014f:
	case 0x0150:
	case 0x0151:
	case 0x0152:
	case 0x0153:
		return(1);

	default:
		return(0);
    }
}


static void
key_process(uint16_t scan, int down)
{
    const scancode_t *codes = scan_table;
    int c;

    if (! keyboard_scan) return;

    oldkey[scan] = down;
    if (down && codes[scan].mk[0] == 0)
	return;

    if (!down && codes[scan].brk[0] == 0)
	return;

    if (AT && ((keyboard_mode & 3) == 3)) {
	if (!keyboard_set3_all_break && !down && !(keyboard_set3_flags[codes[scan].mk[0]] & 2))
		return;
    }

    c = 0;
    if (down) {
	/* Send the special code indicating an opening fake shift might be needed. */
	if (fake_shift_needed(scan))
		keyboard_send(0x100);
	while (codes[scan].mk[c] != 0)
		keyboard_send(codes[scan].mk[c++]);
    } else {
	while (codes[scan].brk[c] != 0)
		keyboard_send(codes[scan].brk[c++]);
	/* Send the special code indicating a closing fake shift might be needed. */
	if (fake_shift_needed(scan))
		keyboard_send(0x101);
    }
}


/* Insert keystrokes into the machine's keyboard buffer. */
static void
keyboard_send_scan(uint8_t val)
{
    if (AT)
	keyboard_at_adddata_keyboard_raw(val);
      else
	keyboard_send(val);
}


/* Send the machine a Control-Alt sequence. */
static void
keyboard_ca(uint8_t sc)
{
    keyboard_send_scan(29);		/* Ctrl key pressed */
    keyboard_send_scan(56);		/* Alt key pressed */

    keyboard_send_scan(sc);		/* press */
    keyboard_send_scan(sc | 0x80);	/* release */

    keyboard_send_scan(184);		/* Alt key released */
    keyboard_send_scan(157);		/* Ctrl key released */
}


/* Reset the keyboard driver. */
void
keyboard_reset(void)
{
    keyboard_scan = 1;
    keyboard_delay = 0;

    memset(recv_key, 0x00, sizeof(recv_key));

    memset(keyboard_set3_flags, 0x00, sizeof(keyboard_set3_flags));
    keyboard_set3_all_repeat = 0;
    keyboard_set3_all_break = 0;
}


void
keyboard_set_table(const scancode_t *ptr)
{
    scan_table = ptr;
}


int
keyboard_recv(uint16_t key)
{
    return(recv_key[key]);
}


/* Handle a keystroke event from the UI layer. */
void
keyboard_input(int down, uint16_t scan)
{
    int uiflag = 0;

    /* Translate E0 xx scan codes to 01xx because we use 512-byte arrays for states
       and scan code sets. */
    if ((scan >> 8) == 0xe0) {
	scan &= 0x00ff;
	scan |= 0x0100;		/* extended key code */
    } else if ((scan >> 8) != 0x01) {
	/*
	 * we can receive a scan code whose upper byte is 0x01,
	 * this means we're the Win32 version running on windows
	 * that already sends us preprocessed scan codes, which
	 * means we then use the scan code as is, and need to
	 * make sure we do not accidentally strip that upper byte.
	 */
	scan &= 0x00ff;
    }

    if (recv_key[scan & 0x01ff] ^ down) {
	if (down) {
		switch(scan & 0x01ff) {
			case 0x001c:	/* Left Ctrl */
				shift |= 0x01;
				break;

			case 0x011c:	/* Right Ctrl */
				shift |= 0x10;
				break;

			case 0x002a:	/* Left Shift */
				shift |= 0x02;
				break;

			case 0x0036:	/* Right Shift */
				shift |= 0x20;
				break;

			case 0x0038:	/* Left Alt */
				shift |= 0x04;
				break;

			case 0x0138:	/* Right Alt */
				shift |= 0x40;
				break;
		}
	} else {
		switch(scan & 0x01ff) {
			case 0x001c:	/* Left Ctrl */
				shift &= ~0x01;
				break;

			case 0x011c:	/* Right Ctrl */
				shift &= ~0x10;
				break;

			case 0x002a:	/* Left Shift */
				shift &= ~0x02;
				break;

			case 0x0036:	/* Right Shift */
				shift &= ~0x20;
				break;

			case 0x0038:	/* Left Alt */
				shift &= ~0x04;
				break;

			case 0x0138:	/* Right Alt */
				shift &= ~0x40;
				break;

			case 0x003a:	/* Caps Lock */
				caps_lock ^= 1;
				uiflag++;
				break;

			case 0x0045:
				num_lock ^= 1;
				uiflag++;
				break;

			case 0x0046:
				scroll_lock ^= 1;
				uiflag++;
				break;
		}
	}
    }

    if (uiflag) {
	/* One of the toggle keys changed, update UI. */
	uiflag = 0;
	if (caps_lock)
		uiflag |= KBD_FLAG_CAPS;
	if (num_lock)
		uiflag |= KBD_FLAG_NUM;
	if (scroll_lock)
		uiflag |= KBD_FLAG_SCROLL;

	/* Update UI and possibly host. */
	ui_set_kbd_state(uiflag);
    }

    DEBUG("Received scan code: %03X (%s)\n",scan & 0x1ff, down ? "down" : "up");
    recv_key[scan & 0x01ff] = down;
    key_process(scan & 0x01ff, down);
}


void
keyboard_adddata(uint16_t val, void (*add_data)(uint16_t val))
{
    uint8_t shift_states;

    if (add_data == NULL) return;

    shift_states = shift & STATE_SHIFT_MASK;

    switch(val) {
	case FAKE_LSHIFT_ON:
		if (num_lock) {
			if (! shift_states) {
				/* Num lock on and no shifts are pressed, send non-inverted fake shift. */
				add_data(0x2a);
			}
		} else {
			if (shift_states & STATE_LSHIFT) {
				/* Num lock off and left shift pressed. */
				add_data(0xaa);
			}
			if (shift_states & STATE_RSHIFT) {
				/* Num lock off and right shift pressed. */
				add_data(0xb6);
			}
		}
		break;

	case FAKE_LSHIFT_OFF:
		if (num_lock) {
			if (! shift_states) {
				/* Num lock on and no shifts are pressed, send non-inverted fake shift. */
				add_data(0xaa);
			}
		} else {
			if (shift_states & STATE_LSHIFT) {
				/* Num lock off and left shift pressed. */
				add_data(0x2a);
			}
			if (shift_states & STATE_RSHIFT) {
				/* Num lock off and right shift pressed. */
				add_data(0x36);
			}
		}
		break;

	default:
		add_data(val);
		break;
    }
}


uint8_t
keyboard_get_shift(void)
{
    return(shift);
}


uint8_t
keyboard_get_state(void)
{
    uint8_t ret = 0x00;

    if (caps_lock)
	ret |= KBD_FLAG_CAPS;
    if (num_lock)
	ret |= KBD_FLAG_NUM;
    if (scroll_lock)
	ret |= KBD_FLAG_SCROLL;

    return(ret);
}


/* Send the machine a Control-Alt-DEL sequence. */
void
keyboard_cad(void)
{
    keyboard_ca(83);			/* Delete */
}


/* Send the machine a Control-Alt-ESC sequence. */
void
keyboard_cae(void)
{
    keyboard_ca(1);			/* Esc */
}


/* Send the machine a Control-Alt-Break sequence. */
void
keyboard_cab(void)
{
    keyboard_ca(1);			/* Break (FIXME: Esc) */
}


/* Do we have Control-Alt-PgDn in the keyboard buffer? */
int
keyboard_isfsexit(void)
{
    return( (recv_key[0x001d] || recv_key[0x011d]) &&
	    (recv_key[0x0038] || recv_key[0x0138]) &&
	    (recv_key[0x0051] || recv_key[0x0151]) );
}


/* Do we have F8-F12 in the keyboard buffer? */
int
keyboard_ismsexit(void)
{
#ifdef _WIN32
    /* Windows: F8+F12 */
    return( recv_key[0x042] && recv_key[0x058] );
#else
    /* WxWidgets cannot do two regular keys.. CTRL+END */
    return( (recv_key[0x001d] || recv_key[0x011d]) &&
	    (recv_key[0x004f] || recv_key[0x014f]) );
#endif
}
