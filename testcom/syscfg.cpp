#include "pch.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "blockdev.h"
#include "syscfg.h"

static u_int32_t	syscfgKeyCount;
static uint8_t		*syscfgData = NULL;
static size_t		syscfgDataLength=0;


#define SCFG_MAGIC 0x53436667
#define CNTB_MAGIC 0x434e5442
#define SCFG_LOCATION 0x4000

struct syscfgHeader {
	u_int32_t	shMagic;
#define kSysCfgHeaderMagic	'SCfg'
	u_int32_t	shSize;
	u_int32_t	shMaxSize;
	u_int32_t	shVersion;
	u_int32_t	shBigEndian;
	u_int32_t	shKeyCount;
};



void syscfg_init(uint8_t *sys, size_t len)
{
	syscfgData = sys;
	syscfgDataLength = len;
	syscfgKeyCount = 62;
}

void syscfg_reinit(void)
{
	free(syscfgData);
	syscfgDataLength = 0;
	syscfgData = NULL;
	syscfgKeyCount = 0;
}

int
do_syscfg(int argc, struct cmd_arg *args)
{
	struct syscfgHeader	hdr;
	struct syscfgMemEntry	entry;
	int			index, curArg;
	size_t			size, i, j;
	char			*sfx;
	bool			isExt;
	size_t			lowExt;
	size_t			highExt;
	size_t			extSize = 0;
	uint8_t			*buffer;
	bool			showExt = false;

	if (NULL == syscfgData) {
		printf("syscfg is not initialized!\n");
		return(0);
	}

	if (syscfgDataLength < sizeof(struct syscfgHeader))
		return (0);

	memcpy(&hdr, syscfgData, sizeof(struct syscfgHeader));
	printf("version 0x%08x with %d entries\n\n", hdr.shVersion, hdr.shKeyCount);

	syscfgKeyCount = hdr.shKeyCount;

	lowExt = hdr.shMaxSize;
	highExt = hdr.shSize;

	for (index = 0; ; index++) {
		if (!syscfgFindByIndex(index, &entry))
			break;

		if (argc > 1) {

			/* When parameters are present, always show in extended mode */
			showExt = true;

			/* If the first parameter is ext, assume match in all cases by skipping the match block */
			if (strcmp("ext", args[1].str) != 0) {

				/* Scan the list of arguments looking for a match */
				for (curArg = 1; curArg < argc; curArg++) {
					if (((entry.seTag >> 24) & 0xFF) == args[curArg].str[0] &&
						((entry.seTag >> 16) & 0xFF) == args[curArg].str[1] &&
						((entry.seTag >> 8) & 0xFF) == args[curArg].str[2] &&
						((entry.seTag >> 0) & 0xFF) == args[curArg].str[3])
						break;
				}
				if (curArg == argc) continue;
			}
		}

		sfx = (char *)"";

		/* Is this a standard entry or an Offset entry? */
		if (entry.seDataOffset != 0) {
			size = entry.seDataSize;
			if (size > sizeof(entry.seData)) {

				/* The Size of the entry is too big for the structure
				 * Should it be truncated? */
				if (!showExt) {
					size = sizeof(entry.seData);
					sfx = (char *)"...";
				}
			}
			if (entry.seDataOffset + entry.seDataSize > hdr.shMaxSize) {
				printf("entry->seDataOffset not within syscfgData");
				return(0);
			}

			/* Allocate the requested space aligned on 16 bytes to zero
			 * pad the display. */
			buffer = (uint8_t *)calloc(1, (size + 15) & ~15);
			memcpy(buffer, syscfgData + entry.seDataOffset, size);

			// Calculate the extents of the offset section
			if (entry.seDataOffset < lowExt) {
				lowExt = entry.seDataOffset;
			}
			if ((entry.seDataOffset + entry.seDataSize) > highExt) {
				highExt = entry.seDataOffset + entry.seDataSize;
			}
			isExt = true;
		}
		else {
			/* allocate enough space to mirror the entrys buffer */
			size = sizeof(entry.seData);

			buffer = (uint8_t *)malloc(sizeof(entry.seData));
			memcpy(buffer, entry.seData, size);

			isExt = false;
		}
		printf("0x%08x '%c%c%c%c'  %4dB %c  %s  ",
			entry.seTag,
			(entry.seTag >> 24) & 0xff,
			(entry.seTag >> 16) & 0xff,
			(entry.seTag >> 8) & 0xff,
			entry.seTag & 0xff,
			entry.seDataSize,
			isExt ? '*' : ' ',
			entry.seData);

		/* Format the data in nice columns */
		for (i = 0; i < size; i += 16) {
			printf("%*s", (i >= 16) ? 28 : 0, "");

			/* Starting at the offset, display the data values up to 4 per line. */
			for (j = i; j < (i + 16); j += 4) {
				printf("0x%08x ", *((u_int32_t *)&buffer[j]));
			}

			printf("%s\n", sfx);
		}

		free(buffer);
	}

	printf("\n");
	printf("header @ 0-%zu", sizeof(struct syscfgHeader) - 1);
	printf("; entries @ %zu-%d", sizeof(struct syscfgHeader), hdr.shSize - 1);
	if (lowExt < highExt) {
		extSize = highExt - lowExt;
		printf("; extra data @ %zu-%zu", lowExt, highExt - 1);
	}
	printf("; using %zu of %d bytes\n", hdr.shSize + extSize, hdr.shMaxSize);

	return(0);
}

