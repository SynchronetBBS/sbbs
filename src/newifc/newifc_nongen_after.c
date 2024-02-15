#ifdef BUILD_TESTS
#include "CuTest.h"
#endif

NI_err
NI_set_focus(NewIfcObj obj, bool value) {
	NI_err ret;
	if (obj == NULL)
		return NewIfc_error_invalid_arg;
	if (NI_set_locked(obj, true) == NewIfc_error_none) {
		NewIfcObj remove_focus;
		ret = call_bool_change_handlers(obj, NewIfc_focus, value);
		if (ret != NewIfc_error_none) {
			NI_set_locked(obj, false);
			return ret;
		}
		if (value == false) {
			if (obj->root == obj) {
				NI_set_locked(obj, false);
				return NewIfc_error_invalid_arg;
			}
			remove_focus = obj->parent;
		}
		else {
			// If we already have focus, remove focus from all children...
			if (obj->focus) {
				remove_focus = obj;
			}
			// Otherwise, walk up the tree setting focus until we get to an item that does.
			// When we get there, remove focus from its children
			else {
				for (remove_focus = obj; remove_focus->focus == false && remove_focus->root != remove_focus; remove_focus = remove_focus->parent)
					;
			}
			ret = obj->set(obj, NewIfc_focus, value);
		}
		NI_walk_children(remove_focus, true, remove_focus_cb, NULL);
		if (ret == NewIfc_error_not_implemented) {
			obj->focus = value;
			ret = NewIfc_error_none;
		}
		NI_set_locked(obj, false);
	}
	else
		ret = NewIfc_error_lock_failed;
	return ret;
}

static NI_err
clean_cb(NewIfcObj obj, void *cbdata)
{
	NI_err ret;

	if (!obj->dirty)
		return NewIfc_error_skip_subtree;
	obj->set(obj, NewIfc_dirty, false);
	obj->dirty = false;

	return NewIfc_error_none;
}

NI_err
NI_set_dirty(NewIfcObj obj, bool value) {
	NI_err ret;
	if (obj == NULL)
		return NewIfc_error_invalid_arg;
	if (NI_set_locked(obj, true) == NewIfc_error_none) {
		// Setting to dirty walks up
		// Setting to clean walks down
		NewIfcObj mobj;
		if (value) {
			for (mobj = obj; mobj; mobj = mobj->parent) {
				ret = NI_set_locked(mobj, true);
				if (ret == NewIfc_error_none) {
					if (mobj->dirty) {
						NI_set_locked(mobj, false);
						break;
					}
					mobj->dirty = true;
					NI_set_locked(mobj, false);
				}
				else
					ret = NewIfc_error_lock_failed;
			}
		}
		else {
			ret = NI_walk_children(obj, true, clean_cb, NULL);
		}
		NI_set_locked(obj, false);
	}
	else
		ret = NewIfc_error_lock_failed;
	return ret;
}

#ifdef BUILD_TESTS

#include <stdio.h>
void test_api(CuTest *ct)
{
	char *s;
	NewIfcObj obj = NULL;
	NewIfcObj robj = NULL;
	static const char *new_title = "New Title";
	struct vmem_cell cells;
	uint32_t u32;
	int32_t i32;
	uint16_t u16;
	uint8_t u8;
	enum NewIfc_alignment a;

	CuAssertTrue(ct, NI_create(NewIfc_root_window, NULL, &robj) == NewIfc_error_none);
	CuAssertTrue(ct, NI_set_fill_character_color(robj, NI_CYAN) == NewIfc_error_none);
	CuAssertTrue(ct, NI_get_fill_character_color(robj, &u32) == NewIfc_error_none && u32 == NI_CYAN);
	CuAssertTrue(ct, NI_set_fill_colour(robj, NI_BLUE) == NewIfc_error_none);
	CuAssertTrue(ct, NI_get_fill_colour(robj, &u32) == NewIfc_error_none && u32 == NI_BLUE);
	CuAssertTrue(ct, NI_set_fill_character(robj, 0xb0) == NewIfc_error_none);
	CuAssertTrue(ct, NI_get_fill_character(robj, &u8) == NewIfc_error_none && u8 == 0xb0);

	CuAssertTrue(ct, NI_create(NewIfc_label, robj, &obj) == NewIfc_error_none);
	CuAssertTrue(ct, NI_set_bg_colour(obj, NI_CYAN) == NewIfc_error_none);
	CuAssertTrue(ct, NI_get_bg_colour(obj, &u32) == NewIfc_error_none && u32 == NI_CYAN);
	CuAssertTrue(ct, NI_set_fg_colour(obj, NI_WHITE) == NewIfc_error_none);
	CuAssertTrue(ct, NI_get_fg_colour(obj, &u32) == NewIfc_error_none && u32 == NI_WHITE);
	CuAssertTrue(ct, NI_set_text(obj, new_title) == NewIfc_error_none);
	CuAssertTrue(ct, NI_get_text(obj, &s) == NewIfc_error_none && strcmp(s, new_title) == 0);
	CuAssertTrue(ct, NI_set_left_pad(obj, 1) == NewIfc_error_none);
	CuAssertTrue(ct, NI_get_left_pad(obj, &u16) == NewIfc_error_none && u16 == 1);
	CuAssertTrue(ct, NI_set_right_pad(obj, 1) == NewIfc_error_none);
	CuAssertTrue(ct, NI_get_right_pad(obj, &u16) == NewIfc_error_none && u16 == 1);
	CuAssertTrue(ct, NI_set_align(obj, NewIfc_align_centre) == NewIfc_error_none);
	CuAssertTrue(ct, NI_get_align(obj, &a) == NewIfc_error_none && a == NewIfc_align_centre);
	CuAssertTrue(ct, NI_set_height(obj, 1) == NewIfc_error_none);
	CuAssertTrue(ct, NI_get_height(obj, &i32) == NewIfc_error_none && i32 == 1);

	CuAssertTrue(ct, robj->do_render(robj, NULL) == NewIfc_error_none);
	size_t sz = strlen(new_title);
	for (size_t i = 0; i < sz; i++) {
		ciolib_vmem_gettext(i+36,1,i+36,1,&cells);
		CuAssertTrue(ct, cells.ch == new_title[i]);
		CuAssertTrue(ct, cells.fg == obj->fg_colour);
		CuAssertTrue(ct, cells.bg == obj->bg_colour);
		CuAssertTrue(ct, cells.font == obj->font);
	}
	CuAssertTrue(ct, NI_destroy(robj) == NewIfc_error_none);
}

#endif
