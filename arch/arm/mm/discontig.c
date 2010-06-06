/*
 * linux/arch/arm/mm/discontig.c
 *
 * Discontiguous memory support.
 *
 * Initial code: Copyright (C) 1999-2000 Nicolas Pitre
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/mmzone.h>
#include <linux/bootmem.h>

#if MAX_NUMNODES != 4 && MAX_NUMNODES != 16
# error Fix Me Please
#endif

/*
 * Our node_data structure for discontiguous memory.
 */

pg_data_t discontig_node_data[MAX_NUMNODES] = {
/* FIH_ADQ, Ming { */
/* Fix Section Mismatch Warning */
/*
  { .bdata = &bootmem_node_data[0] },
  { .bdata = &bootmem_node_data[1] },
  { .bdata = &bootmem_node_data[2] },
  { .bdata = &bootmem_node_data[3] },
*/
  { .bootmem_node_id = 0 },
  { .bootmem_node_id = 1 },
  { .bootmem_node_id = 2 },
  { .bootmem_node_id = 3 },
/* } FIH_ADQ, Ming */
#if MAX_NUMNODES == 16
/* FIH_ADQ, Ming { */
/* Fix Section Mismatch Warning */
/*
  { .bdata = &bootmem_node_data[4] },
  { .bdata = &bootmem_node_data[5] },
  { .bdata = &bootmem_node_data[6] },
  { .bdata = &bootmem_node_data[7] },
  { .bdata = &bootmem_node_data[8] },
  { .bdata = &bootmem_node_data[9] },
  { .bdata = &bootmem_node_data[10] },
  { .bdata = &bootmem_node_data[11] },
  { .bdata = &bootmem_node_data[12] },
  { .bdata = &bootmem_node_data[13] },
  { .bdata = &bootmem_node_data[14] },
  { .bdata = &bootmem_node_data[15] },
*/
  { .bootmem_node_id = 4 },
  { .bootmem_node_id = 5 },
  { .bootmem_node_id = 6 },
  { .bootmem_node_id = 7 },
  { .bootmem_node_id = 8 },
  { .bootmem_node_id = 9 },
  { .bootmem_node_id = 10 },
  { .bootmem_node_id = 11 },
  { .bootmem_node_id = 12 },  { .bootmem_node_id = 13 },
  { .bootmem_node_id = 14 },
  { .bootmem_node_id = 15 },
/* } FIH_ADQ, Ming */
#endif
};

EXPORT_SYMBOL(discontig_node_data);
