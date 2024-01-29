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
			(*newobj)->higherpeer = NULL;
			(*newobj)->lowerpeer = NULL;
			(*newobj)->topchild = NULL;
			(*newobj)->bottomchild = NULL;
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
		nobj = obj->topchild;
	else
		nobj = obj->bottomchild;

	if (nobj != NULL) {
		err = NI_walk_children_recurse(nobj, top_down, cb, cbdata);
		if (err != NewIfc_error_none && err != NewIfc_error_skip_subtree)
			return err;
	}
	if (top_down)
		nobj = obj->lowerpeer;
	else
		nobj = obj->higherpeer;

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
		ret = NI_walk_children_recurse(obj->bottomchild, top_down, cb, cbdata);
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
