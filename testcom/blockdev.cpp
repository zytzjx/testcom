#include "pch.h"
#include "blockdev.h"
/*
 * Copyright (C) 2006-2014 Apple Computer, Inc. All rights reserved.
 *
 * This document is the property of Apple Computer, Inc.
 * It is considered confidential and proprietary.
 *
 * This document may not be reproduced or transmitted in any form,
 * in whole or in part, without the express written permission of
 * Apple Computer, Inc.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <assert.h>

static struct blockdev *bdev_list = NULL;

#define BLOCKSIZE_MOD(dev, val) ((val) & ((1<<((dev)->block_shift)) - 1))
#define BLOCK_FROM_OFF(dev, off) ((off) >> (dev)->block_shift)
#define BLOCK_TO_OFF(dev, block) ((block) << (dev)->block_shift)
#define ALIGNMENT_MOD(dev, val) ((val) & ((1<<((dev)->alignment_shift)) - 1))

#define posix_memalign(p, a, s) (((*(p)) = _aligned_malloc((s), (a))), *(p) ?0 :errno)

#ifdef WIN32
#include <intrin.h>
static uint32_t __inline __builtin_clz(uint32_t x) {
	unsigned long r = 0;
	_BitScanReverse(&r, x);
	return (31 - r);
}
#endif

int log2_int(unsigned int val)
{
	if (val == 0)
		return 0;

	return 31 - __builtin_clz(val);
}


static size_t get_cpu_cache_line_size(void)
{
#define CPU_CACHELINE_SIZE 64
#ifdef CPU_CACHELINE_SIZE
	return CPU_CACHELINE_SIZE;
#endif
}

struct blockdev *lookup_blockdev(const char *name)
{
	struct blockdev *dev;

	for (dev = bdev_list; dev; dev = dev->next) {
		if (strcmp(dev->name, name) == 0)
			return dev;
	}

	return NULL;
}

struct blockdev *first_blockdev(void)
{
	return bdev_list;
}

struct blockdev *next_blockdev(struct blockdev *dev)
{
	return dev->next;
}

int register_blockdev(struct blockdev *dev)
{
	assert(dev != NULL);
	assert(dev->block_size > 0);

	printf( "register_blockdev: dev %p '%s'\n", dev, dev->name);

	dev->next = bdev_list;
	bdev_list = dev;

	return 0;
}

static void *get_bounce_buf(struct blockdev *dev)
{
	void *result;
	uint32_t align = __max(dev->alignment, (uint32_t)get_cpu_cache_line_size());
	uint32_t size = __max(dev->alignment, dev->block_size);

	posix_memalign(&result, align, size);
	return result;
}

/* a reasonable default bytewise read routine in case the block device does not
 * choose to override the read_hook. deblocks the device by reading from the read_block hook.
 */
