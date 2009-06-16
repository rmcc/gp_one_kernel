/*
   raid0.c : Multiple Devices driver for Linux
             Copyright (C) 1994-96 Marc ZYNGIER
	     <zyngier@ufr-info-p7.ibp.fr> or
	     <maz@gloups.fdn.fr>
             Copyright (C) 1999, 2000 Ingo Molnar, Red Hat


   RAID-0 management functions.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.
   
   You should have received a copy of the GNU General Public License
   (for example /usr/src/linux/COPYING); if not, write to the Free
   Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  
*/

#include <linux/blkdev.h>
#include <linux/seq_file.h>
#include "md.h"
#include "raid0.h"

static void raid0_unplug(struct request_queue *q)
{
	mddev_t *mddev = q->queuedata;
	raid0_conf_t *conf = mddev_to_conf(mddev);
	mdk_rdev_t **devlist = conf->strip_zone[0].dev;
	int i;

	for (i=0; i<mddev->raid_disks; i++) {
		struct request_queue *r_queue = bdev_get_queue(devlist[i]->bdev);

		blk_unplug(r_queue);
	}
}

static int raid0_congested(void *data, int bits)
{
	mddev_t *mddev = data;
	raid0_conf_t *conf = mddev_to_conf(mddev);
	mdk_rdev_t **devlist = conf->strip_zone[0].dev;
	int i, ret = 0;

	for (i = 0; i < mddev->raid_disks && !ret ; i++) {
		struct request_queue *q = bdev_get_queue(devlist[i]->bdev);

		ret |= bdi_congested(&q->backing_dev_info, bits);
	}
	return ret;
}

