#ifndef AMIGAFONT_H
#define AMIGAFONT_H

#include <stdint.h>

enum fonttype {
	FONTTYPE_STANDARD = 0x00ff,
	FONTTYPE_TAGGED = 0x020f
};

struct StandardFontEntry {
	char		name[256];
	uint16_t	height;
	uint8_t		style;
	uint8_t		flags;
};

struct TaggedFontEntry {
	char		name[254];
	uint16_t	tags;
	uint16_t	height;
	uint8_t		style;
	uint8_t		flags;
};

struct FontContentsHeader {
	uint16_t	type;
	uint16_t	count;
};

struct FontList {
	struct FontContentsHeader header;
	union {
		struct StandardFontEntry std;
		struct StandardFontEntry tag;
	} entry[];
};

struct FontHeader {
	uint8_t		ignore[0x6E];
	uint16_t	height;
	uint8_t		style;
	uint8_t		flags;
	uint16_t	xsize;
	uint16_t	baseline;	// Distance from top to baseline
	uint16_t	boldsmear;
	uint16_t	rc;		// Literally a reference count on disk.
	uint8_t		first;
	uint8_t		last;
	uint32_t	dataOffset;
	uint16_t	modulo;
	uint32_t	charlocOffset;
	uint32_t	fontSpaceOffset;
	uint32_t	kernOffset;
} __attribute__((packed));	// TODO: Deal with Microsoft.

#endif
