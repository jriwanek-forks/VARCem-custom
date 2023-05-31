/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the SiS 85C496/497 based machines.
 *
 * Version:	@(#)m_sis49x.c	1.0.19	2021/03/23
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		Sarah Walker, <tommowalker@tommowalker.co.uk>
 *
 *		Copyright 2017-2021 Fred N. van Kempen.
 *		Copyright 2016-2021 Miran Grca.
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
#include <86box/86box.h>
#include "../cpu/cpu.h"
#include <86box/io.h>
#include <86box/device.h>
#include <86box/mem.h>
#include <86box/rom.h>
#include <86box/sis496.h>
#include <86box/pci.h>
#include <86box/memregs.h>
#include <86box/sio.h>
#include <86box/fdd.h>
#include <86box/fdc.h>
#include <86box/hdc.h>
#include <86box/keyboard.h>
#include <86box/machine.h>


static priv_t
common_init(const device_t *info, void *arg)
{
    /* Add machine device to system. */
    device_add_ex(info, (priv_t)arg);

    switch (info->local) {
	case 0:		/* Lucky Star LS-486E */
		pci_init(PCI_CONFIG_TYPE_1);
		pci_register_slot(0x05, PCI_CARD_SPECIAL, 0, 0, 0, 0);
		pci_register_slot(0x0B, PCI_CARD_NORMAL, 1, 2, 3, 4);
		pci_register_slot(0x0D, PCI_CARD_NORMAL, 2, 3, 4, 1);
		pci_register_slot(0x0F, PCI_CARD_NORMAL, 3, 4, 1, 2);
		pci_register_slot(0x06, PCI_CARD_NORMAL, 4, 1, 2, 3);

		pci_set_irq_routing(PCI_INTA, PCI_IRQ_DISABLED);
		pci_set_irq_routing(PCI_INTB, PCI_IRQ_DISABLED);
		pci_set_irq_routing(PCI_INTC, PCI_IRQ_DISABLED);
		pci_set_irq_routing(PCI_INTD, PCI_IRQ_DISABLED);

		device_add(&memregs_device);
		device_add(&sis_85c496_device);
		m_at_common_ide_init();
		device_add(&keyboard_ps2_ami_pci_device);
		device_add(&fdc_at_device);
		device_add(&fdc37c665_device);
		break;

	case 1:		/* Lucky Star 4C-1 */
		pci_init(PCI_CONFIG_TYPE_1);
		pci_register_slot(0x05, PCI_CARD_SPECIAL, 0, 0, 0, 0);
		pci_register_slot(0x0B, PCI_CARD_NORMAL, 1, 2, 3, 4);
		pci_register_slot(0x0D, PCI_CARD_NORMAL, 2, 3, 4, 1);
		pci_register_slot(0x0F, PCI_CARD_NORMAL, 3, 4, 1, 2);

		pci_set_irq_routing(PCI_INTA, PCI_IRQ_DISABLED);
		pci_set_irq_routing(PCI_INTB, PCI_IRQ_DISABLED);
		pci_set_irq_routing(PCI_INTC, PCI_IRQ_DISABLED);
		pci_set_irq_routing(PCI_INTD, PCI_IRQ_DISABLED);

		device_add(&memregs_device);
		device_add(&sis_85c496_device);
		m_at_common_ide_init();
		device_add(&keyboard_at_device);
		device_add(&w83787f_device);
		break;

	case 2:		/* Rise 418 */
		pci_init(PCI_CONFIG_TYPE_1);
		pci_register_slot(0x05, PCI_CARD_SPECIAL, 0, 0, 0, 0);
		pci_register_slot(0x0B, PCI_CARD_NORMAL, 1, 2, 3, 4);
		pci_register_slot(0x0D, PCI_CARD_NORMAL, 2, 3, 4, 1);
		pci_register_slot(0x0F, PCI_CARD_NORMAL, 3, 4, 1, 2);
		pci_register_slot(0x07, PCI_CARD_NORMAL, 4, 1, 2, 3);

		pci_set_irq_routing(PCI_INTA, PCI_IRQ_DISABLED);
		pci_set_irq_routing(PCI_INTB, PCI_IRQ_DISABLED);
		pci_set_irq_routing(PCI_INTC, PCI_IRQ_DISABLED);
		pci_set_irq_routing(PCI_INTD, PCI_IRQ_DISABLED);

		device_add(&sis_85c496_device);
		device_add(&memregs_eb_device);
		m_at_common_init();
		device_add(&keyboard_ps2_pci_device);
		device_add(&ide_pci_2ch_device);
		device_add(&fdc37c665_device);
		break;
    }

    return(arg);
}


static const machine_t ami_info = {
    MACHINE_PCI | MACHINE_ISA | MACHINE_AT | MACHINE_HDC,
    0,
    1, 128, 1, 128, 0,
    {{"Intel",cpus_i486},{"AMD",cpus_Am486},{"Cyrix",cpus_Cx486}}
};

const device_t m_sis496_ami = {
    "Lucky Star LS-486E (Sis496, AMI)",
    DEVICE_ROOT,
    0,
    L"sis/sis496/ami",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &ami_info,
    NULL
};


static const machine_t award_info = {
    MACHINE_PCI | MACHINE_ISA | MACHINE_AT | MACHINE_HDC,
    0,
    1, 128, 1, 128, 0,
    {{"Intel",cpus_i486},{"AMD",cpus_Am486},{"Cyrix",cpus_Cx486}}
};

const device_t m_sis496_award = {
    "Lucky Star LS-486E Rev C1 (Sis496, Award)",
    DEVICE_ROOT,
    1,
    L"sis/sis496/award",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &award_info,
    NULL
};


static const machine_t rise_info = {
    MACHINE_PCI | MACHINE_ISA | MACHINE_AT | MACHINE_HDC,
    0,
    1, 256, 1, 128, 0,
    {{"Intel",cpus_i486},{"AMD",cpus_Am486},{"Cyrix",cpus_Cx486}}
};

const device_t m_rise418 = {
    "Rise 418 (SiS 496)",
    DEVICE_ROOT,
    2,
    L"rise/r418",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &rise_info,
    NULL
};
