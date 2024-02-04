#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum ni_type {
	ni_bool,
	ni_uint32,
	ni_string,
};

struct objtype_info {
	const char *name;
	int needs_parent;
};

const struct objtype_info
objtypes[] = {
	{"root_window", 0},
	{"label", 1},
	{"sizer", 1},
	{"sizer_cell", 1},
};

enum attribute_types {
	NI_attr_type_NewIfcObj,
	NI_attr_type_uint16_t,
	NI_attr_type_uint8_t,
	NI_attr_type_uint32_t,
	NI_attr_type_bool,
	NI_attr_type_charptr,
	NI_attr_type_NewIfc_object,
	NI_attr_type_NewIfc_align,
	NI_attr_type_int32_t,
	NI_attr_type_voidptr_t,
};

struct type_str_values {
	const char *type;
	const char *var_name;
};

struct type_str_values type_str[] = {
	{"NewIfcObj", "obj"},
	{"uint16_t", "uint16"},
	{"uint8_t", "uint8"},
	{"uint32_t", "uint32"},
	{"bool", "bool"},
	{"char *", "char_ptr"},
	{"enum NewIfc_object", "obj_enum"},
	{"enum NewIfc_alignment", "align_enum"},
	{"int32_t", "int32"},
	{"void *", "void_ptr"},
};

enum attribute_impl {
	attr_impl_global,
	attr_impl_object,
	attr_impl_root,
	attr_impl_global_custom_setter,
};

struct attribute_info {
	const char *name;
	const char *member_name;
	enum attribute_types type;
	enum attribute_impl impl;
	int read_only;
	int no_event;
	int inherit;
};

const struct attribute_info
attributes[] = {
	{"align", NULL, NI_attr_type_NewIfc_align, attr_impl_global, 0, 0, 1},
	{"bg_colour", NULL, NI_attr_type_uint32_t, attr_impl_global, 0, 0, 1},
	{"bottom_child", NULL, NI_attr_type_NewIfcObj, attr_impl_global, 1, 0, 0},
	{"bottom_pad", NULL, NI_attr_type_uint16_t, attr_impl_global, 0, 0, 0},
	{"child_height", NULL, NI_attr_type_uint16_t, attr_impl_global, 1, 0, 0},
	{"child_width", NULL, NI_attr_type_uint16_t, attr_impl_global, 1, 0, 0},
	{"child_xpos", NULL, NI_attr_type_uint16_t, attr_impl_global, 1, 0, 0},
	{"child_ypos", NULL, NI_attr_type_uint16_t, attr_impl_global, 1, 0, 0},
	{"fg_colour", NULL, NI_attr_type_uint32_t, attr_impl_global, 0, 0, 1},
	{"fill_character", NULL, NI_attr_type_uint8_t, attr_impl_global, 0, 0, 1},
	{"fill_character_colour", NULL, NI_attr_type_uint32_t, attr_impl_global, 0, 0, 1},
	{"fill_colour", NULL, NI_attr_type_uint32_t, attr_impl_global, 0, 0, 1},
	{"fill_font", NULL, NI_attr_type_uint8_t, attr_impl_global, 0, 0, 1},
	{"focus", NULL, NI_attr_type_bool, attr_impl_global_custom_setter, 0, 0, 0},
	{"font", NULL, NI_attr_type_uint8_t, attr_impl_global, 0, 0, 1},
	{"group_size", NULL, NI_attr_type_uint8_t, attr_impl_object, 0, 0, 0},
	{"height", NULL, NI_attr_type_int32_t, attr_impl_global_custom_setter, 0, 0, 0},
	{"hidden", NULL, NI_attr_type_bool, attr_impl_global, 1, 0, 1},
	{"higher_peer", NULL, NI_attr_type_NewIfcObj, attr_impl_global, 1, 0, 0},
	{"layout_width", "layout_size.width", NI_attr_type_uint16_t, attr_impl_global, 1, 0, 0},
	{"layout_height", "layout_size.height", NI_attr_type_uint16_t, attr_impl_global, 1, 0, 0},
	{"locked", NULL, NI_attr_type_bool, attr_impl_root, 0, 1, 0},
	{"locked_by_me", NULL, NI_attr_type_bool, attr_impl_root, 1, 1, 0},
	{"lower_peer", NULL, NI_attr_type_NewIfcObj, attr_impl_global, 1, 0, 0},
	{"left_pad", NULL, NI_attr_type_uint16_t, attr_impl_global, 0, 0, 0},
	{"min_height", NULL, NI_attr_type_uint16_t, attr_impl_global, 1, 0, 0},
	{"min_width", NULL, NI_attr_type_uint16_t, attr_impl_global, 1, 0, 0},
	{"parent", NULL, NI_attr_type_NewIfcObj, attr_impl_global, 1, 0, 0},
	{"right_pad", NULL, NI_attr_type_uint16_t, attr_impl_global, 0, 0, 0},
	{"root", NULL, NI_attr_type_NewIfcObj, attr_impl_global, 1, 0, 1},
	{"text", NULL, NI_attr_type_charptr, attr_impl_object, 0, 0, 0},
	{"top_pad", NULL, NI_attr_type_uint16_t, attr_impl_global, 0, 0, 0},
	{"top_child", NULL, NI_attr_type_NewIfcObj, attr_impl_global, 1, 0, 0},
	{"type", NULL, NI_attr_type_NewIfc_object, attr_impl_global, 1, 0, 0},
	{"user_data", NULL, NI_attr_type_voidptr_t, attr_impl_global, 0, 0, 0},
	{"weight", NULL, NI_attr_type_uint8_t, attr_impl_object, 0, 0, 0},
	{"width", NULL, NI_attr_type_int32_t, attr_impl_global_custom_setter, 0, 0, 0},
	{"xpos", NULL, NI_attr_type_uint16_t, attr_impl_global, 0, 0, 0},
	{"ypos", NULL, NI_attr_type_uint16_t, attr_impl_global, 0, 0, 0},
};

