#include "scale.h"
#include "xbr.h"

static void pointy_scale3(uint32_t* src, uint32_t* dest, int width, int height);
static void pointy_scale5(uint32_t* src, uint32_t* dest, int width, int height);

uint32_t*
do_scale(struct rectlist* rect, uint32_t **target, size_t *targetsz, int* xscale, int* yscale, int* w, int* h)
{
	int pointy5 = 0;
	int pointy3 = 0;
	int xbr2 = 0;
	int xbr4 = 0;
	int newscale = 1;
	int total_scaling = 1;
	static uint32_t *tmptarget = NULL;
	static size_t tmptargetsz = 0;
	uint32_t *ctarget;
	uint32_t *csrc = rect->data;
	uint32_t* nt;
	int swidth = rect->rect.width;
	int sheight = rect->rect.height;

	*w = rect->rect.width;
	*h = rect->rect.height;
	// As a first pass, only do equal integer scaling
	if (*xscale != *yscale)
		return rect->data;
	switch (*xscale) {
		case 1:
			return rect->data;
		case 2:
			xbr2 = 1;
			break;
		case 3:
			pointy3 = 1;
			break;
		case 4:
			xbr4 = 1;
			break;
		case 5:
			pointy5 = 1;
			break;
		case 6:
			pointy3 = 1;
			xbr2 = 1;
			break;
		default:
			total_scaling = *xscale;
			*xscale = 1;
			while (total_scaling > 1 && ((total_scaling % 5) == 0)) {
				pointy5++;
				total_scaling /= 5;
				*xscale *= 5;
			}
			while (total_scaling > 1 && ((total_scaling % 3) == 0)) {
				pointy3++;
				total_scaling /= 3;
				*xscale *= 3;
			}
			while (total_scaling > 1 && ((total_scaling % 4) == 0)) {
				xbr4++;
				total_scaling /= 4;
				*xscale *= 4;
			}
			while (total_scaling > 1 && ((total_scaling % 2) == 0)) {
				xbr2++;
				total_scaling /= 2;
				*xscale *= 2;
			}
			if (*xscale == 1)
				return rect->data;
			break;
	}

	// Now make sure target is big enough...
	size_t needsz = rect->rect.width * rect->rect.height * (*xscale) * (*xscale) * sizeof(uint32_t);
	if (needsz > *targetsz) {
		nt = realloc(*target, needsz);
		if (nt == NULL)
			return rect->data;
		*target = nt;
		*targetsz = needsz;
	}
	ctarget = *target;

	// And if we need an extra target, do the same there...
	if (pointy3 + pointy5 + xbr4 + xbr2 > 1) {
		if (needsz > tmptargetsz) {
			nt = realloc(tmptarget, needsz);
			if (nt == NULL)
				return rect->data;
			tmptarget = nt;
			tmptargetsz = needsz;
		}
	}

	// And finally, scale...
	while (xbr4 > 0) {
		xbr_filter(csrc, ctarget, swidth, sheight, 4);
		xbr4--;
		swidth *= 4;
		sheight *= 4;
		csrc = ctarget;
		if (ctarget == tmptarget)
			ctarget = *target;
		else
			ctarget = tmptarget;
	}
	while (xbr2 > 0) {
		xbr_filter(csrc, ctarget, swidth, sheight, 2);
		xbr2--;
		swidth *= 2;
		sheight *= 2;
		csrc = ctarget;
		if (ctarget == tmptarget)
			ctarget = *target;
		else
			ctarget = tmptarget;
	}
	while (pointy5 > 0) {
		pointy_scale5(csrc, ctarget, swidth, sheight);
		pointy5--;
		swidth *= 5;
		sheight *= 5;
		csrc = ctarget;
		if (ctarget == tmptarget)
			ctarget = *target;
		else
			ctarget = tmptarget;
	}
	while (pointy3 > 0) {
		pointy_scale3(csrc, ctarget, swidth, sheight);
		pointy3--;
		swidth *= 3;
		sheight *= 3;
		csrc = ctarget;
		if (ctarget == tmptarget)
			ctarget = *target;
		else
			ctarget = tmptarget;
	}
	*w *= *xscale;
	*h *= *xscale;
	*xscale = newscale;
	*yscale = newscale;
	return csrc;
}

