#include <assert.h>
#include <stdlib.h>

#include "newifc.h"

static NI_err
NI_walk_children_recurse(NewIfcObj obj, bool top_down, NI_err (*cb)(NewIfcObj obj, void *cb_data), void *cbdata)
{
	NI_err err;
	NewIfcObj nobj;

	if (!obj)
		return NewIfc_error_none;
	err = NI_set_locked(obj, true);
	if (err != NewIfc_error_none)
		return err;
	err = cb(obj, cbdata);
	NI_set_locked(obj, false);
	if (err != NewIfc_error_none)
		return err;
	if (top_down)
		nobj = obj->top_child;
	else
		nobj = obj->bottom_child;

	if (nobj != NULL) {
		err = NI_walk_children_recurse(nobj, top_down, cb, cbdata);
		if (err != NewIfc_error_none && err != NewIfc_error_skip_subtree)
			return err;
	}
	if (top_down)
		nobj = obj->lower_peer;
	else
		nobj = obj->higher_peer;

	if (!nobj)
		return NewIfc_error_none;
	err = NI_walk_children_recurse(nobj, top_down, cb, cbdata);
	if (err != NewIfc_error_none && err != NewIfc_error_skip_subtree)
		return err;
	return NewIfc_error_none;
}

NI_err
NI_walk_children(NewIfcObj obj, bool top_down, NI_err (*cb)(NewIfcObj obj, void *cb_data), void *cbdata)
{
	NI_err ret;

	if (obj == NULL)
		return NewIfc_error_invalid_arg;
	if (cb == NULL)
		return NewIfc_error_invalid_arg;
	ret = NI_walk_children_recurse(top_down ? obj->top_child : obj->bottom_child, top_down, cb, cbdata);
	return ret;
}

static int
handler_bsearch_compar(const void *a, const void *b)
{
	const enum NewIfc_event_handlers *key = a;
	const struct NewIfc_handler **tst = (const struct NewIfc_handler **)b;

	if (*key < (*tst)->event)
		return -1;
	if (*key > (*tst)->event)
		return 1;
	return 0;
}

static int
handler_qsort_compar(const void *a, const void *b)
{
	const struct NewIfc_handler **ap = (const struct NewIfc_handler **)a;
	const struct NewIfc_handler **bp = (const struct NewIfc_handler **)b;

	return ((*ap)->event - (*bp)->event);
}

static NI_err
NI_install_handler(NewIfcObj obj, struct NewIfc_handler *handler)
{
	struct NewIfc_handler **head = bsearch(&handler->event, obj->handlers,
	    obj->handlers_sz, sizeof(obj->handlers[0]), handler_bsearch_compar);

	if (head != NULL) {
		handler->next = *head;
		*head = handler;
		return NewIfc_error_none;
	}
	// Increase the size of handlers...
	// TODO: this is pretty inefficient, a bsearch() for insert point
	// would be best, but even a linear search for it would likely be
	// better.
	size_t new_sz = obj->handlers_sz + 1;
	struct NewIfc_handler **na = realloc(obj->handlers, new_sz * sizeof(obj->handlers[0]));
	if (na == NULL)
		return NewIfc_error_allocation_failure;
	obj->handlers = na;
	obj->handlers[obj->handlers_sz] = handler;
	obj->handlers_sz = new_sz;
	qsort(obj->handlers, obj->handlers_sz, sizeof(obj->handlers[0]), handler_qsort_compar);
	return NewIfc_error_none;
}

static NI_err
remove_focus_cb(NewIfcObj obj, void *cbdata)
{
	(void)cbdata;
	if (obj->focus == false)
		return NewIfc_error_skip_subtree;
	obj->set(obj, NewIfc_focus, false);
	obj->focus = false;
	return NewIfc_error_none;
}

static NI_err
NI_resize(NewIfcObj obj, uint16_t width, uint16_t height)
{
	/* First, make sure all children will fit in new size...
	 * walk through them and check that the minsize is smaller than
	 * or equal to the updated client size accounting for X/Y position */

	/* Now walk through them again actually setting the size...
	 * This is where sizers will need to get beefy */

	return NewIfc_error_none;
}

NI_err
NI_inner_width(NewIfcObj obj, uint16_t *width)
{
	if (width == NULL)
		return NewIfc_error_invalid_arg;
	if (obj == NULL) {
		*width = 0;
		return NewIfc_error_invalid_arg;
	}
	int32_t both = (int32_t)obj->left_pad + obj->right_pad;
	if (both > obj->width) {
		*width = 0;
		return NewIfc_error_wont_fit;
	}
	*width = obj->width - obj->left_pad - obj->right_pad;
	return NewIfc_error_none;
}

