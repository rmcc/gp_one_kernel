/*
 * IDE ATAPI floppy driver.
 *
 * Copyright (C) 1996-1999  Gadi Oxman <gadio@netvision.net.il>
 * Copyright (C) 2000-2002  Paul Bristow <paul@paulbristow.net>
 * Copyright (C) 2005       Bartlomiej Zolnierkiewicz
 *
 * This driver supports the following IDE floppy drives:
 *
 * LS-120/240 SuperDisk
 * Iomega Zip 100/250
 * Iomega PC Card Clik!/PocketZip
 *
 * For a historical changelog see
 * Documentation/ide/ChangeLog.ide-floppy.1996-2002
 */

#define DRV_NAME "ide-floppy"

#define IDEFLOPPY_VERSION "1.00"

#include <linux/module.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/major.h>
#include <linux/errno.h>
#include <linux/genhd.h>
#include <linux/slab.h>
#include <linux/cdrom.h>
#include <linux/ide.h>
#include <linux/hdreg.h>
#include <linux/bitops.h>
#include <linux/mutex.h>
#include <linux/scatterlist.h>

#include <scsi/scsi_ioctl.h>

#include <asm/byteorder.h>
#include <linux/irq.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <asm/unaligned.h>

#include "ide-floppy.h"

/* define to see debug info */
#define IDEFLOPPY_DEBUG_LOG		0

/* #define IDEFLOPPY_DEBUG(fmt, args...) printk(KERN_INFO fmt, ## args) */
#define IDEFLOPPY_DEBUG(fmt, args...)