static void
pointy_scale5(uint32_t* src, uint32_t* dest, int width, int height)
{
	int x, y;
	uint32_t* s;
	uint32_t* d;
	int w5 = width * 5;
	int w10 = width * 10;
	int w15 = width * 15;
	int w20 = width * 20;
	int prevline, prevcol, nextline, nextcol;

	s = src;
	d = dest;
	prevline = 0;
	nextline = width;
	for (y = 0; y < height; y++) {
		if (y == height - 1)
			nextline = 0;
		prevcol = 0;
		nextcol = 1;
		for (x = 0; x < width; x++) {
			if (x == width - 1)
				nextcol = 0;

			if (s[prevline + prevcol] == s[0]) {
				d[0] = s[0];
				d[1] = s[0];
				d[w5] = s[0];
			}
			else if (s[prevcol] == s[prevline]) {
				d[0] = s[prevcol];
				d[1] = s[prevcol];
				d[w5] = s[prevcol];
			}
			else {
				d[0] = *s;
				d[1] = *s;
				d[w5] = *s;
			}

			// Top-middle stays OG.
			d[2] = *s;
			d[w5+1] = *s;
			d[w5+2] = *s;
			d[w5+3] = *s;

			// And so on around the outside (round the outside)
			if (s[prevline + nextcol] == s[0]) {
				d[3] = s[0];
				d[4] = s[0];
				d[w5 + 4] = s[0];
			}
			else if (s[nextcol] == s[prevline]) {
				d[3] = s[nextcol];
				d[4] = s[nextcol];
				d[w5 + 4] = s[nextcol];
			}
			else {
				d[3] = s[0];
				d[4] = s[0];
				d[w5 + 4] = s[0];
			}

			d[w10] = *s;
			d[w10+1] = *s;
			d[w10+2] = *s;
			d[w10+3] = *s;
			d[w10+4] = *s;

			if (s[prevcol + nextline] == s[0]) {
				d[w15] = s[0];
				d[w20] = s[0];
				d[w20 + 1] = s[0];
			}
			else if(s[prevcol] == s[nextline]) {
				d[w15] = s[prevcol];
				d[w20] = s[prevcol];
				d[w20 + 1] = s[prevcol];
			}
			else {
				d[w15] = s[0];
				d[w20] = s[0];
				d[w20 + 1] = s[0];
			}

			d[w15 + 1] = *s;
			d[w15 + 2] = *s;
			d[w15 + 3] = *s;
			d[w20 + 2] = *s;

			if (s[nextcol + nextline] == s[0]) {
				d[w15 + 4] = s[0];
				d[w20 + 3] = s[0];
				d[w20 + 4] = s[0];
			}
			else if (s[nextcol] == s[nextline]) {
				d[w15 + 4] = s[nextcol];
				d[w20 + 3] = s[nextcol];
				d[w20 + 4] = s[nextcol];
			}
			else {
				d[w15 + 4] = s[0];
				d[w20 + 3] = s[0];
				d[w20 + 4] = s[0];
			}

			d += 5;
			s++;

			if (x == 0)
				prevcol = -1;
		}
		d += w20;
		if (y == 0)
			prevline = -width;
	}
}

static void
pointy_scale3(uint32_t* src, uint32_t* dest, int width, int height)
{
	int x, y;
	uint32_t* s;
	uint32_t* d;
	int w3 = width * 3;
	int w6 = width * 6;
	int prevline, prevcol, nextline, nextcol;

	s = src;
	d = dest;
	prevline = 0;
	nextline = width;
	for (y = 0; y < height; y++) {
		if (y == height - 1)
			nextline = 0;
		prevcol = 0;
		nextcol = 1;
		for (x = 0; x < width; x++) {
			if (x == width - 1)
				nextcol = 0;

			// Top-left is filled if both left and top are the same.
			if (s[prevline + prevcol] == s[0])
				d[0] = s[0];
			else if (s[prevcol] == s[prevline])
				d[0] = s[prevcol];
			else
				d[0] = *s;

			// Top-middle stays OG.
			d[1] = *s;

			// And so on around the outside (round the outside)
			if (s[prevline + nextcol] == s[0])
				d[2] = s[0];
			else if (s[nextcol] == s[prevline])
				d[2] = s[nextcol];
			else
				d[2] = *s;

			d[w3] = *s;
			d[w3 + 1] = *s;
			d[w3 + 2] = *s;

			if (s[prevcol + nextline] == s[0])
				d[w6] = s[0];
			else if(s[prevcol] == s[nextline])
				d[w6] = s[prevcol];
			else
				d[w6] = *s;

			d[w6 + 1] = *s;

			if (s[nextcol + nextline] == s[0])
				d[w6 + 2] = s[0];
			else if (s[nextcol] == s[nextline])
				d[w6 + 2] = s[nextcol];
			else
				d[w6 + 2] = s[0];

			d += 3;
			s++;

			if (x == 0)
				prevcol = -1;
		}
		d += w6;
		if (y == 0)
			prevline = -width;
	}
}
