#include <stdlib.h>

#include "newifc.h"

NI_err
NI_copy(NewIfcObj obj, NewIfcObj *newobj) {
	if (newobj == NULL) {
		return NewIfc_error_invalid_arg;
	}
	*newobj = NULL;
	if (obj == NULL) {
		return NewIfc_error_invalid_arg;
	}
	NI_err ret = NewIfc_error_none;
	if (NI_set_locked(obj, true)) {
		ret = obj->copy(obj, newobj);
		if (ret == NewIfc_error_none) {
			(*newobj)->root = NULL;
			(*newobj)->parent = NULL;
			(*newobj)->higher_peer = NULL;
			(*newobj)->lower_peer = NULL;
			(*newobj)->top_child = NULL;
			(*newobj)->bottom_child = NULL;
		}
		NI_set_locked(obj, false);
	}
	else
		ret = NewIfc_error_lock_failed;
	return ret;
}

static NI_err
NI_walk_children_recurse(NewIfcObj obj, bool top_down, NI_err (*cb)(NewIfcObj obj, void *cb_data), void *cbdata)
{
	NI_err err;
	NewIfcObj nobj;

	if (!obj)
		return NewIfc_error_none;
	err = cb(obj, cbdata);
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
	ret = NI_set_locked(obj, true);
	if (ret == NewIfc_error_none) {
		ret = NI_walk_children_recurse(top_down ? obj->top_child : obj->bottom_child, top_down, cb, cbdata);
		NI_set_locked(obj, false);
	}
	else
		ret = NewIfc_error_lock_failed;
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
	size_t new_sz = obj->handlers_sz;
	struct NewIfc_handler ***na = realloc(obj->handlers, new_sz * sizeof(obj->handlers[0]));
	if (na == NULL)
		return NewIfc_error_allocation_failure;
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
	uint32_t both = (uint32_t)obj->left_pad + obj->right_pad;
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
	uint32_t both = (uint32_t)obj->top_pad + obj->bottom_pad;
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
			*xpos = (obj->width - width) / 2;
			*ypos = 0;
			break;
		case NewIfc_align_top_right:
			*xpos = obj->width - width;
			*ypos = 0;
			break;
		case NewIfc_align_middle_left:
			*xpos = 0;
			*ypos = (obj->height - height) / 2;
			break;
		case NewIfc_align_middle:
			*xpos = (obj->width - width) / 2;
			*ypos = (obj->height - height) / 2;
			break;
		case NewIfc_align_middle_right:
			*xpos = obj->width - width;
			*ypos = (obj->height - height) / 2;
			break;
		case NewIfc_align_bottom_left:
			*xpos = 0;
			*ypos = obj->height - height;
			break;
		case NewIfc_align_bottom_middle:
			*xpos = (obj->width - width) / 2;
			*ypos = obj->height - height;
			break;
		case NewIfc_align_bottom_right:
			*xpos = obj->width - width;
			*ypos = obj->height - height;
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