#if IDEFLOPPY_DEBUG_LOG
#define debug_log(fmt, args...) \
	printk(KERN_INFO "ide-floppy: " fmt, ## args)
#else
#define debug_log(fmt, args...) do {} while (0)
#endif

/*
 * After each failed packet command we issue a request sense command and retry
 * the packet command IDEFLOPPY_MAX_PC_RETRIES times.
 */
#define IDEFLOPPY_MAX_PC_RETRIES	3

/* format capacities descriptor codes */
#define CAPACITY_INVALID	0x00
#define CAPACITY_UNFORMATTED	0x01
#define CAPACITY_CURRENT	0x02
#define CAPACITY_NO_CARTRIDGE	0x03

#define IDEFLOPPY_TICKS_DELAY	HZ/20	/* default delay for ZIP 100 (50ms) */

/* Error code returned in rq->errors to the higher part of the driver. */
#define	IDEFLOPPY_ERROR_GENERAL		101

static DEFINE_MUTEX(idefloppy_ref_mutex);

#define to_ide_floppy(obj) container_of(obj, struct ide_floppy_obj, kref)

#define ide_floppy_g(disk) \
	container_of((disk)->private_data, struct ide_floppy_obj, driver)

static void idefloppy_cleanup_obj(struct kref *);

static struct ide_floppy_obj *ide_floppy_get(struct gendisk *disk)
{
	struct ide_floppy_obj *floppy = NULL;

	mutex_lock(&idefloppy_ref_mutex);
	floppy = ide_floppy_g(disk);
	if (floppy) {
		if (ide_device_get(floppy->drive))
			floppy = NULL;
		else
			kref_get(&floppy->kref);
	}
	mutex_unlock(&idefloppy_ref_mutex);
	return floppy;
}

static void ide_floppy_put(struct ide_floppy_obj *floppy)
{
	ide_drive_t *drive = floppy->drive;

	mutex_lock(&idefloppy_ref_mutex);
	kref_put(&floppy->kref, idefloppy_cleanup_obj);
	ide_device_put(drive);
	mutex_unlock(&idefloppy_ref_mutex);
}

/*
 * Used to finish servicing a request. For read/write requests, we will call
 * ide_end_request to pass to the next buffer.
 */
static int idefloppy_end_request(ide_drive_t *drive, int uptodate, int nsecs)
{
	idefloppy_floppy_t *floppy = drive->driver_data;
	struct request *rq = HWGROUP(drive)->rq;
	int error;

	debug_log("Reached %s\n", __func__);

	switch (uptodate) {
	case 0: error = IDEFLOPPY_ERROR_GENERAL; break;
	case 1: error = 0; break;
	default: error = uptodate;
	}
	if (error)
		floppy->failed_pc = NULL;
	/* Why does this happen? */
	if (!rq)
		return 0;
	if (!blk_special_request(rq)) {
		/* our real local end request function */
		ide_end_request(drive, uptodate, nsecs);
		return 0;
	}
	rq->errors = error;
	/* fixme: need to move this local also */
	ide_end_drive_cmd(drive, 0, 0);
	return 0;
}

static void idefloppy_update_buffers(ide_drive_t *drive,
				struct ide_atapi_pc *pc)
{
	struct request *rq = pc->rq;
	struct bio *bio = rq->bio;

	while ((bio = rq->bio) != NULL)
		idefloppy_end_request(drive, 1, 0);
}

static void ide_floppy_callback(ide_drive_t *drive)
{
	idefloppy_floppy_t *floppy = drive->driver_data;
	struct ide_atapi_pc *pc = floppy->pc;
	int uptodate = pc->error ? 0 : 1;

	debug_log("Reached %s\n", __func__);

	if (floppy->failed_pc == pc)
		floppy->failed_pc = NULL;

	if (pc->c[0] == GPCMD_READ_10 || pc->c[0] == GPCMD_WRITE_10 ||
	    (pc->rq && blk_pc_request(pc->rq)))
		uptodate = 1; /* FIXME */
	else if (pc->c[0] == GPCMD_REQUEST_SENSE) {
		u8 *buf = floppy->pc->buf;

		if (!pc->error) {
			floppy->sense_key = buf[2] & 0x0F;
			floppy->asc = buf[12];
			floppy->ascq = buf[13];
			floppy->progress_indication = buf[15] & 0x80 ?
				(u16)get_unaligned((u16 *)&buf[16]) : 0x10000;

			if (floppy->failed_pc)
				debug_log("pc = %x, ", floppy->failed_pc->c[0]);

			debug_log("sense key = %x, asc = %x, ascq = %x\n",
				  floppy->sense_key, floppy->asc, floppy->ascq);
		} else
			printk(KERN_ERR "Error in REQUEST SENSE itself - "
					"Aborting request!\n");
	}

	idefloppy_end_request(drive, uptodate, 0);
}

void ide_floppy_create_request_sense_cmd(struct ide_atapi_pc *pc)
{
	ide_init_pc(pc);
	pc->c[0] = GPCMD_REQUEST_SENSE;
	pc->c[4] = 255;
	pc->req_xfer = 18;
}

/*
 * Called when an error was detected during the last packet command. We queue a
 * request sense packet command in the head of the request list.
 */
static void idefloppy_retry_pc(ide_drive_t *drive)
{
	struct ide_floppy_obj *floppy = drive->driver_data;
	struct request *rq = &floppy->request_sense_rq;
	struct ide_atapi_pc *pc = &floppy->request_sense_pc;

	(void)ide_read_error(drive);
	ide_floppy_create_request_sense_cmd(pc);
	ide_queue_pc_head(drive, floppy->disk, pc, rq);
}

/* The usual interrupt handler called during a packet command. */
static ide_startstop_t idefloppy_pc_intr(ide_drive_t *drive)
{
	idefloppy_floppy_t *floppy = drive->driver_data;

	return ide_pc_intr(drive, floppy->pc, idefloppy_pc_intr,
			   WAIT_FLOPPY_CMD, NULL, idefloppy_update_buffers,
			   idefloppy_retry_pc, NULL, ide_io_buffers);
}

/*
 * What we have here is a classic case of a top half / bottom half interrupt
 * service routine. In interrupt mode, the device sends an interrupt to signal
 * that it is ready to receive a packet. However, we need to delay about 2-3
 * ticks before issuing the packet or we gets in trouble.
 */
static int idefloppy_transfer_pc(ide_drive_t *drive)
{
	idefloppy_floppy_t *floppy = drive->driver_data;

	/* Send the actual packet */
	drive->hwif->tp_ops->output_data(drive, NULL, floppy->pc->c, 12);

	/* Timeout for the packet command */
	return WAIT_FLOPPY_CMD;
}

/*
 * Called as an interrupt (or directly). When the device says it's ready for a
 * packet, we schedule the packet transfer to occur about 2-3 ticks later in
 * transfer_pc.
 */
static ide_startstop_t idefloppy_start_pc_transfer(ide_drive_t *drive)
{
	idefloppy_floppy_t *floppy = drive->driver_data;
	struct ide_atapi_pc *pc = floppy->pc;
	ide_expiry_t *expiry;
	unsigned int timeout;

	/*
	 * The following delay solves a problem with ATAPI Zip 100 drives
	 * where the Busy flag was apparently being deasserted before the
	 * unit was ready to receive data. This was happening on a
	 * 1200 MHz Athlon system. 10/26/01 25msec is too short,
	 * 40 and 50msec work well. idefloppy_pc_intr will not be actually
	 * used until after the packet is moved in about 50 msec.
	 */
	if (drive->atapi_flags & IDE_AFLAG_ZIP_DRIVE) {
		timeout = floppy->ticks;
		expiry = &idefloppy_transfer_pc;
	} else {
		timeout = WAIT_FLOPPY_CMD;
		expiry = NULL;
	}

	return ide_transfer_pc(drive, pc, idefloppy_pc_intr, timeout, expiry);
}

static void ide_floppy_report_error(idefloppy_floppy_t *floppy,
				    struct ide_atapi_pc *pc)
{
	/* supress error messages resulting from Medium not present */
	if (floppy->sense_key == 0x02 &&
	    floppy->asc       == 0x3a &&
	    floppy->ascq      == 0x00)
		return;

	printk(KERN_ERR "ide-floppy: %s: I/O error, pc = %2x, key = %2x, "
			"asc = %2x, ascq = %2x\n",
			floppy->drive->name, pc->c[0], floppy->sense_key,
			floppy->asc, floppy->ascq);

}

static ide_startstop_t idefloppy_issue_pc(ide_drive_t *drive,
		struct ide_atapi_pc *pc)
{
	idefloppy_floppy_t *floppy = drive->driver_data;

	if (floppy->failed_pc == NULL &&
	    pc->c[0] != GPCMD_REQUEST_SENSE)
		floppy->failed_pc = pc;
	/* Set the current packet command */
	floppy->pc = pc;

	if (pc->retries > IDEFLOPPY_MAX_PC_RETRIES) {
		if (!(pc->flags & PC_FLAG_SUPPRESS_ERROR))
			ide_floppy_report_error(floppy, pc);
		/* Giving up */
		pc->error = IDEFLOPPY_ERROR_GENERAL;

		floppy->failed_pc = NULL;
		drive->pc_callback(drive);
		return ide_stopped;
	}

	debug_log("Retry number - %d\n", pc->retries);

	pc->retries++;

	return ide_issue_pc(drive, pc, idefloppy_start_pc_transfer,
			    WAIT_FLOPPY_CMD, NULL);
}

void ide_floppy_create_read_capacity_cmd(struct ide_atapi_pc *pc)
{
	ide_init_pc(pc);
	pc->c[0] = GPCMD_READ_FORMAT_CAPACITIES;
	pc->c[7] = 255;
	pc->c[8] = 255;
	pc->req_xfer = 255;
}

/* A mode sense command is used to "sense" floppy parameters. */
void ide_floppy_create_mode_sense_cmd(struct ide_atapi_pc *pc, u8 page_code)
{
	u16 length = 8; /* sizeof(Mode Parameter Header) = 8 Bytes */

	ide_init_pc(pc);
	pc->c[0] = GPCMD_MODE_SENSE_10;
	pc->c[1] = 0;
	pc->c[2] = page_code;

	switch (page_code) {
	case IDEFLOPPY_CAPABILITIES_PAGE:
		length += 12;
		break;
	case IDEFLOPPY_FLEXIBLE_DISK_PAGE:
		length += 32;
		break;
	default:
		printk(KERN_ERR "ide-floppy: unsupported page code "
				"in create_mode_sense_cmd\n");
	}
	put_unaligned(cpu_to_be16(length), (u16 *) &pc->c[7]);
	pc->req_xfer = length;
}

static void idefloppy_create_rw_cmd(idefloppy_floppy_t *floppy,
				    struct ide_atapi_pc *pc, struct request *rq,
				    unsigned long sector)
{
	int block = sector / floppy->bs_factor;
	int blocks = rq->nr_sectors / floppy->bs_factor;
	int cmd = rq_data_dir(rq);

	debug_log("create_rw10_cmd: block == %d, blocks == %d\n",
		block, blocks);

	ide_init_pc(pc);
	pc->c[0] = cmd == READ ? GPCMD_READ_10 : GPCMD_WRITE_10;
	put_unaligned(cpu_to_be16(blocks), (unsigned short *)&pc->c[7]);
	put_unaligned(cpu_to_be32(block), (unsigned int *) &pc->c[2]);

	memcpy(rq->cmd, pc->c, 12);

	pc->rq = rq;
	pc->b_count = 0;
	if (rq->cmd_flags & REQ_RW)
		pc->flags |= PC_FLAG_WRITING;
	pc->buf = NULL;
	pc->req_xfer = pc->buf_size = blocks * floppy->block_size;
	pc->flags |= PC_FLAG_DMA_OK;
}

static void idefloppy_blockpc_cmd(idefloppy_floppy_t *floppy,
		struct ide_atapi_pc *pc, struct request *rq)
{
	ide_init_pc(pc);
	memcpy(pc->c, rq->cmd, sizeof(pc->c));
	pc->rq = rq;
	pc->b_count = 0;
	if (rq->data_len && rq_data_dir(rq) == WRITE)
		pc->flags |= PC_FLAG_WRITING;
	pc->buf = rq->data;
	if (rq->bio)
		pc->flags |= PC_FLAG_DMA_OK;
	/*
	 * possibly problematic, doesn't look like ide-floppy correctly
	 * handled scattered requests if dma fails...
	 */
	pc->req_xfer = pc->buf_size = rq->data_len;
}

static ide_startstop_t idefloppy_do_request(ide_drive_t *drive,
		struct request *rq, sector_t block_s)
{
	idefloppy_floppy_t *floppy = drive->driver_data;
	ide_hwif_t *hwif = drive->hwif;
	struct ide_atapi_pc *pc;
	unsigned long block = (unsigned long)block_s;

	debug_log("%s: dev: %s, cmd: 0x%x, cmd_type: %x, errors: %d\n",
		  __func__, rq->rq_disk ? rq->rq_disk->disk_name : "?",
		  rq->cmd[0], rq->cmd_type, rq->errors);

	debug_log("%s: sector: %ld, nr_sectors: %ld, current_nr_sectors: %d\n",
		  __func__, (long)rq->sector, rq->nr_sectors,
		  rq->current_nr_sectors);

	if (rq->errors >= ERROR_MAX) {
		if (floppy->failed_pc)
			ide_floppy_report_error(floppy, floppy->failed_pc);
		else
			printk(KERN_ERR "ide-floppy: %s: I/O error\n",
				drive->name);
		idefloppy_end_request(drive, 0, 0);
		return ide_stopped;
	}
	if (blk_fs_request(rq)) {
		if (((long)rq->sector % floppy->bs_factor) ||
		    (rq->nr_sectors % floppy->bs_factor)) {
			printk(KERN_ERR "%s: unsupported r/w request size\n",
					drive->name);
			idefloppy_end_request(drive, 0, 0);
			return ide_stopped;
		}
		pc = &floppy->queued_pc;
		idefloppy_create_rw_cmd(floppy, pc, rq, block);
	} else if (blk_special_request(rq)) {
		pc = (struct ide_atapi_pc *) rq->buffer;
	} else if (blk_pc_request(rq)) {
		pc = &floppy->queued_pc;
		idefloppy_blockpc_cmd(floppy, pc, rq);
	} else {
		blk_dump_rq_flags(rq,
			"ide-floppy: unsupported command in queue");
		idefloppy_end_request(drive, 0, 0);
		return ide_stopped;
	}

	ide_init_sg_cmd(drive, rq);
	ide_map_sg(drive, rq);

	pc->sg = hwif->sg_table;
	pc->sg_cnt = hwif->sg_nents;

	pc->rq = rq;

	return idefloppy_issue_pc(drive, pc);
}

/*
 * Look at the flexible disk page parameters. We ignore the CHS capacity
 * parameters and use the LBA parameters instead.
 */
static int ide_floppy_get_flexible_disk_page(ide_drive_t *drive)
{
	idefloppy_floppy_t *floppy = drive->driver_data;
	struct gendisk *disk = floppy->disk;
	struct ide_atapi_pc pc;
	u8 *page;
	int capacity, lba_capacity;
	u16 transfer_rate, sector_size, cyls, rpm;
	u8 heads, sectors;

	ide_floppy_create_mode_sense_cmd(&pc, IDEFLOPPY_FLEXIBLE_DISK_PAGE);

	if (ide_queue_pc_tail(drive, disk, &pc)) {
		printk(KERN_ERR "ide-floppy: Can't get flexible disk page"
				" parameters\n");
		return 1;
	}

	if (pc.buf[3] & 0x80)
		drive->atapi_flags |= IDE_AFLAG_WP;
	else
		drive->atapi_flags &= ~IDE_AFLAG_WP;

	set_disk_ro(disk, !!(drive->atapi_flags & IDE_AFLAG_WP));

	page = &pc.buf[8];

	transfer_rate = be16_to_cpup((__be16 *)&pc.buf[8 + 2]);
	sector_size   = be16_to_cpup((__be16 *)&pc.buf[8 + 6]);
	cyls          = be16_to_cpup((__be16 *)&pc.buf[8 + 8]);
	rpm           = be16_to_cpup((__be16 *)&pc.buf[8 + 28]);
	heads         = pc.buf[8 + 4];
	sectors       = pc.buf[8 + 5];

	capacity = cyls * heads * sectors * sector_size;

	if (memcmp(page, &floppy->flexible_disk_page, 32))
		printk(KERN_INFO "%s: %dkB, %d/%d/%d CHS, %d kBps, "
				"%d sector size, %d rpm\n",
				drive->name, capacity / 1024, cyls, heads,
				sectors, transfer_rate / 8, sector_size, rpm);

	memcpy(&floppy->flexible_disk_page, page, 32);
	drive->bios_cyl = cyls;
	drive->bios_head = heads;
	drive->bios_sect = sectors;
	lba_capacity = floppy->blocks * floppy->block_size;

	if (capacity < lba_capacity) {
		printk(KERN_NOTICE "%s: The disk reports a capacity of %d "
			"bytes, but the drive only handles %d\n",
			drive->name, lba_capacity, capacity);
		floppy->blocks = floppy->block_size ?
			capacity / floppy->block_size : 0;
	}
	return 0;
}

/*
 * Determine if a media is present in the floppy drive, and if so, its LBA
 * capacity.
 */
static int ide_floppy_get_capacity(ide_drive_t *drive)
{
	idefloppy_floppy_t *floppy = drive->driver_data;
	struct gendisk *disk = floppy->disk;
	struct ide_atapi_pc pc;
	u8 *cap_desc;
	u8 header_len, desc_cnt;
	int i, rc = 1, blocks, length;

	drive->bios_cyl = 0;
	drive->bios_head = drive->bios_sect = 0;
	floppy->blocks = 0;
	floppy->bs_factor = 1;
	set_capacity(floppy->disk, 0);

	ide_floppy_create_read_capacity_cmd(&pc);
	if (ide_queue_pc_tail(drive, disk, &pc)) {
		printk(KERN_ERR "ide-floppy: Can't get floppy parameters\n");
		return 1;
	}
	header_len = pc.buf[3];
	cap_desc = &pc.buf[4];
	desc_cnt = header_len / 8; /* capacity descriptor of 8 bytes */

	for (i = 0; i < desc_cnt; i++) {
		unsigned int desc_start = 4 + i*8;

		blocks = be32_to_cpup((__be32 *)&pc.buf[desc_start]);
		length = be16_to_cpup((__be16 *)&pc.buf[desc_start + 6]);

		debug_log("Descriptor %d: %dkB, %d blocks, %d sector size\n",
				i, blocks * length / 1024, blocks, length);

		if (i)
			continue;
		/*
		 * the code below is valid only for the 1st descriptor, ie i=0
		 */

		switch (pc.buf[desc_start + 4] & 0x03) {
		/* Clik! drive returns this instead of CAPACITY_CURRENT */
		case CAPACITY_UNFORMATTED:
			if (!(drive->atapi_flags & IDE_AFLAG_CLIK_DRIVE))
				/*
				 * If it is not a clik drive, break out
				 * (maintains previous driver behaviour)
				 */
				break;
		case CAPACITY_CURRENT:
			/* Normal Zip/LS-120 disks */
			if (memcmp(cap_desc, &floppy->cap_desc, 8))
				printk(KERN_INFO "%s: %dkB, %d blocks, %d "
					"sector size\n", drive->name,
					blocks * length / 1024, blocks, length);
			memcpy(&floppy->cap_desc, cap_desc, 8);

			if (!length || length % 512) {
				printk(KERN_NOTICE "%s: %d bytes block size "
					"not supported\n", drive->name, length);
			} else {
				floppy->blocks = blocks;
				floppy->block_size = length;
				floppy->bs_factor = length / 512;
				if (floppy->bs_factor != 1)
					printk(KERN_NOTICE "%s: warning: non "
						"512 bytes block size not "
						"fully supported\n",
						drive->name);
				rc = 0;
			}
			break;
		case CAPACITY_NO_CARTRIDGE:
			/*
			 * This is a KERN_ERR so it appears on screen
			 * for the user to see
			 */
			printk(KERN_ERR "%s: No disk in drive\n", drive->name);
			break;
		case CAPACITY_INVALID:
			printk(KERN_ERR "%s: Invalid capacity for disk "
				"in drive\n", drive->name);
			break;
		}
		debug_log("Descriptor 0 Code: %d\n",
			  pc.buf[desc_start + 4] & 0x03);
	}

	/* Clik! disk does not support get_flexible_disk_page */
	if (!(drive->atapi_flags & IDE_AFLAG_CLIK_DRIVE))
		(void) ide_floppy_get_flexible_disk_page(drive);

	set_capacity(disk, floppy->blocks * floppy->bs_factor);

	return rc;
}

static sector_t idefloppy_capacity(ide_drive_t *drive)
{
	idefloppy_floppy_t *floppy = drive->driver_data;
	unsigned long capacity = floppy->blocks * floppy->bs_factor;

	return capacity;
}

#ifdef CONFIG_IDE_PROC_FS
ide_devset_rw_field(bios_cyl, bios_cyl);
ide_devset_rw_field(bios_head, bios_head);
ide_devset_rw_field(bios_sect, bios_sect);

static int get_ticks(ide_drive_t *drive)
{
	idefloppy_floppy_t *floppy = drive->driver_data;
	return floppy->ticks;
}

static int set_ticks(ide_drive_t *drive, int arg)
{
	idefloppy_floppy_t *floppy = drive->driver_data;
	floppy->ticks = arg;
	return 0;
}

IDE_DEVSET(ticks, DS_SYNC, get_ticks, set_ticks);

static const struct ide_proc_devset idefloppy_settings[] = {
	IDE_PROC_DEVSET(bios_cyl,  0, 1023),
	IDE_PROC_DEVSET(bios_head, 0,  255),
	IDE_PROC_DEVSET(bios_sect, 0,   63),
	IDE_PROC_DEVSET(ticks,	   0,  255),
	{ 0 },
};
#endif

static void idefloppy_setup(ide_drive_t *drive, idefloppy_floppy_t *floppy)
{
	u16 *id = drive->id;
	u8 gcw[2];

	*((u16 *)&gcw) = id[ATA_ID_CONFIG];

	drive->pc_callback = ide_floppy_callback;

	if (((gcw[0] & 0x60) >> 5) == 1)
		drive->atapi_flags |= IDE_AFLAG_DRQ_INTERRUPT;
	/*
	 * We used to check revisions here. At this point however I'm giving up.
	 * Just assume they are all broken, its easier.
	 *
	 * The actual reason for the workarounds was likely a driver bug after
	 * all rather than a firmware bug, and the workaround below used to hide
	 * it. It should be fixed as of version 1.9, but to be on the safe side
	 * we'll leave the limitation below for the 2.2.x tree.
	 */
	if (!strncmp((char *)&id[ATA_ID_PROD], "IOMEGA ZIP 100 ATAPI", 20)) {
		drive->atapi_flags |= IDE_AFLAG_ZIP_DRIVE;
		/* This value will be visible in the /proc/ide/hdx/settings */
		floppy->ticks = IDEFLOPPY_TICKS_DELAY;
		blk_queue_max_sectors(drive->queue, 64);
	}

	/*
	 * Guess what? The IOMEGA Clik! drive also needs the above fix. It makes
	 * nasty clicking noises without it, so please don't remove this.
	 */
	if (strncmp((char *)&id[ATA_ID_PROD], "IOMEGA Clik!", 11) == 0) {
		blk_queue_max_sectors(drive->queue, 64);
		drive->atapi_flags |= IDE_AFLAG_CLIK_DRIVE;
		/* IOMEGA Clik! drives do not support lock/unlock commands */
		drive->atapi_flags |= IDE_AFLAG_NO_DOORLOCK;
	}

	(void) ide_floppy_get_capacity(drive);

	ide_proc_register_driver(drive, floppy->driver);
}

static void ide_floppy_remove(ide_drive_t *drive)
{
	idefloppy_floppy_t *floppy = drive->driver_data;
	struct gendisk *g = floppy->disk;

	ide_proc_unregister_driver(drive, floppy->driver);

	del_gendisk(g);

	ide_floppy_put(floppy);
}

static void idefloppy_cleanup_obj(struct kref *kref)
{
	struct ide_floppy_obj *floppy = to_ide_floppy(kref);
	ide_drive_t *drive = floppy->drive;
	struct gendisk *g = floppy->disk;

	drive->driver_data = NULL;
	g->private_data = NULL;
	put_disk(g);
	kfree(floppy);
}

#ifdef CONFIG_IDE_PROC_FS
static int proc_idefloppy_read_capacity(char *page, char **start, off_t off,
		int count, int *eof, void *data)
{
	ide_drive_t*drive = (ide_drive_t *)data;
	int len;

	len = sprintf(page, "%llu\n", (long long)idefloppy_capacity(drive));
	PROC_IDE_READ_RETURN(page, start, off, count, eof, len);
}

static ide_proc_entry_t idefloppy_proc[] = {
	{ "capacity",	S_IFREG|S_IRUGO, proc_idefloppy_read_capacity,	NULL },
	{ "geometry",	S_IFREG|S_IRUGO, proc_ide_read_geometry,	NULL },
	{ NULL, 0, NULL, NULL }
};
#endif	/* CONFIG_IDE_PROC_FS */

static int ide_floppy_probe(ide_drive_t *);

static ide_driver_t idefloppy_driver = {
	.gen_driver = {
		.owner		= THIS_MODULE,
		.name		= "ide-floppy",
		.bus		= &ide_bus_type,
	},
	.probe			= ide_floppy_probe,
	.remove			= ide_floppy_remove,
	.version		= IDEFLOPPY_VERSION,
	.media			= ide_floppy,
	.do_request		= idefloppy_do_request,
	.end_request		= idefloppy_end_request,
	.error			= __ide_error,
#ifdef CONFIG_IDE_PROC_FS
	.proc			= idefloppy_proc,
	.settings		= idefloppy_settings,
#endif
};

static int idefloppy_open(struct inode *inode, struct file *filp)
{
	struct gendisk *disk = inode->i_bdev->bd_disk;
	struct ide_floppy_obj *floppy;
	ide_drive_t *drive;
	int ret = 0;

	debug_log("Reached %s\n", __func__);

	floppy = ide_floppy_get(disk);
	if (!floppy)
		return -ENXIO;

	drive = floppy->drive;

	floppy->openers++;

	if (floppy->openers == 1) {
		drive->atapi_flags &= ~IDE_AFLAG_FORMAT_IN_PROGRESS;
		/* Just in case */

		if (ide_do_test_unit_ready(drive, disk))
			ide_do_start_stop(drive, disk, 1);

		if (ide_floppy_get_capacity(drive)
		   && (filp->f_flags & O_NDELAY) == 0
		    /*
		     * Allow O_NDELAY to open a drive without a disk, or with an
		     * unreadable disk, so that we can get the format capacity
		     * of the drive or begin the format - Sam
		     */
		    ) {
			ret = -EIO;
			goto out_put_floppy;
		}

		if ((drive->atapi_flags & IDE_AFLAG_WP) && (filp->f_mode & 2)) {
			ret = -EROFS;
			goto out_put_floppy;
		}

		drive->atapi_flags |= IDE_AFLAG_MEDIA_CHANGED;
		ide_set_media_lock(drive, disk, 1);
		check_disk_change(inode->i_bdev);
	} else if (drive->atapi_flags & IDE_AFLAG_FORMAT_IN_PROGRESS) {
		ret = -EBUSY;
		goto out_put_floppy;
	}
	return 0;

out_put_floppy:
	floppy->openers--;
	ide_floppy_put(floppy);
	return ret;
}

static int idefloppy_release(struct inode *inode, struct file *filp)
{
	struct gendisk *disk = inode->i_bdev->bd_disk;
	struct ide_floppy_obj *floppy = ide_floppy_g(disk);
	ide_drive_t *drive = floppy->drive;

	debug_log("Reached %s\n", __func__);

	if (floppy->openers == 1) {
		ide_set_media_lock(drive, disk, 0);
		drive->atapi_flags &= ~IDE_AFLAG_FORMAT_IN_PROGRESS;
	}

	floppy->openers--;

	ide_floppy_put(floppy);

	return 0;
}

static int idefloppy_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
	struct ide_floppy_obj *floppy = ide_floppy_g(bdev->bd_disk);
	ide_drive_t *drive = floppy->drive;

	geo->heads = drive->bios_head;
	geo->sectors = drive->bios_sect;
	geo->cylinders = (u16)drive->bios_cyl; /* truncate */
	return 0;
}

