/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the Opti 82C495 based machines.
 *
 * Version:	@(#)m_opti8xx.c	1.0.3	2021/03/20
 *
 * Author:	Altheos, <altheos@varcem.com>
 *
 *		Copyright 2021 Altheos.
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
#include <wchar.h>
#include <86box/86box.h>
#include "../cpu/cpu.h"
#include <86box/io.h>
#include <86box/mem.h>
#include <86box/rom.h>
#include <86box/device.h>
#include <86box/sio.h>
#include <86box/memregs.h>
#include <86box/opti895.h>
#include <86box/keyboard.h>
#include <86box/fdd.h>
#include <86box/fdc.h>
#include <86box/machine.h>
#include <86box/apm.h>


static priv_t
common_init(const device_t *info, void *arg)
{
    /* Add machine device to system. */
    device_add_ex(info, (priv_t)arg);

    device_add(&opti895_device);

    device_add(&fdc_at_device);
    
    switch (info->local) { // DataExpert EXP4044
	case 0:		
		m_at_common_init(); 
		device_add(&keyboard_at_device);  // Winbond W83C42
		break;
		
	case 1:		// HOT-419 - AMIKEY-2
		m_at_common_init();
		device_add(&keyboard_at_ami_device);
		device_add(&memregs_device);
		break;
    }

    return(arg);
}


static const machine_t hot419_info = {
    MACHINE_ISA | MACHINE_VLB | MACHINE_AT,
    0,
    1, 128, 1, 128, 0,
    {{"Intel",cpus_i486},{"AMD",cpus_Am486},{"Cyrix",cpus_Cx486}} /* Note : this board only supports 5V CPU */
};

const device_t m_opti895_hot419 = {
    "Shuttle Hot-419",
    DEVICE_ROOT,
    1,
    L"opti/opti8xx/shuttle",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &hot419_info,
    NULL
};


static const machine_t opti8xx_info = {
    MACHINE_ISA | MACHINE_VLB | MACHINE_AT,
    0,
    1, 64, 1, 128, 0,
    {{"Intel",cpus_i486},{"AMD",cpus_Am486},{"Cyrix",cpus_Cx486}} /* Supports 3.3 and 5V CPU */
};

const device_t m_opti895_dp4044 = {
    "DataExpert EXP4044",
    DEVICE_ROOT,
    0,
    L"opti/opti8xx/dataexpert",
    common_init, NULL, NULL,
    NULL, NULL, NULL,
    &opti8xx_info,
    NULL
};
