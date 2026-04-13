#ifndef RIP_ICONS_H
#define RIP_ICONS_H

#include <stdint.h>
#include <stddef.h>

struct builtin_icon {
	const char     *filename;
	uint16_t        width;
	uint16_t        height;
	const uint8_t  *data;
};

extern const struct builtin_icon builtin_icons[];
extern const size_t builtin_icon_count;

#endif
