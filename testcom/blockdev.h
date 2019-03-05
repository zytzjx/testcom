/*
 * Copyright (C) 2006-2011 Apple Computer, Inc. All rights reserved.
 *
 * This document is the property of Apple Computer, Inc.
 * It is considered confidential and proprietary.
 *
 * This document may not be reproduced or transmitted in any form,
 * in whole or in part, without the express written permission of
 * Apple Computer, Inc.
 */
#ifndef __LIB_BLOCKDEV_H
#define __LIB_BLOCKDEV_H

#include <stdint.h>
#include "types.h"

__BEGIN_DECLS

typedef uint32_t block_addr;

#define BLOCKDEV_FLAG_NEEDS_ERASE (1 << 0) /* writes do not implicitly erase */
#define BLOCKDEV_FLAG_UPGRADE_PARTITION (1 << 1)

typedef int64_t     		off_t;
struct blockdev {
	struct blockdev *next;
	uint32_t flags;

	/* geometry */
	uint32_t block_size;
	uint32_t block_count;

	uint32_t block_shift; /* calculated from the block size (assumes multiple of 2) */
	uint64_t total_len;

	uint32_t alignment; /* required alignment for source/destination buffers in bytes */
	uint32_t alignment_shift;

	/* io hooks */
	int(*read_hook)(struct blockdev *, void *ptr, off_t offset, uint64_t len); // read bytes
	int(*read_block_hook)(struct blockdev *, void *ptr, block_addr block, uint32_t count); // read count blocks
	int(*write_hook)(struct blockdev *, const void *ptr, off_t offset, uint64_t len); // write bytes
	int(*write_block_hook)(struct blockdev *, const void *ptr, block_addr block, uint32_t count); // write count blocks
	int(*erase_hook)(struct blockdev *, off_t offset, uint64_t len);

	/* name */
	char name[16];

	/* write-protected region */
	off_t protect_start;
	off_t protect_end;
};

struct blockdev *lookup_blockdev(const char *name);
int register_blockdev(struct blockdev *);
struct blockdev *first_blockdev(void);
struct blockdev *next_blockdev(struct blockdev *dev);

/* fill out a new blockdev structure with geometry data and default hooks */
int construct_blockdev(struct blockdev *, const char *name, uint64_t len, uint32_t block_size);

/* user api */
#define blockdev_read(bdev, ptr, off, len) (bdev)->read_hook((bdev), ptr, off, len)	
#define blockdev_read_block(bdev, ptr, block, count) (bdev)->read_block_hook((bdev), ptr, block, count)	
#define blockdev_write(bdev, ptr, off, len) blockdev_write_protected((bdev), ptr, off, len)	
#define blockdev_write_block(bdev, ptr, block, count) blockdev_write_block_protected((bdev), ptr, block, count)	
#define blockdev_erase(bdev, off, len) (bdev)->erase_hook((bdev), off, len)

int blockdev_compare(struct blockdev *dev, const void *ptr, off_t bdev_offset, uint64_t len);
int blockdev_set_protection(struct blockdev *dev, off_t offset, uint64_t length);
void blockdev_set_buffer_alignment(struct blockdev *dev, uint32_t alignment);
int blockdev_write_protected(struct blockdev *dev, const void *ptr, off_t offset, uint64_t len);
int blockdev_write_block_protected(struct blockdev *dev, const void *ptr, block_addr block, uint32_t count);

/* a default memory based block device */
struct blockdev *create_mem_blockdev(const char *name, void *ptr, uint64_t len, uint32_t block_size);

/* a subdevice device, which lets you access a range of a device as another device */
struct blockdev *create_subdev_blockdev(const char *name, struct blockdev *parent, off_t off, uint64_t len, uint32_t block_size);

__END_DECLS

#endif