#include <assert.h>
#include <stdint.h>
#include "idle.h"

int main(void) {
	termgfx_idle_t i;
	unsigned       left;

	/* threshold 0 disables the feature outright: however long we wait, the
	 * verdict stays ACTIVE. This is the "sysop did not configure it" path and
	 * the exempt-user path, so it must never warn. */
	termgfx_idle_init(&i, 0, 60, 1000);
	assert(termgfx_idle_poll(&i, 1000u + 3600000u, &left) == TERMGFX_IDLE_ACTIVE);

	/* 900s threshold with a 60s countdown: ACTIVE right up to 840s. */
	termgfx_idle_init(&i, 900, 60, 1000);
	assert(termgfx_idle_poll(&i, 1000u, &left) == TERMGFX_IDLE_ACTIVE);
	assert(termgfx_idle_poll(&i, 1000u + 839000u, &left) == TERMGFX_IDLE_ACTIVE);

	/* At exactly threshold-warn the countdown opens, showing the full 60s. */
	assert(termgfx_idle_poll(&i, 1000u + 840000u, &left) == TERMGFX_IDLE_WARN);
	assert(left == 60);

	/* ...and counts down as the grace runs out. */
	assert(termgfx_idle_poll(&i, 1000u + 880000u, &left) == TERMGFX_IDLE_WARN);
	assert(left == 20);

	/* At the threshold it expires, and stays expired. */
	assert(termgfx_idle_poll(&i, 1000u + 900000u, &left) == TERMGFX_IDLE_EXPIRED);
	assert(termgfx_idle_poll(&i, 1000u + 999999u, &left) == TERMGFX_IDLE_EXPIRED);

	/* A keypress during the warning clears it and restarts the whole clock --
	 * no penalty, and the next warning is a full threshold away. */
	termgfx_idle_init(&i, 900, 60, 1000);
	assert(termgfx_idle_poll(&i, 1000u + 880000u, &left) == TERMGFX_IDLE_WARN);
	termgfx_idle_activity(&i, 1000u + 880000u);
	assert(termgfx_idle_poll(&i, 1000u + 880000u, &left) == TERMGFX_IDLE_ACTIVE);
	assert(termgfx_idle_poll(&i, 1000u + 1719000u, &left) == TERMGFX_IDLE_ACTIVE);
	assert(termgfx_idle_poll(&i, 1000u + 1720000u, &left) == TERMGFX_IDLE_WARN);

	/* A misconfigured warn > timeout clamps: warn immediately, still expire at
	 * the threshold. It must not underflow into never-firing. */
	termgfx_idle_init(&i, 60, 600, 1000);
	assert(termgfx_idle_poll(&i, 1001u, &left) == TERMGFX_IDLE_WARN);
	assert(termgfx_idle_poll(&i, 1000u + 60000u, &left) == TERMGFX_IDLE_EXPIRED);

	/* The ms clock wrapping past UINT32_MAX must not read as a huge elapsed and
	 * expire a present user. Same int32_t-difference idiom the doors' existing
	 * deadline checks use. */
	termgfx_idle_init(&i, 900, 60, 0xFFFFF000u);
	assert(termgfx_idle_poll(&i, (uint32_t)(0xFFFFF000u + 1000u), &left) == TERMGFX_IDLE_ACTIVE);
	assert(termgfx_idle_poll(&i, (uint32_t)(0xFFFFF000u + 840000u), &left) == TERMGFX_IDLE_WARN);
	assert(termgfx_idle_poll(&i, (uint32_t)(0xFFFFF000u + 900000u), &left) == TERMGFX_IDLE_EXPIRED);

	return 0;
}