static int create_strip_zones(mddev_t *mddev)
{
	int i, c, j, err;
	sector_t curr_zone_end;
	mdk_rdev_t *smallest, *rdev1, *rdev2, *rdev;
	struct strip_zone *zone;
	int cnt;
	char b[BDEVNAME_SIZE];
	raid0_conf_t *conf = kzalloc(sizeof(*conf), GFP_KERNEL);

	if (!conf)
		return -ENOMEM;
	list_for_each_entry(rdev1, &mddev->disks, same_set) {
		printk(KERN_INFO "raid0: looking at %s\n",
			bdevname(rdev1->bdev,b));
		c = 0;
		list_for_each_entry(rdev2, &mddev->disks, same_set) {
			printk(KERN_INFO "raid0:   comparing %s(%llu)",
			       bdevname(rdev1->bdev,b),
			       (unsigned long long)rdev1->sectors);
			printk(KERN_INFO " with %s(%llu)\n",
			       bdevname(rdev2->bdev,b),
			       (unsigned long long)rdev2->sectors);
			if (rdev2 == rdev1) {
				printk(KERN_INFO "raid0:   END\n");
				break;
			}
			if (rdev2->sectors == rdev1->sectors) {
				/*
				 * Not unique, don't count it as a new
				 * group
				 */
				printk(KERN_INFO "raid0:   EQUAL\n");
				c = 1;
				break;
			}
			printk(KERN_INFO "raid0:   NOT EQUAL\n");
		}
		if (!c) {
			printk(KERN_INFO "raid0:   ==> UNIQUE\n");
			conf->nr_strip_zones++;
			printk(KERN_INFO "raid0: %d zones\n",
				conf->nr_strip_zones);
		}
	}
	printk(KERN_INFO "raid0: FINAL %d zones\n", conf->nr_strip_zones);
	err = -ENOMEM;
	conf->strip_zone = kzalloc(sizeof(struct strip_zone)*
				conf->nr_strip_zones, GFP_KERNEL);
	if (!conf->strip_zone)
		goto abort;
	conf->devlist = kzalloc(sizeof(mdk_rdev_t*)*
				conf->nr_strip_zones*mddev->raid_disks,
				GFP_KERNEL);
	if (!conf->devlist)
		goto abort;

	/* The first zone must contain all devices, so here we check that
	 * there is a proper alignment of slots to devices and find them all
	 */
	zone = &conf->strip_zone[0];
	cnt = 0;
	smallest = NULL;
	zone->dev = conf->devlist;
	err = -EINVAL;
	list_for_each_entry(rdev1, &mddev->disks, same_set) {
		int j = rdev1->raid_disk;

		if (j < 0 || j >= mddev->raid_disks) {
			printk(KERN_ERR "raid0: bad disk number %d - "
				"aborting!\n", j);
			goto abort;
		}
		if (zone->dev[j]) {
			printk(KERN_ERR "raid0: multiple devices for %d - "
				"aborting!\n", j);
			goto abort;
		}
		zone->dev[j] = rdev1;

		blk_queue_stack_limits(mddev->queue,
				       rdev1->bdev->bd_disk->queue);
		/* as we don't honour merge_bvec_fn, we must never risk
		 * violating it, so limit ->max_sector to one PAGE, as
		 * a one page request is never in violation.
		 */

		if (rdev1->bdev->bd_disk->queue->merge_bvec_fn &&
		    queue_max_sectors(mddev->queue) > (PAGE_SIZE>>9))
			blk_queue_max_sectors(mddev->queue, PAGE_SIZE>>9);

		if (!smallest || (rdev1->sectors < smallest->sectors))
			smallest = rdev1;
		cnt++;
	}
	if (cnt != mddev->raid_disks) {
		printk(KERN_ERR "raid0: too few disks (%d of %d) - "
			"aborting!\n", cnt, mddev->raid_disks);
		goto abort;
	}
	zone->nb_dev = cnt;
	zone->sectors = smallest->sectors * cnt;
	zone->zone_end = zone->sectors;

	curr_zone_end = zone->sectors;

	/* now do the other zones */
	for (i = 1; i < conf->nr_strip_zones; i++)
	{
		zone = conf->strip_zone + i;
		zone->dev = conf->strip_zone[i-1].dev + mddev->raid_disks;

		printk(KERN_INFO "raid0: zone %d\n", i);
		zone->dev_start = smallest->sectors;
		smallest = NULL;
		c = 0;

		for (j=0; j<cnt; j++) {
			char b[BDEVNAME_SIZE];
			rdev = conf->strip_zone[0].dev[j];
			printk(KERN_INFO "raid0: checking %s ...",
				bdevname(rdev->bdev, b));
			if (rdev->sectors <= zone->dev_start) {
				printk(KERN_INFO " nope.\n");
				continue;
			}
			printk(KERN_INFO " contained as device %d\n", c);
			zone->dev[c] = rdev;
			c++;
			if (!smallest || rdev->sectors < smallest->sectors) {
				smallest = rdev;
				printk(KERN_INFO "  (%llu) is smallest!.\n",
					(unsigned long long)rdev->sectors);
			}
		}

		zone->nb_dev = c;
		zone->sectors = (smallest->sectors - zone->dev_start) * c;
		printk(KERN_INFO "raid0: zone->nb_dev: %d, sectors: %llu\n",
			zone->nb_dev, (unsigned long long)zone->sectors);

		curr_zone_end += zone->sectors;
		zone->zone_end = curr_zone_end;

		printk(KERN_INFO "raid0: current zone start: %llu\n",
			(unsigned long long)smallest->sectors);
	}
	mddev->queue->unplug_fn = raid0_unplug;
	mddev->queue->backing_dev_info.congested_fn = raid0_congested;
	mddev->queue->backing_dev_info.congested_data = mddev;

	printk(KERN_INFO "raid0: done.\n");
	mddev->private = conf;
	return 0;
abort:
	kfree(conf->strip_zone);
	kfree(conf->devlist);
	kfree(conf);
	mddev->private = NULL;
	return err;
}

/**
 *	raid0_mergeable_bvec -- tell bio layer if a two requests can be merged
 *	@q: request queue
 *	@bvm: properties of new bio
 *	@biovec: the request that could be merged to it.
 *
 *	Return amount of bytes we can accept at this offset
 */