NI_err
NI_inner_height(NewIfcObj obj, uint16_t *height)
{
	if (height == NULL)
		return NewIfc_error_invalid_arg;
	if (obj == NULL) {
		*height = 0;
		return NewIfc_error_invalid_arg;
	}
	int32_t both = (int32_t)obj->top_pad + obj->bottom_pad;
	if (both > obj->height) {
		*height = 0;
		return NewIfc_error_wont_fit;
	}
	*height = obj->height - obj->top_pad - obj->bottom_pad;
	return NewIfc_error_none;
}

NI_err
NI_inner_xpos(NewIfcObj obj, uint16_t *xpos)
{
	if (xpos == NULL)
		return NewIfc_error_invalid_arg;
	if (obj == NULL) {
		*xpos = 0;
		return NewIfc_error_invalid_arg;
	}
	*xpos = obj->xpos + obj->left_pad;
	return NewIfc_error_none;
}

NI_err
NI_inner_ypos(NewIfcObj obj, uint16_t *ypos)
{
	if (ypos == NULL)
		return NewIfc_error_invalid_arg;
	if (obj == NULL) {
		*ypos = 0;
		return NewIfc_error_invalid_arg;
	}
	*ypos = obj->ypos + obj->top_pad;
	return NewIfc_error_none;
}

NI_err
NI_inner_size(NewIfcObj obj, uint16_t *width, uint16_t *height)
{
	NI_err ret;
	ret = NI_inner_width(obj, width);
	if (ret != NewIfc_error_none)
		return ret;
	ret = NI_inner_height(obj, height);
	if (ret != NewIfc_error_none)
		return ret;
	return NewIfc_error_none;
}

NI_err
NI_inner_size_pos(NewIfcObj obj, uint16_t *width, uint16_t *height, uint16_t *xpos, uint16_t *ypos)
{
	NI_err ret;
	ret = NI_inner_width(obj, width);
	if (ret != NewIfc_error_none)
		return ret;
	ret = NI_inner_height(obj, height);
	if (ret != NewIfc_error_none)
		return ret;
	ret = NI_inner_xpos(obj, xpos);
	if (ret != NewIfc_error_none)
		return ret;
	ret = NI_inner_ypos(obj, ypos);
	if (ret != NewIfc_error_none)
		return ret;
	return NewIfc_error_none;
}

static NI_err
NI_get_top_left(NewIfcObj obj, uint16_t width, uint16_t height, uint16_t *xpos, uint16_t *ypos)
{
	switch(obj->align) {
		case NewIfc_align_top_left:
			*xpos = 0;
			*ypos = 0;
			break;
		case NewIfc_align_top_middle:
			*xpos = (obj->layout_size.width - width) / 2;
			*ypos = 0;
			break;
		case NewIfc_align_top_right:
			*xpos = obj->layout_size.width - width;
			*ypos = 0;
			break;
		case NewIfc_align_middle_left:
			*xpos = 0;
			*ypos = (obj->layout_size.height - height) / 2;
			break;
		case NewIfc_align_middle:
			*xpos = (obj->layout_size.width - width) / 2;
			*ypos = (obj->layout_size.height - height) / 2;
			break;
		case NewIfc_align_middle_right:
			*xpos = obj->layout_size.width - width;
			*ypos = (obj->layout_size.height - height) / 2;
			break;
		case NewIfc_align_bottom_left:
			*xpos = 0;
			*ypos = obj->layout_size.height - height;
			break;
		case NewIfc_align_bottom_middle:
			*xpos = (obj->layout_size.width - width) / 2;
			*ypos = obj->layout_size.height - height;
			break;
		case NewIfc_align_bottom_right:
			*xpos = obj->layout_size.width - width;
			*ypos = obj->layout_size.height - height;
			break;
	}
	return NewIfc_error_none;
}

static NI_err
call_destroy_handlers(NewIfcObj obj)
{
	enum NewIfc_event_handlers hval = NewIfc_on_destroy;
	if (obj->handlers == NULL)
		return NewIfc_error_none;
	struct NewIfc_handler **head = bsearch(&hval, obj->handlers, obj->handlers_sz, sizeof(obj->handlers[0]), handler_bsearch_compar);
	if (head == NULL)
		return NewIfc_error_none;
	struct NewIfc_handler *h = *head;
	while (h != NULL) {
		NI_err ret = h->on_render(obj, h->cbdata);
		if (ret != NewIfc_error_none)
			return ret;
		h = h->next;
	}
	return NewIfc_error_none;
}