struct attribute_alias {
	const char *attribute_name;
	const char *alias_name;
};

const struct attribute_alias aliases[] = {
	{"fill_colour", "fill_color"},
	{"fill_character_colour", "fill_character_color"},
	{"fg_colour", "fg_color"},
	{"bg_colour", "bg_color"},
};

struct error_info {
	const char *name;
	int value;
};

const struct error_info
error_inf[] = {
	{"error_none", 0},
	{"error_allocation_failure", -1},
	{"error_invalid_arg", -1},
	{"error_lock_failed", -1},
	{"error_needs_parent", -1},
	{"error_not_implemented", -1},
	{"error_out_of_range", -1},
	{"error_wont_fit", -1},
	{"error_cancelled", -1},
	{"error_skip_subtree", -1},
};

struct handler_info {
	const char *name;
	const char *prototype;
	const char *args;
	const char *params;
};

const struct handler_info
extra_handlers[] = {
	{"on_layout_minsize", "NI_err (*on_layout_minsize)(NewIfcObj obj, void *cbdata)", NULL, NULL},
	{"on_layout_size", "NI_err (*on_layout_size)(NewIfcObj obj, void *cbdata)", NULL, NULL},
	{"on_render", "NI_err (*on_render)(NewIfcObj obj, void *cbdata)", NULL, NULL},
	{"on_destroy", "NI_err (*on_destroy)(NewIfcObj obj, void *cbdata)", NULL, NULL},
	{"on_key", "NI_err (*on_key)(NewIfcObj obj, uint16_t scancode, void *cbdata)", ", uint16_t scancode", ", scancode"},
};

const char *member_name(size_t idx)
{
	if (attributes[idx].member_name == NULL)
		return attributes[idx].name;
	return attributes[idx].member_name;
}

size_t
find_attribute(const char *name)
{
	size_t nitems = sizeof(attributes) / sizeof(attributes[0]);
	// Linear search in case they're not alphabetical
	for (size_t i = 0; i < nitems; i++) {
		if (strcmp(attributes[i].name, name) == 0)
			return i;
	}
	return -1;
}