static int raid0_mergeable_bvec(struct request_queue *q,
				struct bvec_merge_data *bvm,
				struct bio_vec *biovec)
{
	mddev_t *mddev = q->queuedata;
	sector_t sector = bvm->bi_sector + get_start_sect(bvm->bi_bdev);
	int max;
	unsigned int chunk_sectors = mddev->chunk_size >> 9;
	unsigned int bio_sectors = bvm->bi_size >> 9;

	max =  (chunk_sectors - ((sector & (chunk_sectors - 1)) + bio_sectors)) << 9;
	if (max < 0) max = 0; /* bio_add cannot handle a negative return */
	if (max <= biovec->bv_len && bio_sectors == 0)
		return biovec->bv_len;
	else 
		return max;
}

static sector_t raid0_size(mddev_t *mddev, sector_t sectors, int raid_disks)
{
	sector_t array_sectors = 0;
	mdk_rdev_t *rdev;

	WARN_ONCE(sectors || raid_disks,
		  "%s does not support generic reshape\n", __func__);

	list_for_each_entry(rdev, &mddev->disks, same_set)
		array_sectors += rdev->sectors;

	return array_sectors;
}

static int raid0_run(mddev_t *mddev)
{
	int ret;

	if (mddev->chunk_size == 0) {
		printk(KERN_ERR "md/raid0: non-zero chunk size required.\n");
		return -EINVAL;
	}
	printk(KERN_INFO "%s: setting max_sectors to %d, segment boundary to %d\n",
	       mdname(mddev),
	       mddev->chunk_size >> 9,
	       (mddev->chunk_size>>1)-1);
	blk_queue_max_sectors(mddev->queue, mddev->chunk_size >> 9);
	blk_queue_segment_boundary(mddev->queue, (mddev->chunk_size>>1) - 1);
	mddev->queue->queue_lock = &mddev->queue->__queue_lock;

	ret = create_strip_zones(mddev);
	if (ret < 0)
		return ret;

	/* calculate array device size */
	md_set_array_sectors(mddev, raid0_size(mddev, 0, 0));

	printk(KERN_INFO "raid0 : md_size is %llu sectors.\n",
		(unsigned long long)mddev->array_sectors);
	/* calculate the max read-ahead size.
	 * For read-ahead of large files to be effective, we need to
	 * readahead at least twice a whole stripe. i.e. number of devices
	 * multiplied by chunk size times 2.
	 * If an individual device has an ra_pages greater than the
	 * chunk size, then we will not drive that device as hard as it
	 * wants.  We consider this a configuration error: a larger
	 * chunksize should be used in that case.
	 */
	{
		int stripe = mddev->raid_disks * mddev->chunk_size / PAGE_SIZE;
		if (mddev->queue->backing_dev_info.ra_pages < 2* stripe)
			mddev->queue->backing_dev_info.ra_pages = 2* stripe;
	}

	blk_queue_merge_bvec(mddev->queue, raid0_mergeable_bvec);
	return 0;
}

static int raid0_stop (mddev_t *mddev)
{
	raid0_conf_t *conf = mddev_to_conf(mddev);

	blk_sync_queue(mddev->queue); /* the unplug fn references 'conf'*/
	kfree(conf->strip_zone);
	conf->strip_zone = NULL;
	kfree(conf);
	mddev->private = NULL;

	return 0;
}

/* Find the zone which holds a particular offset */
static struct strip_zone *find_zone(struct raid0_private_data *conf,
		sector_t sector)
{
	int i;
	struct strip_zone *z = conf->strip_zone;

	for (i = 0; i < conf->nr_strip_zones; i++)
		if (sector < z[i].zone_end)
			return z + i;
	BUG();
}