static NI_err
NI_destroy_recurse(NewIfcObj obj, bool top)
{
	// Recurse children and peers...
	if (obj->bottom_child != NULL)
		NI_destroy_recurse(obj->bottom_child, false);
	if (!top) {
		if (obj->higher_peer != NULL)
			NI_destroy_recurse(obj->higher_peer, false);
	}
	// Ignore return value... you can't stop this train.
	call_destroy_handlers(obj);
	obj->destroy(obj);
	return NewIfc_error_none;
}

NI_err
NI_destroy(NewIfcObj obj)
{
	// Remove from parent/peer lists
	if (obj->higher_peer)
		obj->higher_peer->lower_peer = obj->lower_peer;
	if (obj->lower_peer)
		obj->lower_peer->higher_peer = obj->higher_peer;
	if (obj->parent) {
		if (obj->parent->top_child == obj)
			obj->parent->top_child = obj->lower_peer;
		if (obj->parent->bottom_child == obj)
			obj->parent->bottom_child = obj->higher_peer;
	}
	NI_err ret = NI_destroy_recurse(obj, true);
	return ret;
}

// Lowering a min width/height is always fine.
// Raising a min width/height MAY NOT BE fine.
static NI_err
NI_check_new_min_width(NewIfcObj obj, uint16_t new_width, void *cbdata)
{
	uint16_t old_width;
	NI_err ret;

	(void)cbdata;
	ret = NI_get_min_width(obj, &old_width);
	if (ret != NewIfc_error_none)
		return ret;
	// Lowering min width is always fine.
	if (new_width <= old_width) {
		if (new_width >= 0 && obj->width == NI_SHRINK)
			NI_set_layout_width(obj, new_width);
		return NewIfc_error_none;
	}
	// If there's no parent, it's fine
	if (obj->parent == NULL) {
		if (new_width >= 0 && obj->width == NI_SHRINK)
			NI_set_layout_width(obj, new_width);
		return NewIfc_error_none;
	}
	else {
		// If it fits in the parent, it's fine
		if (obj->parent->child_width >= new_width) {
			if (new_width >= 0 && obj->width == NI_SHRINK)
				NI_set_layout_width(obj, new_width);
			return NewIfc_error_none;
		}
	}
	// Now, we know it doesn't fit where it is without adjusting
	// *something* we need a way to know if layout would succeed
	// with this new value... very tricky.
	// TODO 
	return NewIfc_error_wont_fit;
}

// Lowering a min width/height is always fine.
// Raising a min width/height MAY NOT BE fine.
static NI_err
NI_check_new_min_height(NewIfcObj obj, uint16_t new_height, void *cbdata)
{
	uint16_t old_height;
	NI_err ret;

	(void)cbdata;
	ret = NI_get_min_height(obj, &old_height);
	if (ret != NewIfc_error_none)
		return ret;
	// Lowering min height is always fine.
	if (new_height <= old_height) {
		if (new_height >= 0 && obj->height == NI_SHRINK)
			NI_set_layout_height(obj, new_height);
		return NewIfc_error_none;
	}
	if (obj->parent == NULL) {
		if (new_height >= 0 && obj->height == NI_SHRINK)
			NI_set_layout_height(obj, new_height);
		return NewIfc_error_none;
	}
	else {
		// If it fits in the parent, it's fine
		if (obj->parent->child_height >= new_height) {
			if (new_height >= 0 && obj->height == NI_SHRINK)
				NI_set_layout_height(obj, new_height);
			return NewIfc_error_none;
		}
	}
	// Now, we know it doesn't fit where it is without adjusting
	// *something* we need a way to know if layout would succeed
	// with this new value... very tricky.
	// TODO 
	return NewIfc_error_wont_fit;
}

static NI_err
NI_setup_globals(NewIfcObj obj, NewIfcObj parent)
{
	NI_err ret;

	if (parent) {
		ret = NI_copy_globals(obj, parent);
		if (ret != NewIfc_error_none)
			return ret;
		obj->parent = parent;
		obj->root = parent->root;
		obj->lower_peer = parent->top_child;
		parent->top_child = obj;
		if (parent->bottom_child == NULL)
			parent->bottom_child = obj;
		obj->width = NI_GROW;
		obj->height = NI_GROW;
		ret = NI_add_min_width_handler(obj, NI_check_new_min_width, NULL);
		ret = NI_add_min_height_handler(obj, NI_check_new_min_height, NULL);
	}
	else {
		obj->root = obj;
		ret = NI_add_min_width_handler(obj, NI_check_new_min_width, NULL);
		ret = NI_add_min_height_handler(obj, NI_check_new_min_height, NULL);
	}
	NI_set_dirty(obj, true);

	return NewIfc_error_none;
}

