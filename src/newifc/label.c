#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>  // malloc()/free()

#define CIOLIB_NO_MACROS
#include "ciolib.h"
#include "genwrap.h"
#include "strwrap.h"
#include "threadwrap.h"

#include "newifc.h"
#include "newifc_internal.h"

#ifdef BUILD_TESTS
#include "CuTest.h"
#endif

struct label {
	struct newifc_api api;
	char *text;
};

static NI_err
label_set(NewIfcObj obj, int attr, ...)
{
	struct label *l = (struct label *)obj;
	NI_err ret = NewIfc_error_none;
	va_list ap;
	char *buf;

	va_start(ap, attr);
	switch (attr) {
		case NewIfc_text:
			buf = strdup(va_arg(ap, const char *));
			if (buf == NULL) {
				ret = NewIfc_error_allocation_failure;
				break;
			}
			free(l->text);
			l->text = buf;
			break;
		default:
			ret = NewIfc_error_not_implemented;
			break;
	}
	va_end(ap);

	return ret;
}

static NI_err
label_get(NewIfcObj obj, int attr, ...)
{
	struct label *l = (struct label *)obj;
	NI_err ret = NewIfc_error_none;
	va_list ap;

	va_start(ap, attr);
	switch (attr) {
		case NewIfc_text:
			*(va_arg(ap, char **)) = l->text;
			break;
		default:
			ret = NewIfc_error_not_implemented;
			break;
	}

	return ret;
}

static NI_err
label_copy(NewIfcObj old, NewIfcObj *newobj)
{
	struct label **newl = (struct label **)newobj;
	struct label *oldl = (struct label *)old;

	*newl = malloc(sizeof(struct label));
	if (*newl == NULL)
		return NewIfc_error_allocation_failure;
	memcpy(*newl, oldl, sizeof(struct label));
	assert(oldl->text);
	(*newl)->text = strdup(oldl->text);
	if ((*newl)->text == NULL) {
		free(*newl);
		return NewIfc_error_allocation_failure;
	}

	return NewIfc_error_none;
}

static NI_err
label_do_render(NewIfcObj obj, struct NewIfc_render_context *ctx)
{
	size_t sz = 0;
	struct label *l = (struct label *) obj;
	if (l->text)
		sz = strlen(l->text);
	// TODO: Ensure text fits...
	size_t c = ctx->ypos * ctx->dwidth + ctx->xpos;
	for (size_t i = 0; i < sz; i++, c++) {
		if (obj->bg_colour != NI_TRANSPARENT)
			ctx->vmem[c].bg = obj->bg_colour;
		if (obj->fg_colour != NI_TRANSPARENT) {
			ctx->vmem[c].fg = obj->fg_colour;
			ctx->vmem[c].font = obj->font;
			ctx->vmem[c].ch = l->text[i];
		}
	}
	return NewIfc_error_none;
}

static NI_err
NewIFC_label(NewIfcObj parent, NewIfcObj *newobj)
{
	struct label **newl = (struct label **)newobj;

	if (parent == NULL)
		return NewIfc_error_invalid_arg;
	*newl = calloc(1, sizeof(struct label));

	if (*newl == NULL)
		return NewIfc_error_allocation_failure;
	(*newl)->text = strdup("");
	if ((*newl)->text == NULL) {
		free(*newl);
		return NewIfc_error_allocation_failure;
	}
	NI_copy_globals(&((*newl)->api), parent);
	(*newl)->api.get = &label_get;
	(*newl)->api.set = &label_set;
	(*newl)->api.copy = &label_copy;
	(*newl)->api.do_render = &label_do_render;
	(*newl)->api.child_ypos = 0;
	(*newl)->api.child_xpos = 0;
	(*newl)->api.child_width = 0;
	(*newl)->api.child_height = 0;
	// TODO: This is only needed by the unit tests...
	(*newl)->api.root = parent->root;
	parent->bottomchild = parent->topchild = (NewIfcObj)*newl;
	(*newl)->api.root = parent->root;
	(*newl)->api.parent = parent;
	(*newl)->api.width = parent->child_width;
	(*newl)->api.height = 1;
	(*newl)->api.min_width = 0;
	(*newl)->api.min_height = 1;
	return NewIfc_error_none;
}

#ifdef BUILD_TESTS

void test_label(CuTest *ct)
{
	char *s;
	NewIfcObj obj;
	NewIfcObj robj;
	static const char *new_title = "New Title";
	struct vmem_cell cells;

	CuAssertTrue(ct, NewIFC_root_window(NULL, &robj) == NewIfc_error_none);
	CuAssertPtrNotNull(ct, robj);
	CuAssertTrue(ct, NewIFC_label(robj, &obj) == NewIfc_error_none);
	CuAssertPtrNotNull(ct, obj);
	CuAssertPtrNotNull(ct, obj->get);
	CuAssertPtrNotNull(ct, obj->set);
	CuAssertPtrNotNull(ct, obj->copy);
	CuAssertPtrNotNull(ct, obj->do_render);
	CuAssertTrue(ct, obj->width == 80);
	CuAssertTrue(ct, obj->height == 1);
	CuAssertTrue(ct, obj->min_width == 0);
	CuAssertTrue(ct, obj->min_height == 1);
	CuAssertTrue(ct, obj->focus == true);
	CuAssertTrue(ct, obj->get(obj, NewIfc_text, &s) == NewIfc_error_none && strcmp(s, "") == 0);

	CuAssertTrue(ct, obj->set(obj, NewIfc_text, new_title) == NewIfc_error_none);

	CuAssertTrue(ct, obj->get(obj, NewIfc_text, &s) == NewIfc_error_none && strcmp(s, new_title) == 0);

	CuAssertTrue(ct, robj->do_render(robj, NULL) == NewIfc_error_none);
	size_t sz = strlen(new_title);
	for (size_t i = 0; i < sz; i++) {
		ciolib_vmem_gettext(i+1,1,i+1,1,&cells);
		CuAssertTrue(ct, cells.ch == new_title[i]);
		CuAssertTrue(ct, cells.fg == obj->fg_colour);
		CuAssertTrue(ct, cells.bg == obj->bg_colour);
		CuAssertTrue(ct, cells.font == obj->font);
	}
}

#endif