static int raid0_make_request (struct request_queue *q, struct bio *bio)
{
	mddev_t *mddev = q->queuedata;
	unsigned int sect_in_chunk, chunksect_bits, chunk_sects;
	raid0_conf_t *conf = mddev_to_conf(mddev);
	struct strip_zone *zone;
	mdk_rdev_t *tmp_dev;
	sector_t chunk;
	sector_t sector, rsect;
	const int rw = bio_data_dir(bio);
	int cpu;

	if (unlikely(bio_barrier(bio))) {
		bio_endio(bio, -EOPNOTSUPP);
		return 0;
	}

	cpu = part_stat_lock();
	part_stat_inc(cpu, &mddev->gendisk->part0, ios[rw]);
	part_stat_add(cpu, &mddev->gendisk->part0, sectors[rw],
		      bio_sectors(bio));
	part_stat_unlock();

	chunk_sects = mddev->chunk_size >> 9;
	chunksect_bits = ffz(~chunk_sects);
	sector = bio->bi_sector;

	if (unlikely(chunk_sects < (bio->bi_sector & (chunk_sects - 1)) + (bio->bi_size >> 9))) {
		struct bio_pair *bp;
		/* Sanity check -- queue functions should prevent this happening */
		if (bio->bi_vcnt != 1 ||
		    bio->bi_idx != 0)
			goto bad_map;
		/* This is a one page bio that upper layers
		 * refuse to split for us, so we need to split it.
		 */
		bp = bio_split(bio, chunk_sects - (bio->bi_sector & (chunk_sects - 1)));
		if (raid0_make_request(q, &bp->bio1))
			generic_make_request(&bp->bio1);
		if (raid0_make_request(q, &bp->bio2))
			generic_make_request(&bp->bio2);

		bio_pair_release(bp);
		return 0;
	}
	zone = find_zone(conf, sector);
	sect_in_chunk = bio->bi_sector & (chunk_sects - 1);
	{
		sector_t x = (zone->sectors + sector - zone->zone_end)
				>> chunksect_bits;

		sector_div(x, zone->nb_dev);
		chunk = x;

		x = sector >> chunksect_bits;
		tmp_dev = zone->dev[sector_div(x, zone->nb_dev)];
	}
	rsect = (chunk << chunksect_bits) + zone->dev_start + sect_in_chunk;
 
	bio->bi_bdev = tmp_dev->bdev;
	bio->bi_sector = rsect + tmp_dev->data_offset;

	/*
	 * Let the main block layer submit the IO and resolve recursion:
	 */
	return 1;

bad_map:
	printk("raid0_make_request bug: can't convert block across chunks"
		" or bigger than %dk %llu %d\n", chunk_sects / 2,
		(unsigned long long)bio->bi_sector, bio->bi_size >> 10);

	bio_io_error(bio);
	return 0;
}

static void raid0_status (struct seq_file *seq, mddev_t *mddev)
{
#undef MD_DEBUG
#ifdef MD_DEBUG
	int j, k, h;
	char b[BDEVNAME_SIZE];
	raid0_conf_t *conf = mddev_to_conf(mddev);

	h = 0;
	for (j = 0; j < conf->nr_strip_zones; j++) {
		seq_printf(seq, "      z%d", j);
		seq_printf(seq, "=[");
		for (k = 0; k < conf->strip_zone[j].nb_dev; k++)
			seq_printf(seq, "%s/", bdevname(
				conf->strip_zone[j].dev[k]->bdev,b));

		seq_printf(seq, "] ze=%d ds=%d s=%d\n",
				conf->strip_zone[j].zone_end,
				conf->strip_zone[j].dev_start,
				conf->strip_zone[j].sectors);
	}
#endif
	seq_printf(seq, " %dk chunks", mddev->chunk_size/1024);
	return;
}

static struct mdk_personality raid0_personality=
{
	.name		= "raid0",
	.level		= 0,
	.owner		= THIS_MODULE,
	.make_request	= raid0_make_request,
	.run		= raid0_run,
	.stop		= raid0_stop,
	.status		= raid0_status,
	.size		= raid0_size,
};

static int __init raid0_init (void)
{
	return register_md_personality (&raid0_personality);
}

static void raid0_exit (void)
{
	unregister_md_personality (&raid0_personality);
}

module_init(raid0_init);
module_exit(raid0_exit);
MODULE_LICENSE("GPL");
MODULE_ALIAS("md-personality-2"); /* RAID0 */
MODULE_ALIAS("md-raid0");
MODULE_ALIAS("md-level-0");