void
attribute_functions(size_t i, FILE *c_code, const char *alias)
{
	switch (attributes[i].impl) {
		case attr_impl_object:
			if (!attributes[i].read_only) {
				fprintf(c_code, "NI_err\n"
						"NI_set_%s(NewIfcObj obj, const %s value) {\n"
						"	NI_err ret;\n"
						"	if (obj == NULL)\n"
						"		return NewIfc_error_invalid_arg;\n"
						"	if (NI_set_locked(obj, true) == NewIfc_error_none) {\n", alias, type_str[attributes[i].type].type);
				if (!attributes[i].no_event) {
					fprintf(c_code, "		ret = call_%s_change_handlers(obj, NewIfc_%s_handler, value);\n"
							"		if (ret != NewIfc_error_none) {\n"
							"			NI_set_locked(obj, false);\n"
							"			return ret;\n"
							"		}\n", type_str[attributes[i].type].var_name, attributes[i].name);
				}
				fprintf(c_code, "		ret = obj->set(obj, NewIfc_%s, value);\n"
						"		NI_set_locked(obj, false);\n"
						"	}\n"
						"	else\n"
						"		ret = NewIfc_error_lock_failed;\n"
						"	return ret;\n"
						"}\n\n", attributes[i].name);
			}

			fprintf(c_code, "NI_err\n"
					"NI_get_%s(NewIfcObj obj, %s* value) {\n"
					"	NI_err ret;\n"
					"	if (obj == NULL)\n"
					"		return NewIfc_error_invalid_arg;\n"
					"	if (value == NULL)\n"
					"		return NewIfc_error_invalid_arg;\n"
					"	if (NI_set_locked(obj, true) == NewIfc_error_none) {\n"
					"		ret = obj->get(obj, NewIfc_%s, value);\n"
					"		NI_set_locked(obj, false);\n"
					"	}\n"
					"	else\n"
					"		ret = NewIfc_error_lock_failed;\n"
					"	return ret;\n"
					"}\n\n", alias, type_str[attributes[i].type].type, attributes[i].name);
			break;
		case attr_impl_global:
			fprintf(c_code, "%sNI_err\n"
					"NI_set_%s(NewIfcObj obj, const %s value) {\n"
					"	NI_err ret;\n"
					"	if (obj == NULL)\n"
					"		return NewIfc_error_invalid_arg;\n"
					"	if (NI_set_locked(obj, true) == NewIfc_error_none) {\n", attributes[i].read_only ? "static " : "", alias, type_str[attributes[i].type].type);
			if (!attributes[i].no_event) {
				fprintf(c_code, "		ret = call_%s_change_handlers(obj, NewIfc_%s_handler, value);\n"
						"		if (ret != NewIfc_error_none) {\n"
						"			NI_set_locked(obj, false);\n"
						"			return ret;\n"
						"		}\n", type_str[attributes[i].type].var_name, attributes[i].name);
			}
			fprintf(c_code, "		ret = obj->set(obj, NewIfc_%s, value);\n"
					"		if (ret == NewIfc_error_not_implemented) {\n"
					"			obj->%s = value;\n"
					"			ret = NewIfc_error_none;\n"
					"		}\n"
					"		NI_set_locked(obj, false);\n"
					"	}\n"
					"	else\n"
					"		ret = NewIfc_error_lock_failed;\n"
					"	return ret;\n"
					"}\n\n", attributes[i].name, member_name(i));

			// Fall-through
		case attr_impl_global_custom_setter:
			fprintf(c_code, "NI_err\n"
					"NI_get_%s(NewIfcObj obj, %s* value) {\n"
					"	NI_err ret;\n"
					"	if (obj == NULL)\n"
					"		return NewIfc_error_invalid_arg;\n"
					"	if (value == NULL)\n"
					"		return NewIfc_error_invalid_arg;\n"
					"	if (NI_set_locked(obj, true) == NewIfc_error_none) {\n"
					"		ret = obj->get(obj, NewIfc_%s, value);\n"
					"		if (ret == NewIfc_error_not_implemented) {\n"
					"			*value = obj->%s;\n"
					"			ret = NewIfc_error_none;\n"
					"		}\n"
					"		NI_set_locked(obj, false);\n"
					"	}\n"
					"	else\n"
					"		ret = NewIfc_error_lock_failed;\n"
					"	return ret;\n"
					"}\n\n", alias, type_str[attributes[i].type].type, attributes[i].name, member_name(i));
			break;
		case attr_impl_root:
			if (!attributes[i].read_only) {
				fprintf(c_code, "NI_err\n"
						"NI_set_%s(NewIfcObj obj, %s value) {\n"
						"	if (obj == NULL)\n"
						"		return NewIfc_error_invalid_arg;\n"
						"	NI_err ret = obj->root->set(obj->root, NewIfc_%s, value);\n"
						"	return ret;\n"
						"}\n\n", alias, type_str[attributes[i].type].type, attributes[i].name);
			}

			fprintf(c_code, "NI_err\n"
					"NI_get_%s(NewIfcObj obj, %s* value) {\n"
					"	if (obj == NULL)\n"
					"		return NewIfc_error_invalid_arg;\n"
					"	if (value == NULL)\n"
					"		return NewIfc_error_invalid_arg;\n"
					"	NI_err ret = obj->root->get(obj->root, NewIfc_%s, value);\n"
					"	return ret;\n"
					"}\n\n", alias, type_str[attributes[i].type].type, attributes[i].name);
			break;
	}
	if (!attributes[i].no_event) {
		fprintf(c_code, "NI_err\n"
				"NI_add_%s_handler(NewIfcObj obj, NI_err (*handler)(NewIfcObj obj, const %s newval, void *cbdata), void *cbdata)\n"
				"{\n"
				"	NI_err ret;\n"
				"	if (obj == NULL || handler == NULL)\n"
				"		return NewIfc_error_invalid_arg;\n"
				"	if (NI_set_locked(obj, true) == NewIfc_error_none) {\n"
				"		struct NewIfc_handler *h = malloc(sizeof(struct NewIfc_handler));\n"
				"		if (h == NULL) {\n"
				"			NI_set_locked(obj, false);\n"
				"			return NewIfc_error_allocation_failure;\n"
				"		}\n"
				"		h->on_%s_change = handler;\n"
				"		h->cbdata = cbdata;\n"
				"		h->event = NewIfc_on_%s_change;\n"
				"		h->next = NULL;\n"
				"		ret = NI_install_handler(obj, h);\n"
				"		NI_set_locked(obj, false);\n"
				"	}\n"
				"	else\n"
				"		ret = NewIfc_error_lock_failed;\n"
				"	return ret;\n"
				"}\n\n", alias, type_str[attributes[i].type].type, type_str[attributes[i].type].var_name, attributes[i].name);
	}
}