static void
set_vmem_cell(struct NewIfc_render_context *ctx, uint16_t x, uint16_t y, uint32_t fg, uint32_t bg, uint8_t ch, uint8_t font)
{
	int py = ctx->ypos + y;
	int px = ctx->xpos + x;
	struct vmem_cell *cell;

	if (px < 0 || x >= ctx->width)
		return;
	if (py < 0 || y >= ctx->height)
		return;
	cell = &ctx->vmem[py * ctx->dwidth + px];
	if (bg != NI_TRANSPARENT)
		cell->bg = bg;
	if (fg != NI_TRANSPARENT) {
		cell->fg = fg;
		cell->ch = ch;
		if (ciolib_checkfont(font))
			cell->font = font;
	}
	uint8_t la = ciolib_rgb_to_legacyattr(cell->fg, cell->bg);
	// Preserve blink
	la |= (cell->legacy_attr & 0x80);
	cell->legacy_attr = la;
}

NI_err
NI_set_width(NewIfcObj obj, const int32_t value) {
	NI_err ret;
	if (obj == NULL)
		return NewIfc_error_invalid_arg;
	if (value > UINT16_MAX || (value < 0 && value != NI_GROW && value != NI_SHRINK))
		return NewIfc_error_out_of_range;
	if (NI_set_locked(obj, true) == NewIfc_error_none) {
		ret = call_int32_change_handlers(obj, NewIfc_width, value);
		if (ret != NewIfc_error_none) {
			NI_set_locked(obj, false);
			return ret;
		}
		ret = obj->set(obj, NewIfc_width, value);
		if (ret == NewIfc_error_not_implemented) {
			obj->width = value;
			if (value >= 0)
				NI_set_min_width(obj, obj->width);
			if (obj->parent && value == NI_GROW)
				NI_set_layout_width(obj, obj->parent->child_width - obj->xpos);
			else if (value >= 0)
				NI_set_layout_width(obj, value);
			ret = NewIfc_error_none;
		}
		NI_set_locked(obj, false);
	}
	else
		ret = NewIfc_error_lock_failed;
	return ret;
}

NI_err
NI_set_height(NewIfcObj obj, const int32_t value) {
	NI_err ret;
	if (obj == NULL)
		return NewIfc_error_invalid_arg;
	if (value > UINT16_MAX || (value < 0 && value != NI_GROW && value != NI_SHRINK))
		return NewIfc_error_out_of_range;
	if (NI_set_locked(obj, true) == NewIfc_error_none) {
		ret = call_int32_change_handlers(obj, NewIfc_height, value);
		if (ret != NewIfc_error_none) {
			NI_set_locked(obj, false);
			return ret;
		}
		ret = obj->set(obj, NewIfc_height, value);
		if (ret == NewIfc_error_not_implemented) {
			obj->height = value;
			if (value >= 0)
				NI_set_min_height(obj, obj->height);
			if (obj->parent && (value == NI_GROW || value >= 0))
				NI_set_layout_height(obj, obj->parent->child_height - obj->ypos);
			else if (value >= 0)
				NI_set_layout_height(obj, value);
			ret = NewIfc_error_none;
		}
		NI_set_locked(obj, false);
	}
	else
		ret = NewIfc_error_lock_failed;
	return ret;
}

static NI_err do_layout_set_minsize_recurse(NewIfcObj obj, struct lo_size *ms);

static NI_err
do_layout_recurse_nonnull(NewIfcObj obj, struct lo_size *ms)
{
	if (obj)
		return do_layout_set_minsize_recurse(obj, ms);
	return NewIfc_error_none;
}