static int blockdev_read_hook(struct blockdev *dev, void *ptr, off_t offset, uint64_t len)
{
	int err;
	size_t bytes_read;
	block_addr block;
	size_t buf_offset;
	char *buf = NULL;

	/* trim the read len */
	assert(offset >= 0);
	if ((uint64_t)offset >= dev->total_len)
		return 0;
	if ((offset + len) > dev->total_len)
		len = dev->total_len - offset;

	buf_offset = 0;
	bytes_read = 0;
	block = BLOCK_FROM_OFF(dev, offset);

	//	dprintf(DEBUG_INFO, "read: ptr %p, buf_offset %d, bytes_read %zd, block %u, len %d, offset %lld\n", ptr, buf_offset, bytes_read, block, len, offset);

		/* read in partial first block */
	if (BLOCKSIZE_MOD(dev, offset) != 0 || ALIGNMENT_MOD(dev, (uintptr_t)ptr) != 0) {
		size_t block_off = BLOCKSIZE_MOD(dev, offset);
		size_t toread = __min(len, dev->block_size - block_off);

		buf = (char *)get_bounce_buf(dev);

		err = blockdev_read_block(dev, buf, block, 1);
		if (err <= 0)
			goto done;

		/* copy the partial data */
		memcpy(ptr, buf + block_off, toread);

		block++;
		offset += toread;
		buf_offset += toread;
		bytes_read += toread;
		len -= toread;
	}

	//	dprintf(DEBUG_INFO, "read2: ptr %p, buf_offset %d, bytes_read %zd, block %u, len %d, offset %lld, blockshift %d\n", ptr, buf_offset, bytes_read, block, len, offset, dev->block_shift);

	assert(len == 0 || BLOCKSIZE_MOD(dev, offset) == 0);

	/* handle multiple middle blocks */
	while (len >= dev->block_size) {
		size_t blocks_to_read;
		size_t alignment_offset;

		blocks_to_read = BLOCK_FROM_OFF(dev, len);

		/* If the buffer we're writing to isn't aligned, shorten the read by a block and
		   bounce through an aligned address higher up in the buffer. This relies on the block
		   size being >= the alignment, which is enforced by blockdev_set_buffer_alignment.
		   For reads of just one block, go through the bounce buffer */
		alignment_offset = ALIGNMENT_MOD(dev, (uintptr_t)ptr + buf_offset);
		if (alignment_offset != 0) {
			if (blocks_to_read > 1) {
				uint8_t *dest;
				dest = (uint8_t *)ptr + buf_offset + dev->alignment - alignment_offset;

				err = blockdev_read_block(dev, dest, block, blocks_to_read - 1);
				if (err > 0)
					memmove((uint8_t *)ptr + buf_offset, dest, BLOCK_TO_OFF(dev, err));
			}
			else {
				if (!buf)
					buf = (char *)get_bounce_buf(dev);

				err = blockdev_read_block(dev, buf, block, 1);
				if (err > 0)
					memmove((uint8_t *)ptr + buf_offset, buf, BLOCK_TO_OFF(dev, 1));
			}
		}
		else {
			err = blockdev_read_block(dev, (uint8_t *)ptr + buf_offset, block, blocks_to_read);
		}

		if (err <= 0)
			goto done;

		/* account for blocks read */
		block += err;
		buf_offset += BLOCK_TO_OFF(dev, err);
		bytes_read += BLOCK_TO_OFF(dev, err);
		len -= BLOCK_TO_OFF(dev, err);
	}

	//	dprintf(DEBUG_INFO, "read3: ptr %p, buf_offset %d, bytes_read %zd, block %ud, len %d, offset %lld\n", ptr, buf_offset, bytes_read, block, len, offset);

		/* handle partial last block */
	if (len > 0) {
		assert(len < dev->block_size);

		if (!buf)
			buf = (char *)get_bounce_buf(dev);

		err = blockdev_read_block(dev, buf, block, 1);
		if (err <= 0)
			goto done;

		memcpy((uint8_t *)ptr + buf_offset, buf, len);

		bytes_read += len;
	}

done:
	if (NULL != buf)
		free(buf);

	return bytes_read;
}


static int blockdev_read_block_hook(struct blockdev *dev, void *ptr, block_addr block, uint32_t count)
{
	printf("no reasonable default block read routine\n");

	return -1;
}

static int blockdev_write_hook(struct blockdev *dev, const void *ptr, off_t offset, uint64_t len)
{
	int err;
	size_t bytes_written;
	block_addr block;
	size_t buf_offset;
	char *buf = NULL;
	const void *src;

	/* trim the write len */
	assert(offset >= 0);
	if ((uint64_t)offset >= dev->total_len)
		return 0;
	if ((offset + len) > dev->total_len)
		len = dev->total_len - offset;

	buf_offset = 0;
	bytes_written = 0;
	block = BLOCK_FROM_OFF(dev, offset);

	//	dprintf(DEBUG_INFO, "write: ptr %p, buf_offset %d, bytes_written %zd, block %u, len %d, offset %lld\n", ptr, buf_offset, bytes_written, block, len, offset);

		/* write partial first block */
	if (BLOCKSIZE_MOD(dev, offset) != 0 || ALIGNMENT_MOD(dev, offset) != 0) {
		size_t block_off = BLOCKSIZE_MOD(dev, offset);
		size_t towrite = __min(len, dev->block_size - block_off);

		buf = (char *)get_bounce_buf(dev);

		/* read in a buffer */
		err = blockdev_read_block(dev, buf, block, 1);
		if (err <= 0)
			goto done;

		/* copy the partial data */
		memcpy(buf + block_off, ptr, towrite);

		/* write it back out */
		err = blockdev_write_block(dev, buf, block, 1);
		if (err <= 0)
			goto done;

		block++;
		offset += towrite;
		buf_offset += towrite;
		bytes_written += towrite;
		len -= towrite;
	}

	//	dprintf(DEBUG_INFO, "write2: ptr %p, buf_offset %d, bytes_written %zd, block %u, len %d, offset %lld, blockshift %d\n", ptr, buf_offset, bytes_written, block, len, offset, dev->block_shift);

	assert(len == 0 || BLOCKSIZE_MOD(dev, offset) == 0);

	/* handle multiple middle blocks */
	while (len >= dev->block_size) {
		size_t blocks_to_write;
		size_t alignment_offset;

		/* If the buffer we're reading from isn't aligned, bounce buffer each block. iBoot
		   doesn't care about write performance for big writes, so it's ok for this to be slow */
		alignment_offset = ALIGNMENT_MOD(dev, (uintptr_t)ptr + buf_offset);
		if (alignment_offset != 0) {
			if (!buf)
				buf = (char *)get_bounce_buf(dev);

			blocks_to_write = 1;
			memcpy(buf, (const uint8_t *)ptr + buf_offset, BLOCK_TO_OFF(dev, blocks_to_write));
			src = buf;
		}
		else {
			blocks_to_write = BLOCK_FROM_OFF(dev, len);
			src = (const uint8_t *)ptr + buf_offset;
		}

		err = blockdev_write_block(dev, src, block, blocks_to_write);
		if (err <= 0)
			goto done;

		/* account for write */
		block += err;
		buf_offset += BLOCK_TO_OFF(dev, err);
		bytes_written += BLOCK_TO_OFF(dev, err);
		len -= BLOCK_TO_OFF(dev, err);
	}

	//	dprintf(DEBUG_INFO, "write3: ptr %p, buf_offset %d, bytes_written %zd, block %ud, len %d, offset %lld\n", ptr, buf_offset, bytes_written, block, len, offset);

		/* handle partial last block */
	if (len > 0) {
		assert(len < dev->block_size);

		if (!buf)
			buf = (char *)get_bounce_buf(dev);

		err = blockdev_read_block(dev, buf, block, 1);
		if (err <= 0)
			goto done;

		memcpy(buf, (uint8_t *)ptr + buf_offset, len);

		err = blockdev_write_block(dev, buf, block, 1);
		if (err <= 0)
			goto done;

		bytes_written += len;
	}

done:
	if (NULL != buf)
		free(buf);

	return bytes_written;
}

