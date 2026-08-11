#include "focus.h"

const char *const termgfx_focus_enable  = "\x1b[?1004h";
const char *const termgfx_focus_restore = "\x1b[?1004l";

void termgfx_focus_init(termgfx_focus_t *f)
{
	if (f == NULL)
		return;
	f->blurred  = 0;
	f->reported = 0;
	f->lost     = 0;
}

int termgfx_focus_on_csi(termgfx_focus_t *f, const char *par, size_t parlen,
                         char final)
{
	if (f == NULL || parlen != 0)
		return 0;
	(void)par;
	if (final == 'I') {
		f->blurred  = 0;
		f->reported = 1;
		return 1;
	}
	if (final == 'O') {
		f->blurred  = 1;
		f->reported = 1;
		f->lost     = 1;
		return 1;
	}
	return 0;
}

int termgfx_focus_have(const termgfx_focus_t *f)
{
	return (f == NULL) ? 1 : !f->blurred;
}

int termgfx_focus_reported(const termgfx_focus_t *f)
{
	return (f == NULL) ? 0 : f->reported;
}

int termgfx_focus_take_lost(termgfx_focus_t *f)
{
	int was;

	if (f == NULL)
		return 0;
	was = f->lost;
	f->lost = 0;
	return was;
}