int
main(int argc, char **argv)
{
	FILE *header;
	FILE *internal_header;
	FILE *c_code;
	size_t nitems;
	size_t i;

	(void)argc;
	(void)argv;
	header = fopen("newifc.h", "w");
	if (header == NULL) {
		perror("Opening header");
		return EXIT_FAILURE;
	}

	fputs("/*\n"
	      " * This code is autogeneraged by genapi\n"
	      " */\n\n", header);

	fputs("#ifndef NEWIFC_H\n"
	      "#define NEWIFC_H\n\n", header);

	fputs("#include \"gen_defs.h\"\n\n", header);

	fputs("typedef struct newifc_api *NewIfcObj;\n\n", header);

	fputs("typedef enum NewIfc_error {\n", header);
	nitems = sizeof(error_inf) / sizeof(error_inf[0]);
	for (i = 0; i < nitems; i++) {
		if (error_inf[i].value >= 0)
			fprintf(header, "	NewIfc_%s = %d,\n", error_inf[i].name, error_inf[i].value);
		else
			fprintf(header, "	NewIfc_%s,\n", error_inf[i].name);
	}
	fputs("} NI_err;\n\n", header);

	fputs("enum NewIfc_object {\n", header);
	nitems = sizeof(objtypes) / sizeof(objtypes[0]);
	for (i = 0; i < nitems; i++) {
		fprintf(header, "	NewIfc_%s,\n", objtypes[i].name);
	}
	fputs("};\n\n", header);

	fputs("enum NewIfc_alignment {\n"
	      "	NewIfc_align_top_left,\n"
	      "	NewIfc_align_top_middle,\n"
	      "	NewIfc_align_top_right,\n"
	      "	NewIfc_align_middle_left,\n"
	      "	NewIfc_align_centre,\n"
	      "	NewIfc_align_center = NewIfc_align_centre,\n"
	      "	NewIfc_align_middle = NewIfc_align_centre,\n"
	      "	NewIfc_align_middle_right,\n"
	      "	NewIfc_align_bottom_left,\n"
	      "	NewIfc_align_bottom_middle,\n"
	      "	NewIfc_align_bottom_right,\n"
	      "};\n\n", header);

	fputs("#define NI_TRANSPARENT  UINT32_MAX\n"
	      "#define NI_BLACK        UINT32_C(0)\n"
	      "#define NI_BLUE         UINT32_C(0x800000A8)\n"
	      "#define NI_GREEN        UINT32_C(0x8000A800)\n"
	      "#define NI_CYAN         UINT32_C(0x8000A8A8)\n"
	      "#define NI_RED          UINT32_C(0x80A80000)\n"
	      "#define NI_MAGENTA      UINT32_C(0x80A800A8)\n"
	      "#define NI_BROWN        UINT32_C(0x80A85400)\n"
	      "#define NI_LIGHTGRAY    UINT32_C(0x80A8A8A8)\n"
	      "#define NI_DARKGRAY     UINT32_C(0x80545454)\n"
	      "#define NI_LIGHTBLUE    UINT32_C(0x805454FF)\n"
	      "#define NI_LIGHTGREEN   UINT32_C(0x8054FF54)\n"
	      "#define NI_LIGHTCYAN    UINT32_C(0x8054FFFF)\n"
	      "#define NI_LIGHTRED     UINT32_C(0x80FF5454)\n"
	      "#define NI_LIGHTMAGENTA UINT32_C(0x80FF54FF)\n"
	      "#define NI_YELLOW       UINT32_C(0x80FFFF54)\n"
	      "#define NI_WHITE        UINT32_C(0x80FFFFFF)\n\n", header);

	fputs("#define NI_GROW   -1\n"
	      "#define NI_SHRINK -2\n", header);

	fputs("NI_err NI_create(enum NewIfc_object obj, NewIfcObj parent, NewIfcObj *newobj);\n", header);
	fputs("NI_err NI_destroy(NewIfcObj obj);\n", header);
	fputs("NI_err NI_error(NewIfcObj obj);\n", header);
	fputs("NI_err NI_walk_children(NewIfcObj obj, bool top_down, NI_err (*cb)(NewIfcObj obj, void *cb_data), void *cbdata);\n\n", header);
	fputs("NI_err NI_inner_width(NewIfcObj obj, uint16_t *width);\n", header);
	fputs("NI_err NI_inner_height(NewIfcObj obj, uint16_t *hright);\n", header);
	fputs("NI_err NI_inner_xpos(NewIfcObj obj, uint16_t *xpos);\n", header);
	fputs("NI_err NI_inner_ypos(NewIfcObj obj, uint16_t *ypos);\n", header);
	fputs("NI_err NI_inner_size(NewIfcObj obj, uint16_t *width, uint16_t *height);\n", header);
	fputs("NI_err NI_inner_size_pos(NewIfcObj obj, uint16_t *width, uint16_t *height, uint16_t *xpos, uint16_t *ypos);\n", header);

	nitems = sizeof(attributes) / sizeof(attributes[0]);
	for (i = 0; i < nitems; i++) {
		if (!attributes[i].read_only) {
			fprintf(header, "NI_err NI_set_%s(NewIfcObj obj, const %s value);\n", attributes[i].name, type_str[attributes[i].type].type);
		}
		if (!attributes[i].no_event)
			fprintf(header, "NI_err NI_add_%s_handler(NewIfcObj obj, NI_err (*handler)(NewIfcObj obj, const %s newval, void *cbdata), void *cbdata);\n", attributes[i].name, type_str[attributes[i].type].type);
		fprintf(header, "NI_err NI_get_%s(NewIfcObj obj, %s* value);\n", attributes[i].name, type_str[attributes[i].type].type);
	}
	nitems = sizeof(aliases) / sizeof(aliases[0]);
	for (i = 0; i < nitems; i++) {
		size_t a = find_attribute(aliases[i].attribute_name);
		if (!attributes[a].read_only) {
			fprintf(header, "NI_err NI_set_%s(NewIfcObj obj, const %s value);\n", aliases[i].alias_name, type_str[attributes[a].type].type);
		}
		if (!attributes[i].no_event)
			fprintf(header, "NI_err NI_add_%s_handler(NewIfcObj obj, NI_err (*handler)(NewIfcObj obj, const %s newval, void *cbdata), void *cbdata);\n", aliases[i].alias_name, type_str[attributes[a].type].type);
		fprintf(header, "NI_err NI_get_%s(NewIfcObj obj, %s* value);\n", aliases[i].alias_name, type_str[attributes[a].type].type);
	}
	nitems = sizeof(extra_handlers) / sizeof(extra_handlers[0]);
	for (i = 0; i < nitems; i++) {
		fprintf(header, "NI_err NI_add_%s_handler(NewIfcObj obj, %s);\n", extra_handlers[i].name, extra_handlers[i].prototype);
	}
	fputs("\n#endif\n", header);
	fclose(header);


	internal_header = fopen("newifc_internal.h", "w");
	if (internal_header == NULL) {
		perror("Opening internal header");
		return EXIT_FAILURE;
	}
	fputs("/*\n"
	      " * This code is autogeneraged by genapi\n"
	      " */\n\n", internal_header);

	fputs("#ifndef NEWIFC_INTERNAL_H\n"
	      "#define NEWIFC_INTERNAL_H\n\n", internal_header);

	fputs("#include \"ciolib.h\"\n\n", internal_header);

	fputs("enum NewIfc_event_handlers {\n", internal_header);
	nitems = sizeof(attributes) / sizeof(attributes[0]);
	for (i = 0; i < nitems; i++) {
		fprintf(internal_header, "	NewIfc_on_%s_change,\n", attributes[i].name);
	}
	nitems = sizeof(extra_handlers) / sizeof(extra_handlers[0]);
	for (i = 0; i < nitems; i++) {
		fprintf(internal_header, "	NewIfc_%s,\n", extra_handlers[i].name);
	}
	fputs("};\n\n", internal_header);

	fputs("struct NewIfc_handler {\n"
	      "	struct NewIfc_handler *next;\n"
	      "	void *cbdata;\n"
	      "	union {\n", internal_header);
	nitems = sizeof(type_str) / sizeof(type_str[0]);
	for (i = 0; i < nitems; i++) {
	      fprintf(internal_header, "		NI_err (*on_%s_change)(NewIfcObj obj, const %s newval, void *cbdata);\n", type_str[i].var_name, type_str[i].type);
	}
	nitems = sizeof(extra_handlers) / sizeof(extra_handlers[0]);
	for (i = 0; i < nitems; i++) {
	      fprintf(internal_header, "		%s;\n", extra_handlers[i].prototype);
	}
	fputs("	};\n"
	      "	enum NewIfc_event_handlers event;\n"
	      "};\n\n", internal_header);

	fputs("struct NewIfc_render_context {\n"
	      "	struct vmem_cell *vmem;\n"
	      "	uint16_t dwidth;\n"
	      "	uint16_t dheight;\n"
	      "	uint16_t width;\n"
	      "	uint16_t height;\n"
	      "	uint16_t xpos;\n"
	      "	uint16_t ypos;\n"
	      "};\n\n", internal_header);

	fputs("struct lo_size {\n"
	      "	uint16_t width;\n"
	      "	uint16_t height;\n"
	      "};\n\n", internal_header);

	fputs("struct newifc_api {\n"
	      "	NI_err (*set)(NewIfcObj niobj, const int attr, ...);\n"
	      "	NI_err (*get)(NewIfcObj niobj, const int attr, ...);\n"
	      "	NI_err (*do_render)(NewIfcObj obj, struct NewIfc_render_context *ctx);\n"
	      "	NI_err (*destroy)(NewIfcObj obj);\n"
	      "	struct NewIfc_handler **handlers;\n"
	      "	NewIfcObj root;\n"
	      "	NewIfcObj parent;\n"
	      "	NewIfcObj higher_peer;\n"
	      "	NewIfcObj lower_peer;\n"
	      "	NewIfcObj top_child;\n"
	      "	NewIfcObj bottom_child;\n"
	      "	void * user_data;\n"
	      "	size_t handlers_sz;\n"
	      "	struct lo_size layout_min_size;\n"
	      "	struct lo_size layout_size;\n"
	      "	uint32_t fill_character_colour;\n"
	      "	uint32_t fill_colour;\n"
	      "	uint32_t fg_colour;\n"
	      "	uint32_t bg_colour;\n"
	      "	enum NewIfc_object type;\n"
	      "	enum NewIfc_alignment align;\n"
	      "	int32_t height;\n"
	      "	int32_t width;\n"
	      "	uint16_t min_height;\n"
	      "	uint16_t min_width;\n"
	      "	uint16_t xpos;\n"
	      "	uint16_t ypos;\n"
	      "	uint16_t child_height;\n"
	      "	uint16_t child_width;\n"
	      "	uint16_t child_xpos;\n"
	      "	uint16_t child_ypos;\n"
	      "	uint16_t left_pad;\n"
	      "	uint16_t right_pad;\n"
	      "	uint16_t bottom_pad;\n"
	      "	uint16_t top_pad;\n"
	      "	uint8_t fill_character;\n"
	      "	uint8_t fill_font;\n"
	      "	uint8_t font;\n"
	      "	unsigned focus:1;\n"
	      "	unsigned hidden:1;\n"
	      "};\n\n", internal_header);

	fputs("enum NewIfc_attribute {\n", internal_header);
	nitems = sizeof(attributes) / sizeof(attributes[0]);
	for (i = 0; i < nitems; i++) {
		fprintf(internal_header, "	NewIfc_%s,\n", attributes[i].name);
	}
	fputs("};\n\n", internal_header);

	fputs("enum NewIfc_handler_type {\n", internal_header);
	nitems = sizeof(attributes) / sizeof(attributes[0]);
	for (i = 0; i < nitems; i++) {
		fprintf(internal_header, "	NewIfc_%s_handler,\n", attributes[i].name);
	}
	nitems = sizeof(extra_handlers) / sizeof(extra_handlers[0]);
	for (i = 0; i < nitems; i++) {
		fprintf(internal_header, "	NewIfc_%s_handler,\n", extra_handlers[i].name);
	}
	fputs("};\n\n", internal_header);

	nitems = sizeof(attributes) / sizeof(attributes[0]);
	for (i = 0; i < nitems; i++) {
		if (attributes[i].read_only && attributes[i].impl == attr_impl_global) {
			fprintf(header, "static NI_err NI_set_%s(NewIfcObj obj, const %s value);\n", attributes[i].name, type_str[attributes[i].type].type);
		}
	}

	fputs("static NI_err NI_copy_globals(NewIfcObj dst, NewIfcObj src);\n", internal_header);

	nitems = sizeof(type_str) / sizeof(type_str[0]);
	for (i = 0; i < nitems; i++) {
		fprintf(internal_header, "static NI_err call_%s_change_handlers(NewIfcObj obj, enum NewIfc_handler_type type, const %s newval);\n", type_str[i].var_name, type_str[i].type);
	}
	nitems = sizeof(extra_handlers) / sizeof(extra_handlers[0]);
	for (i = 0; i < nitems; i++) {
		fprintf(internal_header, "static NI_err call_%s_handlers(NewIfcObj obj, enum NewIfc_handler_type type%s);\n", extra_handlers[i].name, extra_handlers[i].args ? extra_handlers[i].args : "");
	}

	fputs("\n", internal_header);
	fputs("#endif\n", internal_header);
	fclose(internal_header);

	c_code = fopen("newifc.c", "w");
	if (c_code == NULL) {
		perror("Opening c source");
		return EXIT_FAILURE;
	}
	fputs("/*\n"
	      " * This code is autogeneraged by genapi\n"
	      " */\n\n", c_code);

	fputs("#include \"newifc.h\"\n", c_code);
	fputs("#include \"newifc_internal.h\"\n", c_code);
	fputs("\n", c_code);

	nitems = sizeof(objtypes) / sizeof(objtypes[0]);
	for (i = 0; i < nitems; i++) {
		fprintf(c_code, "static NI_err NewIFC_%s(NewIfcObj parent, NewIfcObj *newobj);\n", objtypes[i].name);
	}
	fputs("\n", c_code);

	fputs("NI_err\n"
	      "NI_create(enum NewIfc_object obj, NewIfcObj parent, NewIfcObj *newobj)\n"
	      "{\n"
	      "	NI_err ret = NewIfc_error_none;\n"
	      "\n"
	      "	if (newobj == NULL)\n"
	      "		return NewIfc_error_invalid_arg;\n"
	      "	*newobj = NULL;\n"
	      "	switch(obj) {\n", c_code);
	nitems = sizeof(objtypes) / sizeof(objtypes[0]);
	for (i = 0; i < nitems; i++) {
		fprintf(c_code, "		case NewIfc_%s:\n", objtypes[i].name);
		if (objtypes[i].needs_parent) {
			fputs("			if (parent == NULL) {\n"
			      "				ret = NewIfc_error_invalid_arg;\n"
			      "				break;\n"
			      "			}\n", c_code);
		}
		else {
			fputs("			if (parent != NULL) {\n"
			      "				ret = NewIfc_error_invalid_arg;\n"
			      "				break;\n"
			      "			}\n", c_code);
		}
		fprintf(c_code, "			ret = NewIFC_%s(parent, newobj);\n"
		                "			if (ret != NewIfc_error_none) {\n"
		                "				break;\n"
		                "			}\n"
		                "			(*newobj)->type = obj;\n"
		                "			break;\n", objtypes[i].name);
	}
	fputs("	}\n"
	      "	return ret;\n"
	      "}\n\n", c_code);

	fputs("#include \"newifc_nongen.c\"\n\n", c_code);

	fputs("static NI_err\n"
	      "NI_copy_globals(NewIfcObj dst, NewIfcObj src)\n"
	      "{\n", c_code);
	nitems = sizeof(attributes) / sizeof(attributes[0]);
	for (i = 0; i < nitems; i++) {
		if (attributes[i].impl == attr_impl_global || attributes[i].impl == attr_impl_global_custom_setter) {
			if (attributes[i].inherit)
				fprintf(c_code,"	dst->%s = src->%s;\n", member_name(i), member_name(i));
		}
	}
	fputs("	return NewIfc_error_none;\n"
	      "}\n\n", c_code);

	nitems = sizeof(type_str) / sizeof(type_str[0]);
	for (i = 0; i < nitems; i++) {
		fprintf(c_code, "static NI_err\n"
		                "call_%s_change_handlers(NewIfcObj obj, enum NewIfc_handler_type type, const %s newval)\n"
		                "{\n"
		                "	if (obj->handlers == NULL)\n"
		                "		return NewIfc_error_none;\n"
		                "	struct NewIfc_handler **head = bsearch(&type, obj->handlers, obj->handlers_sz, sizeof(obj->handlers[0]), handler_bsearch_compar);\n"
		                "	if (head == NULL)\n"
		                "		return NewIfc_error_none;\n"
		                "	struct NewIfc_handler *h = *head;\n"
		                "	while (h != NULL) {\n"
		                "		NI_err ret = h->on_%s_change(obj, newval, h->cbdata);\n"
		                "		if (ret != NewIfc_error_none)\n"
		                "			return ret;\n"
		                "		h = h->next;\n"
		                "	}\n"
		                "	return NewIfc_error_none;\n"
		                "}\n\n", type_str[i].var_name, type_str[i].type, type_str[i].var_name);
	}

	nitems = sizeof(extra_handlers) / sizeof(extra_handlers[0]);
	for (i = 0; i < nitems; i++) {
		fprintf(c_code, "static NI_err\n"
		                "call_%s_handlers(NewIfcObj obj, enum NewIfc_handler_type type%s)\n"
		                "{\n"
		                "	if (obj->handlers == NULL)\n"
		                "		return NewIfc_error_none;\n"
		                "	struct NewIfc_handler **head = bsearch(&type, obj->handlers, obj->handlers_sz, sizeof(obj->handlers[0]), handler_bsearch_compar);\n"
		                "	if (head == NULL)\n"
		                "		return NewIfc_error_none;\n"
		                "	struct NewIfc_handler *h = *head;\n"
		                "	while (h != NULL) {\n"
		                "		NI_err ret = h->%s(obj%s, h->cbdata);\n"
		                "		if (ret != NewIfc_error_none)\n"
		                "			return ret;\n"
		                "		h = h->next;\n"
		                "	}\n"
		                "	return NewIfc_error_none;\n"
		                "}\n\n", extra_handlers[i].name, extra_handlers[i].args ? extra_handlers[i].args : "", extra_handlers[i].name, extra_handlers[i].params ? extra_handlers[i].params : "");
	}

	nitems = sizeof(attributes) / sizeof(attributes[0]);
	for (i = 0; i < nitems; i++) {
		attribute_functions(i, c_code, attributes[i].name);
	}
	nitems = sizeof(aliases) / sizeof(aliases[0]);
	for (i = 0; i < nitems; i++) {
		size_t a = find_attribute(aliases[i].attribute_name);
		attribute_functions(a, c_code, aliases[i].alias_name);
	}

	fputs("#include \"newifc_nongen_after.c\"\n\n", c_code);
	nitems = sizeof(objtypes) / sizeof(objtypes[0]);
	for (i = 0; i < nitems; i++) {
		fprintf(c_code, "#include \"%s.c\"\n", objtypes[i].name);
	}
	fputs("\n", c_code);

	fclose(c_code);
}
