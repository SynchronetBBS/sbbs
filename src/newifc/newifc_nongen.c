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
NI_walk_children_recurse(NewIfcObj obj, NI_err (*cb)(NewIfcObj obj, void *cb_data), void *cbdata)
{
	NI_err err;

	if (!obj)
		return NewIfc_error_none;
	err = cb(obj, cbdata);
	if (err != NewIfc_error_none)
		return err;
	if (obj->bottomchild != NULL) {
		err = NI_walk_children_recurse(obj->bottomchild, cb, cbdata);
		if (err != NewIfc_error_none)
			return err;
	}
	if (!obj->higherpeer)
		return NewIfc_error_none;
	return NI_walk_children_recurse(obj->higherpeer, cb, cbdata);
}

NI_err
NI_walk_children(NewIfcObj obj, NI_err (*cb)(NewIfcObj obj, void *cb_data), void *cbdata)
{
	NI_err ret;

	if (obj == NULL)
		return NewIfc_error_invalid_arg;
	if (cb == NULL)
		return NewIfc_error_invalid_arg;
	ret = NI_set_locked(obj, true);
	if (ret == NewIfc_error_none) {
		ret = NI_walk_children_recurse(obj->bottomchild, cb, cbdata);
		NI_set_locked(obj, false);
	}
	else
		ret = NewIfc_error_lock_failed;
	return ret;
}
