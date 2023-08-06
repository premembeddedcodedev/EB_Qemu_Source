/**
 * Copyright (c) 2013 Anup Patel.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * @file vmm_virtio_blk.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief VirtIO Block Device Interface.
 *
 * This header has been derived from linux kernel source:
 * <linux_source>/include/uapi/linux/virtio_blk.h
 *
 * The original header is BSD licensed.
 */

/* This header is BSD licensed so anyone can use the definitions to implement
 * compatible drivers/servers.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of IBM nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL IBM OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef __VMM_VIRTIO_BLK_H__
#define __VMM_VIRTIO_BLK_H__

#include <vmm_types.h>

/* Feature bits */
#define VMM_VIRTIO_BLK_F_SIZE_MAX	1	/* Indicates maximum segment size */
#define VMM_VIRTIO_BLK_F_SEG_MAX	2	/* Indicates maximum # of segments */
#define VMM_VIRTIO_BLK_F_GEOMETRY	4	/* Legacy geometry available  */
#define VMM_VIRTIO_BLK_F_RO		5	/* Disk is read-only */
#define VMM_VIRTIO_BLK_F_BLK_SIZE	6	/* Block size of disk is available*/
#define VMM_VIRTIO_BLK_F_TOPOLOGY	10	/* Topology information is available */
#define VMM_VIRTIO_BLK_F_MQ		12	/* support more than one vq */
#define VMM_VIRTIO_BLK_F_DISCARD	13	/* DISCARD is supported */
#define VMM_VIRTIO_BLK_F_WRITE_ZEROES	14	/* WRITE ZEROES is supported */

/* Legacy feature bits */
#ifndef VMM_VIRTIO_BLK_NO_LEGACY
#define VMM_VIRTIO_BLK_F_BARRIER	0	/* Does host support barriers? */
#define VMM_VIRTIO_BLK_F_SCSI		7	/* Supports scsi command passthru */
#define VMM_VIRTIO_BLK_F_FLUSH		9	/* Flush command supported */
#define VMM_VIRTIO_BLK_F_CONFIG_WCE	11	/* Writeback mode available in config */
/* Old (deprecated) name for VIRTIO_BLK_F_FLUSH. */
#define VMM_VIRTIO_BLK_F_WCE		VMM_VIRTIO_BLK_F_FLUSH
#endif /* !VMM_VIRTIO_BLK_NO_LEGACY */

#define VMM_VIRTIO_BLK_ID_BYTES		20	/* ID string length */

struct vmm_virtio_blk_config {
	/* The capacity (in 512-byte sectors). */
	u64 capacity;
	/* The maximum segment size (if VMM_VIRTIO_BLK_F_SIZE_MAX) */
	u32 size_max;
	/* The maximum number of segments (if VMM_VIRTIO_BLK_F_SEG_MAX) */
	u32 seg_max;
	/* geometry the device (if VMM_VIRTIO_BLK_F_GEOMETRY) */
	struct vmm_virtio_blk_geometry {
		u16 cylinders;
		u8 heads;
		u8 sectors;
	} geometry;

	/* block size of device (if VMM_VIRTIO_BLK_F_BLK_SIZE) */
	u32 blk_size;

	/* the next 4 entries are guarded by VMM_VIRTIO_BLK_F_TOPOLOGY  */
	/* exponent for physical block per logical block. */
	u8 physical_block_exp;
	/* alignment offset in logical blocks. */
	u8 alignment_offset;
	/* minimum I/O size without performance penalty in logical blocks. */
	u16 min_io_size;
	/* optimal sustained I/O size in logical blocks. */
	u32 opt_io_size;

	/* writeback mode (if VMM_VIRTIO_BLK_F_CONFIG_WCE) */
	u8 wce;
	u8 unused;

	/* number of vqs, only available when VMM_VIRTIO_BLK_F_MQ is set */
	u16 num_queues;

	/* the next 3 entries are guarded by VMM_VIRTIO_BLK_F_DISCARD */
	/*
	 * The maximum discard sectors (in 512-byte sectors) for
	 * one segment.
	 */
	u32 max_discard_sectors;
	/*
	 * The maximum number of discard segments in a
	 * discard command.
	 */
	u32 max_discard_seg;
	/* Discard commands must be aligned to this number of sectors. */
	u32 discard_sector_alignment;

	/* the next 3 entries are guarded by VMM_VIRTIO_BLK_F_WRITE_ZEROES */
	/*
	 * The maximum number of write zeroes sectors (in 512-byte sectors) in
	 * one segment.
	 */
	u32 max_write_zeroes_sectors;
	/*
	 * The maximum number of segments in a write zeroes
	 * command.
	 */
	u32 max_write_zeroes_seg;
	/*
	 * Set if a VMM_VIRTIO_BLK_T_WRITE_ZEROES request may result in the
	 * deallocation of one or more of the sectors.
	 */
	u8 write_zeroes_may_unmap;

	u8 unused1[3];
} __attribute__((packed));

/*
 * Command types
 *
 * Usage is a bit tricky as some bits are used as flags and some are not.
 *
 * Rules:
 *   VMM_VIRTIO_BLK_T_OUT may be combined with VMM_VIRTIO_BLK_T_SCSI_CMD or
 *   VMM_VIRTIO_BLK_T_BARRIER. VMM_VIRTIO_BLK_T_FLUSH is a command of its own
 *   and may not be combined with any of the other flags.
 */

/* These two define direction. */
#define VMM_VIRTIO_BLK_T_IN		0
#define VMM_VIRTIO_BLK_T_OUT		1

/* This bit says it's a scsi command, not an actual read or write. */
#define VMM_VIRTIO_BLK_T_SCSI_CMD	2

/* Cache flush command */
#define VMM_VIRTIO_BLK_T_FLUSH		4

/* Get device ID command */
#define VMM_VIRTIO_BLK_T_GET_ID		8

/* Discard command */
#define VMM_VIRTIO_BLK_T_DISCARD	11

/* Write zeroes command */
#define VMM_VIRTIO_BLK_T_WRITE_ZEROES	13

/* Barrier before this op. */
#define VMM_VIRTIO_BLK_T_BARRIER	0x80000000

/* This is the first element of the read scatter-gather list. */
struct vmm_virtio_blk_outhdr {
	/* VMM_VIRTIO_BLK_T */
	u32 type;
	/* io priority. */
	u32 ioprio;
	/* Sector (ie. 512 byte offset) */
	u64 sector;
} __attribute__((packed));

/* Unmap this range (only valid for write zeroes command) */
#define VMM_VIRTIO_BLK_WRITE_ZEROES_FLAG_UNMAP	0x00000001

/* Discard/write zeroes range for each request. */
struct virtio_blk_discard_write_zeroes {
	/* discard/write zeroes start sector */
	u64 sector;
	/* number of discard/write zeroes sectors */
	u32 num_sectors;
	/* flags for this range */
	u32 flags;
} __attribute__((packed));

struct vmm_virtio_scsi_inhdr {
	u32 errors;
	u32 data_len;
	u32 sense_len;
	u32 residual;
} __attribute__((packed));

/* And this is the final byte of the write scatter-gather list. */
#define VMM_VIRTIO_BLK_S_OK		0
#define VMM_VIRTIO_BLK_S_IOERR		1
#define VMM_VIRTIO_BLK_S_UNSUPP		2

#endif /* __VMM_VIRTIO_BLK_H__ */