static int ide_floppy_lockdoor(ide_drive_t *drive, struct ide_atapi_pc *pc,
			       unsigned long arg, unsigned int cmd)
{
	idefloppy_floppy_t *floppy = drive->driver_data;
	struct gendisk *disk = floppy->disk;
	int prevent = (arg && cmd != CDROMEJECT) ? 1 : 0;

	if (floppy->openers > 1)
		return -EBUSY;

	ide_set_media_lock(drive, disk, prevent);

	if (cmd == CDROMEJECT)
		ide_do_start_stop(drive, disk, 2);

	return 0;
}

static int idefloppy_ioctl(struct inode *inode, struct file *file,
			unsigned int cmd, unsigned long arg)
{
	struct block_device *bdev = inode->i_bdev;
	struct ide_floppy_obj *floppy = ide_floppy_g(bdev->bd_disk);
	ide_drive_t *drive = floppy->drive;
	struct ide_atapi_pc pc;
	void __user *argp = (void __user *)arg;
	int err;

	if (cmd == CDROMEJECT || cmd == CDROM_LOCKDOOR)
		return ide_floppy_lockdoor(drive, &pc, arg, cmd);

	err = ide_floppy_format_ioctl(drive, file, cmd, argp);
	if (err != -ENOTTY)
		return err;

	/*
	 * skip SCSI_IOCTL_SEND_COMMAND (deprecated)
	 * and CDROM_SEND_PACKET (legacy) ioctls
	 */
	if (cmd != CDROM_SEND_PACKET && cmd != SCSI_IOCTL_SEND_COMMAND)
		err = scsi_cmd_ioctl(file, bdev->bd_disk->queue,
					bdev->bd_disk, cmd, argp);

	if (err == -ENOTTY)
		err = generic_ide_ioctl(drive, file, bdev, cmd, arg);

	return err;
}