bool
syscfgInitWithBdev(const char *bdevName)
{
	int result;
	struct blockdev	*candidate;
	struct syscfgHeader hdr;

	if (syscfgData != NULL)
		return(false);

	/* look for the suggested bdev */
	if ((candidate = lookup_blockdev(bdevName)) == NULL) {
		printf("syscfg: can't find bdev \"%s\"\n", bdevName);
		return(false);
	}

	/* Make sure it's big enough */
	if (candidate->total_len <= kSysCfgBdevOffset + sizeof(hdr)) {
		printf("syscfg: bdev size 0x%llx too small\n", candidate->total_len);
		return (false);
	}

	/* look for the header */
	if (blockdev_read(candidate, &hdr, kSysCfgBdevOffset, sizeof(hdr)) < (int)sizeof(hdr)) {
		printf("syscfg: bdev read fail\n");
		return(false);
	}

	if (hdr.shMagic != kSysCfgHeaderMagic) {
		printf("syscfg: bad magic\n");
		return(false);
	}

	printf("syscfg: version 0x%08x with %d entries using %d of %d bytes\n",
		hdr.shVersion, hdr.shKeyCount, hdr.shSize, hdr.shMaxSize);

	/* If there is a syscfg there will also be diag info... protect them. */
	blockdev_set_protection(candidate, 0x2000, 0x6000);

	syscfgDataLength = candidate->total_len - kSysCfgBdevOffset;
	if (hdr.shSize < syscfgDataLength)
		syscfgDataLength = hdr.shMaxSize;

	syscfgData = (uint8_t		*)malloc(syscfgDataLength);

	result = blockdev_read(candidate, syscfgData, kSysCfgBdevOffset, syscfgDataLength);

	if (result < 0 || (size_t)result < syscfgDataLength) {
		printf("syscfg: bdev read fail (%d)\n", result);
		syscfg_reinit();
		return false;
	}

	syscfgKeyCount = hdr.shKeyCount;

	return(true);
}

void *
syscfgGetData(struct syscfgMemEntry *entry)
{
	/* Handle data stored externally from the entry */
	if (entry->seDataOffset != 0) {
		if (entry->seDataOffset > syscfgDataLength)
			return NULL;
		if (entry->seDataOffset + entry->seDataSize > syscfgDataLength)
			return NULL;

		return syscfgData + entry->seDataOffset;
	}
	else {
		return entry->seData;
	}
}

uint32_t
syscfgGetSize(struct syscfgMemEntry *entry)
{
	return entry->seDataSize;
}

int
syscfgCopyDataForTag(u_int32_t tag, u_int8_t *buffer, size_t size)
{
	bool			result;
	struct syscfgMemEntry	entry;
	void			*data;

	result = syscfgFindByTag(tag, &entry);
	if (!result)
		return(-1);
	if (size > entry.seDataSize)
		size = entry.seDataSize;

	data = syscfgGetData(&entry);
	if (data == NULL)
		return(-1);

	memcpy(buffer, data, size);

	// XXX check what to do with the assert that was removed from here
	return size;
}

bool
syscfgFindByTag(u_int32_t tag, struct syscfgMemEntry *entry)
{
	static struct syscfgMemEntry	temp;
	bool				result;
	u_int32_t			index;

	for (index = 0; index < syscfgKeyCount; index++) {
		result = syscfgFindByIndex(index, &temp);
		if (result && (temp.seTag == tag)) {
			memcpy(entry, &temp, sizeof(*entry));
			return true;
		}
	}
	return false;
}

bool
syscfgFindByIndex(u_int32_t index, struct syscfgMemEntry *result)
{
	struct syscfgEntry	entry;
	struct syscfgEntryCNTB	*entryCNTB;

	if (index >= syscfgKeyCount)
		return false;

	size_t offset = sizeof(struct syscfgHeader) + index * sizeof(struct syscfgEntry);
	if (offset >= syscfgDataLength || offset + sizeof(struct syscfgEntry) >= syscfgDataLength)
		return false;

	memcpy(&entry, syscfgData + offset, sizeof(entry));

	if (entry.seTag == 'CNTB') {
		entryCNTB = (struct syscfgEntryCNTB *)&entry;
		result->seTag = entryCNTB->seRealTag;
		memset(result->seData, 0, sizeof(result->seData));
		result->seDataSize = entryCNTB->seDataSize;
		result->seDataOffset = entryCNTB->seDataOffset;
	}
	else {
		result->seTag = entry.seTag;
		memcpy(result->seData, entry.seData, sizeof(result->seData));
		result->seDataSize = sizeof(result->seData);
		result->seDataOffset = 0;
	}
	return true;
}

bool
syscfg_find_tag(uint32_t tag, void **data_out, uint32_t *size_out)
{
	static struct syscfgMemEntry result;

	if (syscfgFindByTag(tag, &result)) {
		if (data_out) {
			*data_out = syscfgGetData(&result);
		}
		if (size_out) {
			*size_out = syscfgGetSize(&result);
		}

		return true;
	}
	else {
		return false;
	}
}