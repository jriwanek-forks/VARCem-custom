/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the NCR 5380 series of SCSI Host Adapters
 *		made by NCR. These controllers were designed for the ISA bus.
 *
 * NOTE:	This code now only supports targets at LUN=0 !!
 *
 * Version:	@(#)scsi_ncr5380.c	1.0.19	2019/05/17
 *
 * Authors:	Fred N. van Kempen, <decwiz@yahoo.com>
 *		Miran Grca, <mgrca8@gmail.com>
 *		TheCollector1995, <mariogplayer@gmail.com>
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
#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#define dbglog scsi_card_log
#include <86box/86box.h>
#include <86box/timer.h>
#include <86box/io.h>
#include <86box/mem.h>
#include <86box/rom.h>
#include <86box/device.h>
#include <86box/nvr.h>
#include <86box/plat.h>
#include <86box/dma.h>
#include <86box/pic.h>
#include <86box/mca.h>
#include <86box/scsi.h>
#include <86box/scsi_device.h>
#include <86box/scsi_ncr5380.h>


#define LCS6821N_ROM	L"scsi/ncr/ncr5380/longshine lcs-6821n - bios version 1.04.bin"
#define RT1000B_ROM	L"scsi/ncr/ncr5380/rancho_rt1000_rtbios_version_8.10r.bin"
#define T130B_ROM	L"scsi/ncr/ncr5380/trantor_t130b_bios_v2.14.bin"
#define SCSIAT_ROM	L"scsi/ncr/ncr5380/sumo_scsiat_bios_v6.3.bin"


#define NCR_CURDATA	0		/* current SCSI data (read only) */
#define NCR_OUTDATA 	0		/* output data (write only) */
#define NCR_INITCOMMAND 1		/* initiator command (read/write) */
#define NCR_MODE	2		/* mode (read/write) */
#define NCR_TARGETCMD	3		/* target command (read/write) */
#define NCR_SELENABLE	4		/* select enable (write only) */
#define NCR_BUSSTATUS	4		/* bus status (read only) */
#define NCR_STARTDMA	5		/* start DMA send (write only) */
#define NCR_BUSANDSTAT	5		/* bus and status (read only) */
#define NCR_DMATARGET	6		/* DMA target (write only) */
#define NCR_INPUTDATA	6		/* input data (read only) */
#define NCR_DMAINIRECV	7		/* DMA initiator receive (write only) */
#define NCR_RESETPARITY	7		/* reset parity/interrupt (read only) */

#define ICR_DBP			0x01
#define ICR_ATN			0x02
#define ICR_SEL			0x04
#define ICR_BSY			0x08
#define ICR_ACK			0x10
#define ICR_ARB_LOST		0x20
#define ICR_ARB_IN_PROGRESS	0x40

#define MODE_ARBITRATE		0x01
#define MODE_DMA		0x02
#define MODE_MONITOR_BUSY	0x04
#define MODE_ENA_EOP_INT	0x08

#define STATUS_ACK		0x01
#define STATUS_BUSY_ERROR	0x04
#define STATUS_PHASE_MATCH	0x08
#define STATUS_INT		0x10
#define STATUS_DRQ		0x40
#define STATUS_END_OF_DMA	0x80

#define TCR_IO			0x01
#define TCR_CD			0x02
#define TCR_MSG			0x04
#define TCR_REQ			0x08
#define TCR_LAST_BYTE_SENT	0x80

#define CTRL_DATA_DIR		0x40
#define STATUS_BUFFER_NOT_READY	0x04
#define STATUS_53C80_ACCESSIBLE 0x80


typedef struct {	
    uint8_t	icr;
    uint8_t	mode;
    uint8_t	tcr;
    uint8_t	ser;
    uint8_t	isr;
    uint8_t	output_data;

    uint8_t	target_id,
		target_lun;

    int		dma_mode;

    int 	bus_host, cur_bus, bus_in;
    int 	new_phase;
    int 	state;

    int		clear_req, wait_data, wait_complete;

    int		command_pos;
    uint8_t	command[20];
    int 	data_pos;
    uint8_t	tx_data;

    uint8_t	unk_08, unk_08_ret;
} ncr_t;

typedef struct {
    const char	*name;

    uint32_t	rom_addr;
    uint16_t	base;
    int8_t	irq;
    int8_t	type;

    uint8_t	block_count;
    uint8_t	status_ctrl;

    int8_t	dma_enabled;
    int8_t	ncr_busy;

    int		block_count_loaded;
    int		buffer_pos;
    int		buffer_host_pos;

    tmrval_t	timer_period;
    tmrval_t	timer_enabled;
    double	period;

    ncr_t	ncr;

    rom_t	bios_rom;
    mem_map_t	mapping;

    uint8_t	buffer[128],
		int_ram[0x40],
		ext_ram[0x600];
} ncr5380_t;


#define STATE_IDLE	0
#define STATE_COMMAND	1
#define STATE_DATAIN	2
#define STATE_DATAOUT	3
#define STATE_STATUS	4
#define STATE_MESSAGEIN	5
#define STATE_SELECT	6

#define DMA_IDLE	0
#define DMA_SEND	1
#define DMA_INITIATOR_RECEIVE 2

#define SET_BUS_STATE(ncr, state) \
		ncr->cur_bus = (ncr->cur_bus & ~(SCSI_PHASE_MESSAGE_IN)) | \
			        (state & (SCSI_PHASE_MESSAGE_IN))

static const int cmd_len[8] = {6, 10, 10, 6, 16, 12, 6, 6};


static int
get_dev_id(uint8_t data)
{
    int c;

    for (c = 0; c < SCSI_ID_MAX; c++) {
	if (data & (1 << c)) return(c);
    }

    return(-1);
}


static void
ncr_reset(ncr_t *ncr)
{
    memset(ncr, 0x00, sizeof(ncr_t));
    DEBUG("NCR reset\n");
}


static uint32_t
get_bus_host(ncr_t *ncr)
{
    uint32_t bus_host = 0;

    if (ncr->icr & ICR_DBP) {
	DEBUG("Data bus phase\n");
	bus_host |= BUS_DBP;
    }
    if (ncr->icr & ICR_SEL) {
	DEBUG("Selection phase\n");
	bus_host |= BUS_SEL;
    }
    if (ncr->tcr & TCR_IO) {
	DEBUG("Data phase\n");
	bus_host |= BUS_IO;
    }
    if (ncr->tcr & TCR_CD) {
	DEBUG("Command phase\n");
	bus_host |= BUS_CD;
    }
    if (ncr->tcr & TCR_MSG) {
	DEBUG("Message phase\n");
	bus_host |= BUS_MSG;
    }
    if (ncr->tcr & TCR_REQ) {
	DEBUG("Request phase\n");
	bus_host |= BUS_REQ;
    }
    if (ncr->icr & ICR_BSY) {
	DEBUG("Busy phase\n");
	bus_host |= BUS_BSY;
    }
    if (ncr->icr & ICR_ATN)
	bus_host |= BUS_ATN;
    if (ncr->icr & ICR_ACK) {
	DEBUG("ACK phase\n");
	bus_host |= BUS_ACK;
    }
    if (ncr->mode & MODE_ARBITRATE)
	bus_host |= BUS_ARB;

    return(bus_host | BUS_SETDATA(ncr->output_data));
}


static void
ncr_wait_process(ncr5380_t *ncr_dev)
{
    ncr_t *ncr = &ncr_dev->ncr;
    scsi_device_t *dev;

    /* Wait processes to handle bus requests. */
    DEBUG("Clear REQ=%d\n", ncr->clear_req);
    if (ncr->clear_req) {
	ncr->clear_req--;
	if (! ncr->clear_req) {
		DEBUG("Prelude to command data\n");
		SET_BUS_STATE(ncr, ncr->new_phase);
		ncr->cur_bus |= BUS_REQ;
	}
    }

    if (ncr->wait_data) {
	ncr->wait_data--;
	if (! ncr->wait_data) {
		dev = &scsi_devices[ncr->target_id][ncr->target_lun];
		SET_BUS_STATE(ncr, ncr->new_phase);

		if (ncr->new_phase == SCSI_PHASE_DATA_IN) {
			DEBUG("Data In bus phase\n");
			ncr->tx_data = dev->cmd_buffer[ncr->data_pos++];
			ncr->state = STATE_DATAIN;
			ncr->cur_bus = (ncr->cur_bus & ~BUS_DATAMASK) | BUS_SETDATA(ncr->tx_data) | BUS_DBP;
		} else if (ncr->new_phase == SCSI_PHASE_STATUS) {
			DEBUG("Status bus phase\n");
			ncr->cur_bus |= BUS_REQ;
			ncr->state = STATE_STATUS;
			ncr->cur_bus = (ncr->cur_bus & ~BUS_DATAMASK) | BUS_SETDATA(dev->status) | BUS_DBP;
		} else if (ncr->new_phase == SCSI_PHASE_MESSAGE_IN) {
			DEBUG("Message In bus phase\n");
			ncr->state = STATE_MESSAGEIN;
			ncr->cur_bus = (ncr->cur_bus & ~BUS_DATAMASK) | BUS_SETDATA(0) | BUS_DBP;
		} else {
			if (ncr->new_phase & BUS_IDLE) {
				DEBUG("Bus Idle phase\n");
				ncr->state = STATE_IDLE;
				ncr->cur_bus &= ~BUS_BSY;
			} else {
				ncr->state = STATE_DATAOUT;
				DEBUG("Data Out bus phase\n");
			}
		}
	}
    }

    if (ncr->wait_complete) {
	ncr->wait_complete--;
	if (! ncr->wait_complete)
		ncr->cur_bus |= BUS_REQ;
    }

    ncr->bus_host = ncr->cur_bus;
}


static void
ncr_callback(void *priv)
{
    ncr5380_t *ncr_dev = (ncr5380_t *)priv;
    ncr_t *ncr = &ncr_dev->ncr;
    scsi_device_t *dev = &scsi_devices[ncr->target_id][ncr->target_lun];
    int req_len, c = 0;
    tmrval_t p;
    uint8_t temp, data;

    DEBUG("NCR: DMA mode=%d\n", ncr->dma_mode);

    ncr_dev->timer_enabled = 0;

    if (((ncr->state == STATE_DATAIN) || (ncr->state == STATE_DATAOUT)) && (ncr->dma_mode != DMA_IDLE))
	ncr_dev->timer_period = (tmrval_t) ncr_dev->period;
    else
	ncr_dev->timer_period += 40LL * TIMER_USEC;

    if (ncr->dma_mode == DMA_IDLE) {
	ncr->bus_host = get_bus_host(ncr);

	/*Start the SCSI command layer, which will also make the timings*/
	if (ncr->bus_host & BUS_ARB) {
		DEBUG("NCR: arbitration\n");
		ncr->state = STATE_IDLE;
	}

	if (ncr->state == STATE_IDLE) {
		ncr->clear_req = ncr->wait_data = ncr->wait_complete = 0;
		if ((ncr->bus_host & BUS_SEL) && !(ncr->bus_host & BUS_BSY)) {
			DEBUG("NCR: selection phase\n");
			uint8_t sel_data = BUS_GETDATA(ncr->bus_host);

			ncr->target_id = get_dev_id(sel_data);

			DEBUG("NCR: select - target ID = %i\n", ncr->target_id);

			/*Once the device has been found and selected, mark it as busy*/
			if ((ncr->target_id != 0xff) &&
			    scsi_device_present(&scsi_devices[ncr->target_id][ncr->target_lun])) {
				ncr->cur_bus |= BUS_BSY;
				DEBUG("DEBUG: device found at ID %i\n", ncr->target_id);
				DEBUG("NCR: current Bus BSY=%02x\n", ncr->cur_bus);
				ncr->state = STATE_COMMAND;
				ncr->cur_bus = BUS_BSY | BUS_REQ;
				DEBUG("NCR: CurBus BSY|REQ=%02x\n", ncr->cur_bus);
				ncr->command_pos = 0;
				SET_BUS_STATE(ncr, SCSI_PHASE_COMMAND);
				if (ncr_dev->irq != -1)
					picint(1 << ncr_dev->irq);
			} else {
				ncr->state = STATE_IDLE;
				ncr->cur_bus = 0;
			}
		}
	} else if (ncr->state == STATE_COMMAND) {
		/*Command phase, make sure the ICR ACK bit is set to keep on, 
		because the device must be acknowledged by ICR*/
		DEBUG("NCR: ICR for Command=%02x\n", ncr->bus_host & BUS_ACK);
		if ((ncr->bus_host & BUS_ACK) && (ncr->command_pos < cmd_len[(ncr->command[0] >> 5) & 7])) {
			/*Write command byte to the output data register*/
			ncr->command[ncr->command_pos++] = BUS_GETDATA(ncr->bus_host);

			ncr->new_phase = ncr->cur_bus & SCSI_PHASE_MESSAGE_IN;
			ncr->clear_req = 3;
			DEBUG("NCR: current bus for command request=%02x\n", ncr->cur_bus & BUS_REQ);				
			ncr->cur_bus &= ~BUS_REQ;

			DEBUG("NCR: command pos=%i, output data=%02x\n", ncr->command_pos, BUS_GETDATA(ncr->bus_host));

			if (ncr->command_pos == cmd_len[(ncr->command[0] >> 5) & 7]) {
				/*Reset data position to default*/
				ncr->data_pos = 0;

				dev = &scsi_devices[ncr->target_id][ncr->target_lun];

				DEBUG("NCR: SCSI command 0x%02X for ID %d, status code=%02x\n", ncr->command[0], ncr->target_id, dev->status);

				dev->buffer_length = -1;

				/*Now, execute the given SCSI command*/
				scsi_device_command_phase0(dev, ncr->command);

				DEBUG("NCR: SCSI ID %i: Command %02X: Buffer Length %i, SCSI Phase %02X\n", ncr->target_id, ncr->command[0], dev->buffer_length, dev->phase);

				if (dev->status != SCSI_STATUS_OK) {
					ncr->new_phase = SCSI_PHASE_STATUS;
					ncr->wait_data = 4;
					return;
				}

				/*If the SCSI phase is Data In or Data Out, allocate the SCSI buffer based on the transfer length of the command*/
				if (dev->buffer_length && (dev->phase == SCSI_PHASE_DATA_IN || dev->phase == SCSI_PHASE_DATA_OUT)) {
					dev->cmd_buffer = (uint8_t *)mem_alloc(dev->buffer_length);

					p = scsi_device_get_callback(dev);
					req_len = MIN(64, dev->buffer_length);
					if (p <= 0LL)
						ncr_dev->period = 0.2 * ((double) TIMER_USEC) * ((double) req_len);
					else
						ncr_dev->period = (p / ((double) dev->buffer_length)) * ((double) req_len);
				}

				if (dev->phase == SCSI_PHASE_DATA_OUT) {
					/* Write direction commands have delayed execution - only execute them after the bus has gotten all the data from the host. */
					DEBUG("NCR: next state is data out\n");
					ncr->new_phase = SCSI_PHASE_DATA_OUT;
					ncr->wait_data = 4;
					ncr->clear_req = 4;
				} else {
					/* Other command - execute immediately. */
					ncr->new_phase = dev->phase;

					if (ncr->new_phase == SCSI_PHASE_DATA_IN) 
						scsi_device_command_phase1(dev);

					ncr->wait_data = 4;
				}
			}
		}
	} else if (ncr->state == STATE_DATAIN) {
		dev = &scsi_devices[ncr->target_id][ncr->target_lun];
		DEBUG("NCR: Data In ACK=%02x\n", ncr->bus_host & BUS_ACK);
		if (ncr->bus_host & BUS_ACK) {
			if (ncr->data_pos >= dev->buffer_length) {
				if (dev->cmd_buffer != NULL) {
					free(dev->cmd_buffer);
					dev->cmd_buffer = NULL;	
				}

				ncr->cur_bus &= ~BUS_REQ;
				ncr->new_phase = SCSI_PHASE_STATUS;
				ncr->wait_data = 4;
				ncr->wait_complete = 8;
			} else {
				ncr->tx_data = dev->cmd_buffer[ncr->data_pos++];
				ncr->cur_bus = (ncr->cur_bus & ~BUS_DATAMASK) | BUS_SETDATA(ncr->tx_data) | BUS_DBP | BUS_REQ;
				ncr->clear_req = 3;
				ncr->cur_bus &= ~BUS_REQ;
				ncr->new_phase = SCSI_PHASE_DATA_IN;
			}
		}
	} else if (ncr->state == STATE_DATAOUT) {
		dev = &scsi_devices[ncr->target_id][ncr->target_lun];

		DEBUG("NCR: Data Out ACK=%02x\n", ncr->bus_host & BUS_ACK);
		if (ncr->bus_host & BUS_ACK) {
			dev->cmd_buffer[ncr->data_pos++] = BUS_GETDATA(ncr->bus_host);

			if (ncr->data_pos >= dev->buffer_length) {
				scsi_device_command_phase1(dev);

				if (dev->cmd_buffer != NULL) {
					free(dev->cmd_buffer);
					dev->cmd_buffer = NULL;	
				}

				ncr->cur_bus &= ~BUS_REQ;
				DEBUG("NCR: CurBus ~REQ_DataOutDone=%02x\n", ncr->cur_bus);
				ncr->new_phase = SCSI_PHASE_STATUS;
				ncr->wait_data = 4;
				ncr->wait_complete = 8;
			} else {
				/*More data is to be transferred, place a request*/
				ncr->cur_bus |= BUS_REQ;
				DEBUG("NCR: CurBus ~REQ_DataOut=%02x\n", ncr->cur_bus);
			}
		}
	} else if (ncr->state == STATE_STATUS) {
		if (ncr->bus_host & BUS_ACK) {
			/*All transfers done, wait until next transfer*/
			ncr->cur_bus &= ~BUS_REQ;
			ncr->new_phase = SCSI_PHASE_MESSAGE_IN;
			ncr->wait_data = 4;
			ncr->wait_complete = 8;	
		}
	} else if (ncr->state == STATE_MESSAGEIN) {
		if (ncr->bus_host & BUS_ACK) {
			ncr->cur_bus &= ~BUS_REQ;
			ncr->new_phase = BUS_IDLE;
			ncr->wait_data = 4;
		}
	}
	ncr->bus_in = ncr->bus_host;
    }

    if (ncr_dev->type < 13) {	//FIXME:
	if (ncr->dma_mode == DMA_INITIATOR_RECEIVE) {
		if (!(ncr_dev->status_ctrl & CTRL_DATA_DIR)) {
			DEBUG("NCR: DMA_INITIATOR_RECEIVE with DMA direction set wrong\n");
			return;
		}

		DEBUG("NCR: status for reading=%02x\n", ncr_dev->status_ctrl);

		if (!(ncr_dev->status_ctrl & STATUS_BUFFER_NOT_READY)) return;

		if (!ncr_dev->block_count_loaded) return;

		while (c < 64) {
			/* Data ready. */
			ncr_wait_process(ncr_dev);
			temp = BUS_GETDATA(ncr->bus_host);
			ncr->bus_host = get_bus_host(ncr);

			if (ncr->data_pos >= dev->buffer_length) {
				if (dev->cmd_buffer != NULL) {
					free(dev->cmd_buffer);
					dev->cmd_buffer = NULL;	
				}

				ncr->cur_bus &= ~BUS_REQ;
				ncr->new_phase = SCSI_PHASE_STATUS;
				ncr->wait_data = 4;
				ncr->wait_complete = 8;
			} else {
				ncr->tx_data = dev->cmd_buffer[ncr->data_pos++];
				ncr->cur_bus = (ncr->cur_bus & ~BUS_DATAMASK) | BUS_SETDATA(ncr->tx_data) | BUS_DBP | BUS_REQ;
				ncr->clear_req = 3;
				ncr->cur_bus &= ~BUS_REQ;
				ncr->new_phase = SCSI_PHASE_DATA_IN;
			}

			ncr_dev->buffer[ncr_dev->buffer_pos++] = temp;
			DEBUG("NCR: buffer pos for reading=%d\n", ncr_dev->buffer_pos);

			c++;

			if (ncr_dev->buffer_pos == 128) {
				ncr_dev->buffer_pos = 0;
				ncr_dev->buffer_host_pos = 0;
				ncr_dev->status_ctrl &= ~STATUS_BUFFER_NOT_READY;
				ncr_dev->block_count = (ncr_dev->block_count - 1) & 255;

				DEBUG("NCR: remaining blocks to be read=%d\n", ncr_dev->block_count);

				if (!ncr_dev->block_count) {
					ncr_dev->block_count_loaded = 0;
					DEBUG("DEBUG: IO End of read transfer\n");

					ncr->isr |= STATUS_END_OF_DMA;
					if (ncr->mode & MODE_ENA_EOP_INT) {
						DEBUG("NCR: NCR read irq\n");
						ncr->isr |= STATUS_INT;
						if (ncr_dev->irq != -1)
							picint(1 << ncr_dev->irq);
					}
				}
				break;
			}
		}
	} else if (ncr->dma_mode == DMA_SEND) {
			if (ncr_dev->status_ctrl & CTRL_DATA_DIR) {
				DEBUG("NCR: DMA_SEND with DMA direction set wrong\n");
				return;
			}

			DEBUG("NCR: status for writing=%02x\n", ncr_dev->status_ctrl);

			if (!(ncr_dev->status_ctrl & STATUS_BUFFER_NOT_READY)) {
				DEBUG("NCR: buffer ready\n");
				return;
			}

			if (!ncr_dev->block_count_loaded) return;

			while (c < 64) {
				/* Data ready. */
				data = ncr_dev->buffer[ncr_dev->buffer_pos];
				ncr->bus_host = get_bus_host(ncr) & ~BUS_DATAMASK;
				ncr->bus_host |= BUS_SETDATA(data);

				dev->cmd_buffer[ncr->data_pos++] = BUS_GETDATA(ncr->bus_host);

				if (ncr->data_pos >= dev->buffer_length) {
					scsi_device_command_phase1(dev);

					if (dev->cmd_buffer != NULL) {
						free(dev->cmd_buffer);
						dev->cmd_buffer = NULL;	
					}

					ncr->cur_bus &= ~BUS_REQ;
					DEBUG("NCR: CurBus ~REQ_DataOutDone=%02x\n", ncr->cur_bus);
					ncr->new_phase = SCSI_PHASE_STATUS;
					ncr->wait_data = 4;
					ncr->wait_complete = 8;
				} else {
					/*More data is to be transferred, place a request*/
					ncr->cur_bus |= BUS_REQ;
					DEBUG("NCR: CurBus ~REQ_DataOut=%02x\n", ncr->cur_bus);
				}

				c++;

				if (++ncr_dev->buffer_pos == 128) {
					ncr_dev->buffer_pos = 0;
					ncr_dev->buffer_host_pos = 0;
					ncr_dev->status_ctrl &= ~STATUS_BUFFER_NOT_READY;
					ncr_dev->ncr_busy = 0;
					ncr_dev->block_count = (ncr_dev->block_count - 1) & 255;
					DEBUG("NCR: remaining blocks to be written=%d\n", ncr_dev->block_count);
					if (!ncr_dev->block_count) {
						ncr_dev->block_count_loaded = 0;
						DEBUG("NCR: IO End of write transfer\n");

						ncr->tcr |= TCR_LAST_BYTE_SENT;
						ncr->isr |= STATUS_END_OF_DMA;
						if (ncr->mode & MODE_ENA_EOP_INT) {
							DEBUG("NCR: write irq\n");
							ncr->isr |= STATUS_INT;
							if (ncr_dev->irq != -1)
								picint(1 << ncr_dev->irq);
						}
					}
					break;
				}
			}
	}
    } else {
	if (ncr->dma_mode == DMA_INITIATOR_RECEIVE) {
		if (!(ncr_dev->block_count_loaded))
			return;

		while (c < 64) {
			/* Data ready. */
			ncr_wait_process(ncr_dev);
			temp = BUS_GETDATA(ncr->bus_host);
			ncr->bus_host = get_bus_host(ncr);

			if (ncr->data_pos >= dev->buffer_length) {
				if (dev->cmd_buffer != NULL) {
					free(dev->cmd_buffer);
					dev->cmd_buffer = NULL;	
				}

				ncr->cur_bus &= ~BUS_REQ;
				ncr->new_phase = SCSI_PHASE_STATUS;
				ncr->wait_data = 4;
				ncr->wait_complete = 8;
			} else {
				ncr->tx_data = dev->cmd_buffer[ncr->data_pos++];
				ncr->cur_bus = (ncr->cur_bus & ~BUS_DATAMASK) | BUS_SETDATA(ncr->tx_data) | BUS_DBP | BUS_REQ;
				ncr->clear_req = 3;
				ncr->cur_bus &= ~BUS_REQ;
				ncr->new_phase = SCSI_PHASE_DATA_IN;
			}

			ncr_dev->buffer[ncr_dev->buffer_pos++] = temp;
			DEBUG("NCR: buffer pos for reading=%d\n", ncr_dev->buffer_pos);

			c++;

			if (ncr_dev->buffer_pos == 128) {					
				ncr_dev->buffer_pos = 0;
				ncr_dev->buffer_host_pos = 0;
				ncr_dev->block_count = (ncr_dev->block_count - 1) & 255;
				DEBUG("NCR: remaining blocks to be read=%d\n", ncr_dev->block_count);
				if (!ncr_dev->block_count) {
					ncr_dev->block_count_loaded = 0;
					DEBUG("NCR: IO End of read transfer\n");

					ncr->isr |= STATUS_END_OF_DMA;
					if (ncr->mode & MODE_ENA_EOP_INT) {
						DEBUG("NCR: read irq\n");
						ncr->isr |= STATUS_INT;
						if (ncr_dev->irq != -1)
							picint(1 << ncr_dev->irq);
					}
				}
				break;
			}
		}
	} else if (ncr->dma_mode == DMA_SEND) {
		if (!ncr_dev->block_count_loaded)
			return;

		while (c < 64) {
			/* Data ready. */
			data = ncr_dev->buffer[ncr_dev->buffer_pos];
			ncr->bus_host = get_bus_host(ncr) & ~BUS_DATAMASK;
			ncr->bus_host |= BUS_SETDATA(data);

			dev->cmd_buffer[ncr->data_pos++] = BUS_GETDATA(ncr->bus_host);

			if (ncr->data_pos >= dev->buffer_length) {
				scsi_device_command_phase1(dev);

				if (dev->cmd_buffer != NULL) {
					free(dev->cmd_buffer);
					dev->cmd_buffer = NULL;	
				}

				ncr->cur_bus &= ~BUS_REQ;
				DEBUG("NCR: CurBus ~REQ_DataOutDone=%02x\n", ncr->cur_bus);
				ncr->new_phase = SCSI_PHASE_STATUS;
				ncr->wait_data = 4;
				ncr->wait_complete = 8;
			} else {
				/*More data is to be transferred, place a request*/
				ncr->cur_bus |= BUS_REQ;
				DEBUG("DEBUG: CurBus ~REQ_DataOut=%02x\n", ncr->cur_bus);
			}

			c++;

			if (++ncr_dev->buffer_pos == 128) {
				ncr_dev->buffer_pos = 0;
				ncr_dev->buffer_host_pos = 0;
				ncr_dev->block_count = (ncr_dev->block_count - 1) & 255;
				DEBUG("NCR: remaining blocks to be written=%d\n", ncr_dev->block_count);
				if (!ncr_dev->block_count) {
					ncr_dev->block_count_loaded = 0;
					DEBUG("NCR: IO End of write transfer\n");

					ncr->tcr |= TCR_LAST_BYTE_SENT;
					ncr->isr |= STATUS_END_OF_DMA;
					if (ncr->mode & MODE_ENA_EOP_INT) {
						DEBUG("NCR: write irq\n");
						ncr->isr |= STATUS_INT;
						if (ncr_dev->irq != -1)
							picint(1 << ncr_dev->irq);
					}
				}
				break;
			}
		}
	}
    }

    ncr_wait_process(ncr_dev);
    if (!(ncr->bus_host & BUS_BSY) && (ncr->mode & MODE_MONITOR_BUSY)) {
	DEBUG("NCR: updating DMA\n");
	ncr->mode &= ~MODE_DMA;
	ncr->dma_mode = DMA_IDLE;
    }
}


static void
ncr_write(uint16_t port, uint8_t val, priv_t priv)
{
    ncr5380_t *ncr_dev = (ncr5380_t *)priv;
    ncr_t *ncr = &ncr_dev->ncr;

    DBGLOG(2, "NCR5380 write(%04x,%02x)\n", port & 7, val);

    switch (port & 7) {
	case 0:		/* Output data register */
		DEBUG("Write: Output data register\n");
		ncr->output_data = val;
		break;

	case 1:		/* Initiator Command Register */
		DEBUG("Write: Initiator command register\n");
		ncr->icr = val;

		ncr_dev->timer_enabled = 1;
		break;

	case 2:		/* Mode register */
		DEBUG("Write: Mode register, val=%02x\n", val & MODE_DMA);
		if ((val & MODE_ARBITRATE) && !(ncr->mode & MODE_ARBITRATE)) {
			ncr->icr &= ~ICR_ARB_LOST;
			ncr->icr |=  ICR_ARB_IN_PROGRESS;
		}

		ncr->mode = val;

		/*If it's not DMA mode, don't do anything*/
		if (!(ncr->mode & MODE_DMA)) {
			DEBUG("No DMA mode\n");
			ncr->tcr &= ~TCR_LAST_BYTE_SENT;
			ncr->isr &= ~STATUS_END_OF_DMA;
			ncr->dma_mode = DMA_IDLE;
		}
		break;

	case 3:		/* Target Command Register */
		DEBUG("Write: Target Command register\n");
		ncr->tcr = val;
		break;

	case 4:		/* Select Enable Register */
		DEBUG("Write: Select Enable register\n");
		break;
		
	case 5:		/* start DMA Send */
		DEBUG("Write: start DMA send register\n");
		DEBUG("Write 6 or 10, block count loaded=%d\n", ncr_dev->block_count_loaded);
		/*a Write 6/10 has occurred, start the timer when the block count is loaded*/
		ncr->dma_mode = DMA_SEND;
		break;

	case 7:		/* start DMA Initiator Receive */
		DEBUG("Write: start DMA initiator receive register\n");
		DEBUG("Read 6 or 10, block count loaded=%d\n", ncr_dev->block_count_loaded);
		/*a Read 6/10 has occurred, start the timer when the block count is loaded*/
		ncr->dma_mode = DMA_INITIATOR_RECEIVE;
		break;

	default:
		DEBUG("NCR5380: bad write %04x %02x\n", port, val);
		break;
    }
}


static uint8_t
ncr_read(uint16_t port, priv_t priv)
{
    ncr5380_t *ncr_dev = (ncr5380_t *)priv;
    ncr_t *ncr = &ncr_dev->ncr;
    uint8_t ret = 0xff;

    switch (port & 7) {
	case 0:		/* Current SCSI data */
		DEBUG("Read: Current SCSI data register\n");
		if (ncr->icr & ICR_DBP) {
			/*Return the data from the output register if on data bus phase from ICR*/
			DEBUG("Data Bus Phase\n");
			ret = ncr->output_data;
		} else {
			/*Return the data from the SCSI bus*/
			ncr_wait_process(ncr_dev);
			DEBUG("NCR GetData=%02x\n", BUS_GETDATA(ncr->bus_host));
			ret = BUS_GETDATA(ncr->bus_host);
		}
		break;

	case 1:		/* Initiator Command Register */
		DEBUG("Read: Initiator Command register\n");
		DEBUG("NCR ICR Read=%02x\n", ncr->icr);

		ret = ncr->icr;
		break;

	case 2:		/* Mode register */
		DEBUG("Read: Mode register\n");
		ret = ncr->mode;
		break;

	case 3:		/* Target Command Register */
		DEBUG("Read: Target Command register\n");
		DEBUG("NCR target stat=%02x\n", ncr->tcr);
		ret = ncr->tcr;
		break;

	case 4:		/* Current SCSI Bus status */
		DEBUG("Read: SCSI bus status register\n");
		ret = 0;
		ncr_wait_process(ncr_dev);
		DEBUG("NCR cur bus stat=%02x\n", ncr->bus_host & 0xff);
		ret |= (ncr->bus_host & 0xff);
		break;

	case 5:		/* Bus and Status register */
		DEBUG("Read: Bus and Status register\n");
		ret = 0;		

		ncr->bus_host = get_bus_host(ncr);
		DEBUG("Get host from Interrupt\n");

		/*Check if the phase in process matches with TCR's*/
		if ((ncr->bus_host & SCSI_PHASE_MESSAGE_IN) ==
			(ncr->cur_bus & SCSI_PHASE_MESSAGE_IN)) {
			DEBUG("Phase match\n");
			ret |= STATUS_PHASE_MATCH;
		} else {
			if (ncr_dev->irq != -1)
				picint(1 << ncr_dev->irq);
		}

		ncr_wait_process(ncr_dev);

		if (ncr->bus_host & BUS_ACK)
			ret |= STATUS_ACK;

		if ((ncr->bus_host & BUS_REQ) && (ncr->mode & MODE_DMA)) {
			DEBUG("Entering DMA mode\n");
			ret |= STATUS_DRQ;

			int bus = 0;

			if (ncr->bus_host & BUS_IO)
				bus |= TCR_IO;
			if (ncr->bus_host & BUS_CD)
				bus |= TCR_CD;
			if (ncr->bus_host & BUS_MSG)
				bus |= TCR_MSG;
			if ((ncr->tcr & 7) != bus) {
				ncr->isr |= STATUS_INT;
			}
		}
		if (!(ncr->bus_host & BUS_BSY) && (ncr->mode & MODE_MONITOR_BUSY)) {
			DEBUG("Busy error\n");
			ret |= STATUS_BUSY_ERROR;
		}
		ret |= (ncr->isr & (STATUS_INT | STATUS_END_OF_DMA));
		break;

	case 7:		/* reset Parity/Interrupt */
		ncr->isr &= ~STATUS_INT;
		if (ncr_dev->irq != -1)
			picintc(1 << ncr_dev->irq);
		DEBUG("Reset IRQ\n");
		break;

	default:
		DEBUG("NCR5380: bad read %04x\n", port);
		break;
    }

    DEBUG("NCR5380 read(%04x)=%02x\n", port & 7, ret);

    return(ret);
}


/* Memory-mapped I/O READ handler. */
static uint8_t
memio_read(uint32_t addr, priv_t priv)
{
    ncr5380_t *ncr_dev = (ncr5380_t *)priv;
    uint8_t ret = 0xff;
	
    addr &= 0x3fff;
    DBGLOG(2, "memio_read %08x\n", addr);

    if (addr < 0x2000)
	ret = ncr_dev->bios_rom.rom[addr & 0x1fff];
    else if (addr < 0x3800)
	ret = 0xff;
    else if (addr >= 0x3a00)
	ret = ncr_dev->ext_ram[addr - 0x3a00];
    else switch (addr & 0x3f80) {
	case 0x3800:
		DBGLOG(1, "Read intRAM %02x %02x\n", addr & 0x3f, ncr_dev->int_ram[addr & 0x3f]);
		ret = ncr_dev->int_ram[addr & 0x3f];
		break;

	case 0x3880:
		DBGLOG(1, "Read 53c80 %04x\n", addr);
		ret = ncr_read(addr, ncr_dev);
		break;

	case 0x3900:
		DBGLOG(1, "Read 3900 host pos %i status ctrl %02x\n", ncr_dev->buffer_host_pos, ncr_dev->status_ctrl);
		DEBUG("Read port 0x3900-0x397f\n");

		if (ncr_dev->buffer_host_pos >= 128 || !(ncr_dev->status_ctrl & CTRL_DATA_DIR))
			ret = 0xff;
		else {
			ret = ncr_dev->buffer[ncr_dev->buffer_host_pos++];
			
			DEBUG("Read host buffer=%d\n", ncr_dev->buffer_host_pos);

			if (ncr_dev->buffer_host_pos == 128) {
				DEBUG("Not ready\n");
				ncr_dev->status_ctrl |= STATUS_BUFFER_NOT_READY;
			}
		}
		break;

	case 0x3980:
		switch (addr) {
			case 0x3980:	/* status */
				ret = ncr_dev->status_ctrl;// | 0x80;
				DEBUG("NCR status ctrl read=%02x\n", ncr_dev->status_ctrl & STATUS_BUFFER_NOT_READY);
				if (!ncr_dev->ncr_busy) {
					ret |= STATUS_53C80_ACCESSIBLE;
				}
				break;

			case 0x3981:	/* block counter register*/
				ret = ncr_dev->block_count;
				break;

			case 0x3982:	/* switch register read */
				ret = 0xff;
				break;

			case 0x3983:
				ret = 0xff;
				break;
		}
		break;
    }

    if (addr >= 0x3880) {
	DBGLOG(2, "memio_read(%08x)=%02x\n", addr, ret);
    }

    return(ret);
}


/* Memory-mapped I/O WRITE handler. */
static void
memio_write(uint32_t addr, uint8_t val, priv_t priv)
{
    ncr5380_t *ncr_dev = (ncr5380_t *)priv;

    addr &= 0x3fff;

    DBGLOG(2, "memio_write(%08x,%02x)  %i %02x\n", addr, val,  ncr_dev->buffer_host_pos, ncr_dev->status_ctrl);

    if (addr >= 0x3a00)
	ncr_dev->ext_ram[addr - 0x3a00] = val;
    else switch (addr & 0x3f80) {
	case 0x3800:
		DBGLOG(1, "Write intram %02x %02x\n", addr & 0x3f, val);
		ncr_dev->int_ram[addr & 0x3f] = val;
		break;

	case 0x3880:
		DBGLOG(1, "Write 53c80 %04x %02x\n", addr, val);
		ncr_write(addr, val, ncr_dev);
		break;

	case 0x3900:
		if (!(ncr_dev->status_ctrl & CTRL_DATA_DIR) && ncr_dev->buffer_host_pos < 128) {
			ncr_dev->buffer[ncr_dev->buffer_host_pos++] = val;

			if (ncr_dev->buffer_host_pos == 128) {
				ncr_dev->status_ctrl |= STATUS_BUFFER_NOT_READY;
				ncr_dev->ncr_busy = 1;
			}
		}
		break;

	case 0x3980:
		switch (addr) {
			case 0x3980:	/* Control */
				DEBUG("Write 0x3980: val=%02x\n", val);
				if (val & 0x80) {
					DEBUG("Resetting the 53c400\n");
					if (ncr_dev->irq != -1)
						picint(1 << ncr_dev->irq);
				}

				if ((val & CTRL_DATA_DIR) && !(ncr_dev->status_ctrl & CTRL_DATA_DIR)) {
					DEBUG("Pos 128\n");
					ncr_dev->buffer_host_pos = 128;
					ncr_dev->status_ctrl |= STATUS_BUFFER_NOT_READY;
				}
				else if (!(val & CTRL_DATA_DIR) && (ncr_dev->status_ctrl & CTRL_DATA_DIR)) {
					DEBUG("Pos 0\n");
					ncr_dev->buffer_host_pos = 0;
					ncr_dev->status_ctrl &= ~STATUS_BUFFER_NOT_READY;
				}
				ncr_dev->status_ctrl = (ncr_dev->status_ctrl & 0x87) | (val & 0x78);
				break;

			case 0x3981:	/* block counter register */
				DEBUG("Write 0x3981: val=%d\n", val);
				ncr_dev->block_count = val;
				ncr_dev->block_count_loaded = 1;

				DEBUG("Timer for transfers=%02x\n", ncr_dev->timer_enabled);

				if (ncr_dev->status_ctrl & CTRL_DATA_DIR) {
					DEBUG("Data Read\n");
					ncr_dev->buffer_host_pos = 128;
					ncr_dev->status_ctrl |= STATUS_BUFFER_NOT_READY;
				} else {
					DEBUG("Data Write\n");
					ncr_dev->buffer_host_pos = 0;
					ncr_dev->status_ctrl &= ~STATUS_BUFFER_NOT_READY;
				}
				break;
		}
		break;	
    }
}


/* Memory-mapped I/O READ handler for the Trantor T130B. */
static uint8_t
t130b_read(uint32_t addr, priv_t priv)
{
    ncr5380_t *ncr_dev = (ncr5380_t *)priv;
    uint8_t ret = 0xff;

    addr &= 0x3fff;
    if (addr < 0x1800)
	ret = ncr_dev->bios_rom.rom[addr & 0x1fff];
      else
    if (addr < 0x1880)
	ret = ncr_dev->ext_ram[addr & 0x7f];

    return(ret);
}


/* Memory-mapped I/O WRITE handler for the Trantor T130B. */
static void
t130b_write(uint32_t addr, uint8_t val, priv_t priv)
{
    ncr5380_t *ncr_dev = (ncr5380_t *)priv;

    addr &= 0x3fff;
    if (addr >= 0x1800 && addr < 0x1880)
	ncr_dev->ext_ram[addr & 0x7f] = val;
}


static uint8_t
t130b_in(uint16_t port, priv_t priv)
{
    uint8_t ret = 0xff;

    switch (port & 0x0f) {
	case 0x00:
	case 0x01:
	case 0x02:
	case 0x03:
		ret = memio_read((port & 7) | 0x3980, priv);
		break;

	case 0x04:
	case 0x05:
		ret = memio_read(0x3900, priv);
		break;

	case 0x08:
	case 0x09:
	case 0x0a:
	case 0x0b:
	case 0x0c:
	case 0x0d:
	case 0x0e:
	case 0x0f:
		ret = ncr_read(port, priv);
		break;
    }

    return(ret);
}


static void
t130b_out(uint16_t port, uint8_t val, priv_t priv)
{
    switch (port & 0x0f) {
	case 0x00:
	case 0x01:
	case 0x02:
	case 0x03:
		memio_write((port & 7) | 0x3980, val, priv);
		break;

	case 0x04:
	case 0x05:
		memio_write(0x3900, val, priv);
		break;

	case 0x08:
	case 0x09:
	case 0x0a:
	case 0x0b:
	case 0x0c:
	case 0x0d:
	case 0x0e:
	case 0x0f:
		ncr_write(port, val, priv);
		break;
    }
}


static uint8_t
scsiat_in(uint16_t port, priv_t priv)
{
    uint8_t ret = 0xff;

    switch (port & 0x0f) {
	case 0x00:
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x04:
	case 0x05:
	case 0x06:
	case 0x07:
		ret = ncr_read(port, priv);
		break;
    }

    DBGLOG(2, "SCSI-AT read=0x%03x, ret=%02x\n", port, ret);

    return(ret);
}


static void 
scsiat_out(uint16_t port, uint8_t val, priv_t priv)
{
    ncr5380_t *ncr_dev = (ncr5380_t *)priv;
    ncr_t *ncr = &ncr_dev->ncr;
    scsi_device_t *dev = &scsi_devices[ncr->target_id][ncr->target_lun];

    DBGLOG(2, "SCSI-AT write=0x%03x, val=%02x\n", port, val);

    switch (port & 0x0f) {
	case 0x00:
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x04:
	case 0x05:
	case 0x06:
	case 0x07:
		ncr_write(port, val, priv);
		break;

	case 0x08:
		ncr->unk_08 = val;

		if (ncr->unk_08 & 0x08) {
			if (ncr->dma_mode == DMA_INITIATOR_RECEIVE) {
				while (ncr_dev->buffer_host_pos < 128) {
					uint8_t temp;

					temp = ncr_dev->buffer[ncr_dev->buffer_host_pos++];

					DBGLOG(1, "Read Buffer host=%d\n", ncr_dev->buffer_host_pos);

					ncr->bus_host = get_bus_host(ncr) & ~BUS_DATAMASK;
					ncr->bus_host |= BUS_SETDATA(temp);

					if (ncr_dev->buffer_host_pos == 128)
						break;
				}
			} else if (ncr->dma_mode == DMA_SEND) {
				while (ncr_dev->buffer_host_pos < 128) {
					/* Data ready. */
					uint8_t temp;

					ncr_wait_process(ncr_dev);
					temp = BUS_GETDATA(ncr->bus_host);
					ncr->bus_host = get_bus_host(ncr);

					ncr_dev->buffer[ncr_dev->buffer_host_pos++] = temp;

					DBGLOG(1, "Write Buffer host=%d\n", ncr_dev->buffer_host_pos);

					if (ncr_dev->buffer_host_pos == 128) {

						break;
					}
				}
			}
		}

		if (ncr->unk_08 & 0x01) {
			ncr_dev->block_count_loaded = 1;
			ncr_dev->block_count = dev->buffer_length / 128;
		}
		break;
    }
}


static uint8_t 
dev_in(uint16_t port, priv_t priv)
{
    uint8_t ret = 0xff;

    switch (port & 0x0f) {
	case 0x00:
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x04:
	case 0x05:
	case 0x06:
	case 0x07:
		ret = ncr_read(port, priv);
		break;
    }

    DBGLOG(2, "5380: in(0x%03x) = %02x\n", port, ret);

    return(ret);
}


static void 
dev_out(uint16_t port, uint8_t val, priv_t priv)
{
    ncr5380_t *ncr_dev = (ncr5380_t *)priv;
    ncr_t *ncr = &ncr_dev->ncr;
    scsi_device_t *dev = &scsi_devices[ncr->target_id][ncr->target_lun];

    DBGLOG(2, "5380 write(0x%03x, %02x)\n", port, val);

    switch (port & 0x0f) {
	case 0x00:
	case 0x01:
	case 0x02:
	case 0x03:
	case 0x04:
	case 0x05:
	case 0x06:
	case 0x07:
		ncr_write(port, val, priv);
		break;

	case 0x08:
		ncr->unk_08 = val;

		if (ncr->unk_08 & 0x08) {
			if (ncr->dma_mode == DMA_INITIATOR_RECEIVE) {
				while (ncr_dev->buffer_host_pos < 128) {
					uint8_t temp;

					temp = ncr_dev->buffer[ncr_dev->buffer_host_pos++];

					DBGLOG(1, "Read Buffer host=%d\n", ncr_dev->buffer_host_pos);

					ncr->bus_host = get_bus_host(ncr) & ~BUS_DATAMASK;
					ncr->bus_host |= BUS_SETDATA(temp);

					if (ncr_dev->buffer_host_pos == 128)
						break;
				}
			} else if (ncr->dma_mode == DMA_SEND) {
				while (ncr_dev->buffer_host_pos < 128) {
					/* Data ready. */
					uint8_t temp;

					ncr_wait_process(ncr_dev);
					temp = BUS_GETDATA(ncr->bus_host);
					ncr->bus_host = get_bus_host(ncr);

					ncr_dev->buffer[ncr_dev->buffer_host_pos++] = temp;

					DBGLOG(1, "Write Buffer host=%d\n", ncr_dev->buffer_host_pos);

					if (ncr_dev->buffer_host_pos == 128) {

						break;
					}
				}
			}
		}

		if (ncr->unk_08 & 0x01) {
			ncr_dev->block_count_loaded = 1;
			ncr_dev->block_count = dev->buffer_length / 128;
		}
		break;
    }
}


static void 
ncr_close(priv_t priv)
{
    ncr5380_t *ncr_dev = (ncr5380_t *)priv;

    if (ncr_dev) {
	/* Tell the timer to terminate. */
	ncr_dev->timer_period = 0LL;
	ncr_dev->timer_enabled = 0LL;

	free(ncr_dev);
    }
}


static priv_t
ncr_init(const device_t *info, UNUSED(void *parent))
{
    char temp[128];
    ncr5380_t *ncr_dev;

    ncr_dev = (ncr5380_t *)mem_alloc(sizeof(ncr5380_t));
    memset(ncr_dev, 0x00, sizeof(ncr5380_t));
    ncr_dev->name = info->name;
    ncr_dev->type = info->local;

    switch(ncr_dev->type) {
	case 0:		/* NCR 53C80 (on-board) */
		ncr_dev->base = 0;
		ncr_dev->irq = -1;
		break;

	case 10:	/* Longshine LCS6821N */
		ncr_dev->rom_addr = device_get_config_hex20("bios_addr");
		rom_init(&ncr_dev->bios_rom, info->path,
			 ncr_dev->rom_addr, 0x4000, 0x3fff,
			 0, MEM_MAPPING_EXTERNAL);

		mem_map_disable(&ncr_dev->bios_rom.mapping);

		mem_map_add(&ncr_dev->mapping, ncr_dev->rom_addr, 0x4000, 
			    memio_read, NULL, NULL, memio_write, NULL, NULL,
			    ncr_dev->bios_rom.rom, 0, ncr_dev);
		break;

	case 11:	/* Rancho RT1000B */
		ncr_dev->rom_addr = device_get_config_hex20("bios_addr");
		rom_init(&ncr_dev->bios_rom, info->path,
			 ncr_dev->rom_addr, 0x4000, 0x3fff,
			 0, MEM_MAPPING_EXTERNAL);

		mem_map_disable(&ncr_dev->bios_rom.mapping);

		mem_map_add(&ncr_dev->mapping, ncr_dev->rom_addr, 0x4000, 
			    memio_read, NULL, NULL, memio_write, NULL, NULL,
			    ncr_dev->bios_rom.rom, 0, ncr_dev);
		break;

	case 12:		/* Trantor T130B */
		ncr_dev->rom_addr = device_get_config_hex20("bios_addr");
		ncr_dev->base = device_get_config_hex16("base");
		ncr_dev->irq = device_get_config_int("irq");
		rom_init(&ncr_dev->bios_rom, info->path,
			 ncr_dev->rom_addr, 0x4000, 0x3fff,
			 0, MEM_MAPPING_EXTERNAL);
		mem_map_disable(&ncr_dev->bios_rom.mapping);	 

		mem_map_add(&ncr_dev->mapping, ncr_dev->rom_addr, 0x4000, 
			    t130b_read, NULL, NULL,
			    t130b_write, NULL, NULL,
			    ncr_dev->bios_rom.rom, 0, ncr_dev);

		io_sethandler(ncr_dev->base, 16,
			      t130b_in,NULL,NULL, t130b_out,NULL,NULL, ncr_dev);
		break;

	case 13:		/* Sumo SCSI-AT */
		ncr_dev->base = device_get_config_hex16("base");
		ncr_dev->irq = device_get_config_int("irq");
		ncr_dev->rom_addr = device_get_config_hex20("bios_addr");
		rom_init(&ncr_dev->bios_rom, info->path,
			 ncr_dev->rom_addr, 0x4000, 0x3fff,
			 0, MEM_MAPPING_EXTERNAL);

		mem_map_disable(&ncr_dev->bios_rom.mapping); 

		mem_map_add(&ncr_dev->mapping, ncr_dev->rom_addr, 0x4000, 
			    t130b_read, NULL, NULL,
			    t130b_write, NULL, NULL,
			    ncr_dev->bios_rom.rom, 0, ncr_dev);

		io_sethandler(ncr_dev->base, 16,
			      scsiat_in,NULL,NULL, scsiat_out,NULL,NULL, ncr_dev);
		break;
    }

    sprintf(temp, "%s: BIOS=%05X", ncr_dev->name, ncr_dev->rom_addr);
    if (ncr_dev->base != 0)
	sprintf(&temp[strlen(temp)], " I/O=%04x", ncr_dev->base);
    if (ncr_dev->irq != 0)
	sprintf(&temp[strlen(temp)], " IRQ=%d", ncr_dev->irq);
    INFO("%s\n", temp);

    ncr_reset(&ncr_dev->ncr);

    ncr_dev->status_ctrl = STATUS_BUFFER_NOT_READY;
    ncr_dev->buffer_host_pos = 128;

    ncr_dev->timer_period = 10LL * TIMER_USEC;
    timer_add(ncr_callback, ncr_dev,
	      &ncr_dev->timer_period, TIMER_ALWAYS_ENABLED);

    return((priv_t)ncr_dev);
}


static const device_config_t ncr5380_mmio_config[] = {
    {
	"bios_addr", "BIOS Address", CONFIG_HEX20, "", 0xd8000,
	{
		{
			"Disabled", 0
		},
		{
			"C800H", 0xc8000
		},
		{
			"CC00H", 0xcc000
		},
		{
			"D800H", 0xd8000
		},
		{
			"DC00H", 0xdc000
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

static const device_config_t t130b_config[] = {
    {
        "base", "Address", CONFIG_HEX16, "", 0x0350,
        {
                {
                        "240H", 0x0240
                },
                {
                        "250H", 0x0250
                },
                {
                        "340H", 0x0340
                },
                {
                        "350H", 0x0350
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
                        "IRQ 3", 3
                },
                {
                        "IRQ 5", 5
                },
                {
                        "IRQ 7", 7
                },
                {
                        NULL
                }
        }
    },
    {
	"bios_addr", "BIOS Address", CONFIG_HEX20, "", 0xd8000,
	{
		{
			"Disabled", 0
		},
		{
			"C800H", 0xc8000
		},
		{
			"CC00H", 0xcc000
		},
		{
			"D800H", 0xd8000
		},
		{
			"DC00H", 0xdc000
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

static const device_config_t scsiat_config[] = {
    {
        "base", "Address", CONFIG_HEX16, "", 0x0310,
        {
                {
                        "None",      0
                },
                {
                        "300H", 0x0300
                },
                {
                        "310H", 0x0310
                },
                {
                        "320H", 0x0320
                },
                {
                        "330H", 0x0330
                },
                {
                        "340H", 0x0340
                },
                {
                        "350H", 0x0350
                },
                {
                        "360H", 0x0360
                },
                {
                        "370H", 0x0370
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
                        "IRQ 3", 3
                },
                {
                        "IRQ 4", 4
                },
                {
                        "IRQ 5", 5
                },
                {
                        "IRQ 10", 10
                },
                {
                        "IRQ 11", 11
                },
                {
                        "IRQ 12", 12
                },
                {
                        "IRQ 14", 14
                },
                {
                        "IRQ 15", 15
                },
                {
                        NULL
                }
        }
    },
    {
        "bios_addr", "BIOS Address", CONFIG_HEX20, "", 0xD8000,
        {
                {
                        "Disabled", 0
                },
                {
                        "C800H", 0xc8000
                },
                {
                        "CC00H", 0xcc000
                },
                {
                        "D800H", 0xd8000
                },
                {
                        "DC00H", 0xdc000
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


const device_t scsi_ncr53c80_onboard_device = {
    "NCR 53C80 (onboard)",
    DEVICE_ISA,
    0,
    NULL,
    ncr_init, ncr_close, NULL,
    NULL, NULL, NULL, NULL,
    NULL
};

const device_t scsi_lcs6821n_device = {
    "Longshine LCS-6821N",
    DEVICE_ISA,
    10,
    LCS6821N_ROM,
    ncr_init, ncr_close, NULL,
    NULL, NULL, NULL, NULL,
    ncr5380_mmio_config
};

const device_t scsi_rt1000b_device = {
    "Ranco RT1000B",
    DEVICE_ISA,
    11,
    RT1000B_ROM,
    ncr_init, ncr_close, NULL,
    NULL, NULL, NULL, NULL,
    ncr5380_mmio_config
};

const device_t scsi_t130b_device = {
    "Trantor T130B",
    DEVICE_ISA,
    12,
    T130B_ROM,
    ncr_init, ncr_close, NULL,
    NULL, NULL, NULL, NULL,
    t130b_config
};

const device_t scsi_scsiat_device = {
    "Sumo SCSI-AT",
    DEVICE_ISA,
    13,
    SCSIAT_ROM,
    ncr_init, ncr_close, NULL,
    NULL, NULL, NULL, NULL,
    scsiat_config
};


/*
 * *** THIS IS A TEMPORARY FUNCTION ***
 *
 * Allows setting of the device's IRQ value,
 * currently needed for onboard devices.
 */
void
scsi_ncr5380_set_info(priv_t priv, int base, int irq)
{
    ncr5380_t *dev = (ncr5380_t *)priv;

    DBGLOG(1, "5380: Set_Info(%04x, %i)\n", base, irq);

    if (dev->base != 0)
	io_removehandler(dev->base, 16,
			 dev_in,NULL,NULL, dev_out,NULL,NULL, dev);
    dev->base = base;
    io_sethandler(dev->base, 16,
		  dev_in,NULL,NULL, dev_out,NULL,NULL, dev);

    dev->irq = irq;
}
