/*
 * Copyright (C) 2006 Apple Computer, Inc. All rights reserved.
 *
 * This document is the property of Apple Computer, Inc.
 * It is considered confidential and proprietary.
 *
 * This document may not be reproduced or transmitted in any form,
 * in whole or in part, without the express written permission of
 * Apple Computer, Inc.
 */
 /*
  * Read-only support for iPod-style SysCfg data.
  */

#ifndef __SYSCFG_H
#define __SYSCFG_H

#include "types.h"

__BEGIN_DECLS

/* where we expect to find the magic number in a bdev */
#define kSysCfgBdevOffset  0x4000

struct cmd_arg {
	int n;
	size_t u;
	size_t h;
	bool b;
	char *str;
};

struct syscfgEntry {
	u_int32_t	seTag;
	//u_int8_t	seData[16];
	union {
		u_int8_t seData[16];
		struct {
			uint32_t type;
			uint32_t size;
			uint32_t offset;
		} cntb;
	};
};

struct syscfgEntryCNTB {
	u_int32_t       seTag;	/* 'CTNB' */
	u_int32_t	seRealTag;
	u_int32_t	seDataSize;
	u_int32_t	seDataOffset;
	u_int32_t	reserved;
};

struct syscfgMemEntry {
	u_int32_t	seTag;
	u_int8_t	seData[16];
	u_int32_t	seDataSize;
	u_int32_t	seDataOffset;
};

typedef struct OIBSyscfgEntry
{
	int type;
	uint32_t size;
	uint8_t* data;
} OIBSyscfgEntry;


void syscfg_init(uint8_t *sys, size_t len);

int
do_syscfg(int argc, struct cmd_arg *args);
/*
 * Attempt to initialise SysCfg from (bdevName)
 *
 * Returns true if successful.
 */
bool	syscfgInitWithBdev(const char *bdevName);

/*
 * Copy the data for an entry by tag
 *
 * Look up the provided tag, then copy the upto the size
 * bytes into buffer.
 *
 * Returns -1 if the tag was not found, or the number of bytes copied.
 */
int syscfgCopyDataForTag(u_int32_t tag, u_int8_t *buffer, size_t size);

/*
 * Look up an entry by tag.
 *
 * Returns false if the tag was not found. If the tag was found, returns true
 * and copies the tag's entry into the provided pointer..
 */
bool	syscfgFindByTag(u_int32_t tag, struct syscfgMemEntry *entry);

/*
 * Look up an entry by index.
 *
 * Returns a pointer to a statically-allocated internal buffer, which
 * will remain valid until the next call to syscfgFindByTag or syscfgFindByIndex.
 *
 * Returns NULL if the index was not found or not valid.
 * Returns false if the index  was not found or not valid. If the index was found,
 * returns true and copies the tag's entry into the provided pointer..
 */
bool	syscfgFindByIndex(u_int32_t index, struct syscfgMemEntry *result);

/*
 * Gets a pointer to the data associated with an entry returned by
 * syscfgFindByTag or syscfgFindByIndex
 *
 * Returns NULL if the entry was invalid
 */
void *	syscfgGetData(struct syscfgMemEntry *entry);

/*
 * Gets the size in bytes of the data associated with an entry returned by
 * syscfgFindByTag or syscfgFindByIndex
 */
uint32_t	syscfgGetSize(struct syscfgMemEntry *entry);

/* Returns true if the tag is found and populates the given pointers if they are non-NULL */
bool		syscfg_find_tag(uint32_t tag, void **data_out, uint32_t *size_out);

__END_DECLS

#endif