static int blockdev_write_block_hook(struct blockdev *dev, const void *ptr, block_addr block, uint32_t count)
{
	printf("no reasonable default block write routine\n");

	return -1;
}

static int blockdev_erase_hook(struct blockdev *dev, off_t offset, uint64_t len)
{
	printf("no reasonable default erase routine\n");

	return -1;
}

/* compare memory with a range of the block device */
int blockdev_compare(struct blockdev *dev, const void *_ptr, off_t bdev_offset, uint64_t len)
{
	int result;
	int err;
	uint32_t i;
	uint8_t *buf;
	const uint8_t *ptr = (const uint8_t *)_ptr;

	posix_memalign((void **)&buf, get_cpu_cache_line_size(), dev->block_size);

	result = 0;
	while (len > 0) {
		size_t toread = __min(dev->block_size, len);

		err = blockdev_read(dev, buf, bdev_offset, toread);
		if (err <= 0) {
			printf("bdev_compare: error reading from device at offset %lld\n", bdev_offset);
			result = -1;
			goto out;
		}
		if ((uint32_t)err < toread) {
			printf("bdev_compare: short read from device at offset %lld (read %d)\n", bdev_offset, err);
			result = -1;
			goto out;
		}

		for (i = 0; i < toread; i++) {
			if (buf[i] != *ptr) {
				printf("difference at offset %lld: dev 0x%02x mem 0x%02x\n", bdev_offset + i, buf[i], *ptr);
				result++;
			}
			ptr++;
		}

		len -= toread;
		bdev_offset += toread;
	}

out:
	free(buf);
	return result;
}

/* write to the bdev safely, avoiding the protected region */
int blockdev_write_protected(struct blockdev *dev, const void *ptr, off_t offset, uint64_t len)
{
	size_t	overlap;
	off_t	protect_start, protect_end;
	uint64_t total_length;
	int err;
	off_t bytes_written = 0;

	//	printf("write_protected: ptr %p  offset 0x%llx length 0x%x\n", ptr, offset, len);
	protect_start = dev->protect_start;
	protect_end = dev->protect_end;
	total_length = (uint64_t)offset + len;
	// Test for negative offset and overlap
	assert(offset >= 0);
	assert(total_length >= len);

	/* write the portion before the protected region */
	if (offset < protect_start) {
		assert(protect_start >= 0);
		if ((offset + len) > (size_t)protect_start) {
			overlap = (offset + len) - protect_start;
		}
		else {
			overlap = 0;
		}
		//		printf("BDEV: writing first part: 0x%llx/0x%x\n", offset, len - overlap);
		err = dev->write_hook(dev, ptr, offset, len - overlap);
		if (err < 0)
			return err;
		bytes_written += err;
	}

	/* and write the portion after */
	assert(protect_end >= 0);
	if ((offset + len) > (size_t)protect_end) {
		if (offset < protect_end) {
			overlap = protect_end - offset;
		}
		else {
			overlap = 0;
		}
		//		printf("BDEV: writing second part: 0x%llx/0x%x\n", offset + overlap, len - overlap);
		err = dev->write_hook(dev, (char *)ptr + overlap, offset + overlap, len - overlap);
		if (err < 0)
			return err;
		bytes_written += err;
	}
	return bytes_written;
}