static int idefloppy_media_changed(struct gendisk *disk)
{
	struct ide_floppy_obj *floppy = ide_floppy_g(disk);
	ide_drive_t *drive = floppy->drive;
	int ret;

	/* do not scan partitions twice if this is a removable device */
	if (drive->attach) {
		drive->attach = 0;
		return 0;
	}
	ret = !!(drive->atapi_flags & IDE_AFLAG_MEDIA_CHANGED);
	drive->atapi_flags &= ~IDE_AFLAG_MEDIA_CHANGED;
	return ret;
}

static int idefloppy_revalidate_disk(struct gendisk *disk)
{
	struct ide_floppy_obj *floppy = ide_floppy_g(disk);
	set_capacity(disk, idefloppy_capacity(floppy->drive));
	return 0;
}

static struct block_device_operations idefloppy_ops = {
	.owner			= THIS_MODULE,
	.open			= idefloppy_open,
	.release		= idefloppy_release,
	.ioctl			= idefloppy_ioctl,
	.getgeo			= idefloppy_getgeo,
	.media_changed		= idefloppy_media_changed,
	.revalidate_disk	= idefloppy_revalidate_disk
};

static int ide_floppy_probe(ide_drive_t *drive)
{
	idefloppy_floppy_t *floppy;
	struct gendisk *g;

	if (!strstr("ide-floppy", drive->driver_req))
		goto failed;

	if (drive->media != ide_floppy)
		goto failed;

	if (!ide_check_atapi_device(drive, DRV_NAME)) {
		printk(KERN_ERR "ide-floppy: %s: not supported by this version"
				" of ide-floppy\n", drive->name);
		goto failed;
	}
	floppy = kzalloc(sizeof(idefloppy_floppy_t), GFP_KERNEL);
	if (!floppy) {
		printk(KERN_ERR "ide-floppy: %s: Can't allocate a floppy"
				" structure\n", drive->name);
		goto failed;
	}

	g = alloc_disk(1 << PARTN_BITS);
	if (!g)
		goto out_free_floppy;

	ide_init_disk(g, drive);

	kref_init(&floppy->kref);

	floppy->drive = drive;
	floppy->driver = &idefloppy_driver;
	floppy->disk = g;

	g->private_data = &floppy->driver;

	drive->driver_data = floppy;

	idefloppy_setup(drive, floppy);

	g->minors = 1 << PARTN_BITS;
	g->driverfs_dev = &drive->gendev;
	g->flags = drive->removable ? GENHD_FL_REMOVABLE : 0;
	g->fops = &idefloppy_ops;
	drive->attach = 1;
	add_disk(g);
	return 0;

out_free_floppy:
	kfree(floppy);
failed:
	return -ENODEV;
}

static void __exit idefloppy_exit(void)
{
	driver_unregister(&idefloppy_driver.gen_driver);
}

static int __init idefloppy_init(void)
{
	printk("ide-floppy driver " IDEFLOPPY_VERSION "\n");
	return driver_register(&idefloppy_driver.gen_driver);
}

MODULE_ALIAS("ide:*m-floppy*");
MODULE_ALIAS("ide-floppy");
module_init(idefloppy_init);
module_exit(idefloppy_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ATAPI FLOPPY Driver");

