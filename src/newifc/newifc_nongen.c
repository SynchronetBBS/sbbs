NewIfcObj
NI_copy(NewIfcObj obj) {
	return obj->copy(obj);
}

enum NewIfc_error
NI_error(NewIfcObj obj)
{
	return obj->last_error;
}

static bool
NI_walk_children_recurse(NewIfcObj obj, bool (*cb)(NewIfcObj obj, void *cb_data), void *cbdata)
{
	if (!obj)
		return true;
	if (!cb(obj, cbdata))
		return false;
	if (obj->bottomchild != NULL) {
		if (!NI_walk_children_recurse(obj->bottomchild, cb, cbdata))
			return false;
	}
	if (!obj->higherpeer)
		return true;
	return NI_walk_children_recurse(obj->higherpeer, cb, cbdata);
}

bool
NI_walk_children(NewIfcObj obj, bool (*cb)(NewIfcObj obj, void *cb_data), void *cbdata)
{
	bool ret;
	if (NI_set_locked(obj, true)) {
		NI_walk_children_recurse(obj->bottomchild, cb, cbdata);
		NI_set_locked(obj, false);
	}
	else
		ret = false;
	return ret;
}