static NI_err
do_layout_set_minsize_recurse(NewIfcObj obj, struct lo_size *pms)
{
	NI_err ret;
	uint32_t news;

	ret = do_layout_recurse_nonnull(obj->lower_peer, pms);
	if (ret == NewIfc_error_none) {
		memset(&obj->layout_min_size, 0, sizeof(obj->layout_min_size));
		do_layout_recurse_nonnull(obj->top_child, &obj->layout_min_size);
	}
	if (ret != NewIfc_error_none)
		return ret;

	// This assumes the difference between width and child_width is a constant
	if (obj->child_width) {
		news = (uint32_t)obj->layout_min_size.width + (obj->layout_size.width - obj->child_width);
		if (news > UINT16_MAX)
			return NewIfc_error_wont_fit;
		if (news > obj->layout_min_size.width)
			obj->layout_min_size.width = news;
	}

	// This assumes the difference between height and child_height is a constant
	if (obj->child_height) {
		news = (uint32_t)obj->layout_min_size.height + (obj->layout_size.height - obj->child_height);
		if (news > UINT16_MAX)
			return NewIfc_error_wont_fit;
		if (news > obj->layout_min_size.height)
			obj->layout_min_size.height = news;
	}

	if (obj->min_width > obj->layout_min_size.width)
		obj->layout_min_size.width = obj->min_width;
	if (obj->min_height > obj->layout_min_size.height)
		obj->layout_min_size.height = obj->min_height;

	news = (uint32_t)obj->layout_min_size.width + obj->left_pad + obj->right_pad + obj->xpos;
	if (news > UINT16_MAX)
		return NewIfc_error_wont_fit;
	if (news > pms->width)
		pms->width = news;

	news = (uint32_t)obj->layout_min_size.height + obj->left_pad + obj->right_pad + obj->ypos;
	if (news > UINT16_MAX)
		return NewIfc_error_wont_fit;
	if (news > pms->height)
		pms->height = news;
	return NewIfc_error_none;
}

static NI_err
do_layout_set_size_recurse(NewIfcObj obj)
{
	NI_err ret;
	// First, we do ourself...
	if (obj->width == NI_SHRINK) {
		ret = NI_set_layout_width(obj, obj->layout_min_size.width);
		if (ret != NewIfc_error_none)
			return ret;
	}
	else if (obj->width == NI_GROW) {
		if (obj->parent == NULL)
			return NewIfc_error_needs_parent;
		if (obj->parent->child_width < obj->xpos)
			return NewIfc_error_wont_fit;
		ret = NI_set_layout_width(obj, obj->parent->child_width - obj->xpos);
		if (ret != NewIfc_error_none)
			return ret;
	}
	else {
		if (obj->parent && obj->parent->child_width < obj->width)
			return NewIfc_error_wont_fit;
	}

	if (obj->height == NI_SHRINK) {
		ret = NI_set_layout_height(obj, obj->layout_min_size.height);
		if (ret != NewIfc_error_none)
			return ret;
	}
	else if (obj->height == NI_GROW) {
		if (obj->parent == NULL)
			return NewIfc_error_needs_parent;
		if (obj->parent->child_height < obj->xpos)
			return NewIfc_error_wont_fit;
		ret = NI_set_layout_height(obj, obj->parent->child_height - obj->xpos);
		if (ret != NewIfc_error_none)
			return ret;
	}
	else {
		if (obj->parent && obj->parent->child_height < obj->height)
			return NewIfc_error_wont_fit;
	}

	// Then, recurse peers
	if (obj->higher_peer) {
		ret = do_layout_set_size_recurse(obj->higher_peer);
		if (ret != NewIfc_error_none)
			return ret;
	}

	// And finally, recurse children
	if (obj->bottom_child) {
		ret = do_layout_set_size_recurse(obj->bottom_child);
		if (ret != NewIfc_error_none)
			return ret;
	}

	return NewIfc_error_none;
}

/*
 * This walks up from every leaf, updating min width and min height, then
 * walks *down* from root setting width and height (and adjusting sizers).
 * It is expected that this can fail with NewIFc_error_wont_fit as it will
 * be called immediately after a min_size change anywhere in the tree.
 * 
 * The caller will restore the old value then call this again, which will
 * presumably work.
 */
NI_err
NI_do_layout(NewIfcObj obj)
{
	NI_err ret;

	// Walk up setting minwidth and minheight
	memset(&obj->root->layout_min_size, 0, sizeof(obj->root->layout_min_size));
	ret = do_layout_set_minsize_recurse(obj->root, &obj->root->layout_min_size);
	if (ret != NewIfc_error_none)
		return ret;

	// Walk down setting layout width and height.
	ret = do_layout_set_size_recurse(obj->root);
	if (ret != NewIfc_error_none)
		return ret;

	ret = NI_set_dirty(obj, false);
	if (ret != NewIfc_error_none)
		return ret;

	return NewIfc_error_none;
}