int blockdev_write_block_protected(struct blockdev *dev, const void *ptr, block_addr block, uint32_t count)
{
	size_t	overlap;
	size_t	protect_start, protect_end;
	int	err;
	uint32_t total_count;

	//	printf("write_block_protected: ptr %p  block 0x%x count 0x%x\n", ptr, block, count);
	protect_start = dev->protect_start >> dev->block_shift;
	protect_end = dev->protect_end >> dev->block_shift;
	total_count = block + count;
	// Test for overlap
	assert(total_count >= block);

	/* write the portion before the protected region */
	if (block < protect_start) {
		if ((block + count) > protect_start) {
			overlap = (block + count) - protect_start;
		}
		else {
			overlap = 0;
		}
		//		printf("BDEV: writing first part: 0x%x/0x%x\n", block, count - overlap);
		err = dev->write_block_hook(dev, ptr, block, count - overlap);
		if (err < 0)			/* error */
			return(-1);
		if ((uint32_t)err < (count - overlap))	/* short write */
			return(err);

	}

	/* and write the portion after */
	if ((block + count) > protect_end) {
		if (block < protect_end) {
			overlap = protect_end - block;
		}
		else {
			overlap = 0;
		}
		//		printf("BDEV: writing second part: 0x%x/0x%x\n", block + overlap, count - overlap);
		err = dev->write_block_hook(dev, (char *)ptr + (overlap << dev->block_shift), block + overlap, count - overlap);
		if (err < 0)			/* error */
			return(-1);
		if ((uint32_t)err < (count - overlap))	/* short write XXX does not handle both ends
							 * overhanging */
			return(err);
	}
	return(count);
}

/* set the protected region */
int blockdev_set_protection(struct blockdev *dev, off_t offset, uint64_t length)
{
	if (dev->protect_end == dev->protect_start) {
		dev->protect_start = offset;
		dev->protect_end = offset + length;
		printf("BDEV: protecting 0x%llx-0x%llx\n", dev->protect_start, dev->protect_end);
		return(0);
	}
	return(-1);
}

/* set the required buffer alignment for read and write buffers */
void blockdev_set_buffer_alignment(struct blockdev *dev, uint32_t alignment)
{
	assert(dev != NULL);

	//assert(is_pow2(alignment));

	/* Standard hooks don't support an alignment requirement bigger than the block size.
	   Drivers that need this can just make their blocksize equal to the alignment and
	   break the alignment-sized blocks into their intrinsic blocksize in their read block hook */
	assert(alignment <= dev->block_size);

	/* can't change the alignment after first setting it */
	assert(dev->alignment == 1 && dev->alignment_shift == 0);

	dev->alignment = alignment;
	dev->alignment_shift = log2_int(dev->alignment);
}

/*
 * set up the shared part of the block device structure, and set the default read hooks
 * that provide a lot of the base functionality. The specific block device implementation should
 * customize the structure further
 */
int construct_blockdev(struct blockdev *dev, const char *name, uint64_t len, uint32_t block_size)
{
	assert(dev != NULL);
	//assert(is_pow2(block_size));
	assert(strlen(name) < sizeof(dev->name));

	strcpy_s(dev->name, 16, name);
	dev->flags = 0;
	dev->block_size = block_size;
	dev->block_shift = log2_int(dev->block_size);
	dev->block_count = len >> dev->block_shift;
	dev->total_len = len;
	dev->alignment = 1;
	dev->alignment_shift = 0;

	assert((len >> dev->block_shift) < UINT_MAX);

	dev->read_hook = &blockdev_read_hook;
	dev->read_block_hook = &blockdev_read_block_hook;
	dev->write_hook = &blockdev_write_hook;
	dev->write_block_hook = &blockdev_write_block_hook;
	dev->erase_hook = &blockdev_erase_hook;

	dev->protect_start = 0;
	dev->protect_end = 0;

	return 0;
}