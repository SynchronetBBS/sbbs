#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

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
};

enum attribute_impl {
	attr_impl_global,
	attr_impl_object,
	attr_impl_root,
};

struct attribute_info {
	const char *name;
	const char *type;
	enum attribute_impl impl;
	int read_only;
};

const struct attribute_info
attributes[] = {
	{"bottomchild", "NewIfcObj", attr_impl_global, 1},
	{"child_height", "uint16_t", attr_impl_global, 1},
	{"child_width", "uint16_t", attr_impl_global, 1},
	{"child_xpos", "uint16_t", attr_impl_global, 1},
	{"child_ypos", "uint16_t", attr_impl_global, 1},
	{"dirty", "bool", attr_impl_root, 1},
	{"height", "uint16_t", attr_impl_global, 0},
	{"higherpeer", "NewIfcObj", attr_impl_global, 1},
	{"last_error", "NI_err", attr_impl_global, 1},
	{"locked", "bool", attr_impl_root, 0},
	{"locked_by_me", "bool", attr_impl_root, 1},
	{"lowerpeer", "NewIfcObj", attr_impl_global, 1},
	{"min_height", "uint16_t", attr_impl_global, 1},
	{"min_width", "uint16_t", attr_impl_global, 1},
	{"parent", "NewIfcObj", attr_impl_global, 1},
	{"root", "NewIfcObj", attr_impl_global, 1},
	{"show_help", "bool", attr_impl_object, 0},
	{"show_title", "bool", attr_impl_object, 0},
	{"title", "const char *", attr_impl_object, 0},
	{"topchild", "NewIfcObj", attr_impl_global, 1},
	{"type", "enum NewIfc_object", attr_impl_global, 1},
	{"width", "uint16_t", attr_impl_global, 0},
	{"xpos", "uint16_t", attr_impl_global, 0},
	{"ypos", "uint16_t", attr_impl_global, 0},
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
};

int
main(int argc, char **argv)
{
	FILE *header;
	FILE *internal_header;
	FILE *c_code;
	size_t nitems;
	size_t i;

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

	fputs("NI_err NI_copy(NewIfcObj obj, NewIfcObj *newobj);\n", header);
	fputs("NI_err NI_create(enum NewIfc_object obj, NewIfcObj parent, NewIfcObj *newobj);\n", header);
	fputs("NI_err NI_error(NewIfcObj obj);\n", header);
	fputs("NI_err NI_walk_children(NewIfcObj obj, NI_err (*cb)(NewIfcObj obj, void *cb_data), void *cbdata);\n\n", header);

	nitems = sizeof(attributes) / sizeof(attributes[0]);
	for (i = 0; i < nitems; i++) {
		if (!attributes[i].read_only)
			fprintf(header, "NI_err NI_set_%s(NewIfcObj obj, %s value);\n", attributes[i].name, attributes[i].type);
		fprintf(header, "NI_err NI_get_%s(NewIfcObj obj, %s* value);\n", attributes[i].name, attributes[i].type);
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
	      "#define NEWIFC_INTERNAL_H\n"
	      "\n"
	      "struct newifc_api {\n"
	      "	NI_err (*set)(NewIfcObj niobj, const int attr, ...);\n"
	      "	NI_err (*get)(NewIfcObj niobj, const int attr, ...);\n"
	      "	NI_err (*copy)(NewIfcObj obj, NewIfcObj *newobj);\n"
	      "	NewIfcObj root;\n"
	      "	NewIfcObj parent;\n"
	      "	NewIfcObj higherpeer;\n"
	      "	NewIfcObj lowerpeer;\n"
	      "	NewIfcObj topchild;\n"
	      "	NewIfcObj bottomchild;\n"
	      "	enum NewIfc_object type;\n"
	      "	NI_err last_error;\n"
	      "	uint16_t height;\n"
	      "	uint16_t width;\n"
	      "	uint16_t min_height;\n"
	      "	uint16_t min_width;\n"
	      "	uint16_t xpos;\n"
	      "	uint16_t ypos;\n"
	      "	uint16_t child_height;\n"
	      "	uint16_t child_width;\n"
	      "	uint16_t child_xpos;\n"
	      "	uint16_t child_ypos;\n"
	      "};\n\n", internal_header);

	fputs("enum NewIfc_attribute {\n", internal_header);
	nitems = sizeof(attributes) / sizeof(attributes[0]);
	for (i = 0; i < nitems; i++) {
		fprintf(internal_header, "	NewIfc_%s,\n", attributes[i].name);
	}
	fputs("};\n\n", internal_header);

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
	fputs("#include \"internal_macros.h\"\n", c_code);
	nitems = sizeof(objtypes) / sizeof(objtypes[0]);
	for (i = 0; i < nitems; i++) {
		fprintf(c_code, "#include \"%s.c\"\n", objtypes[i].name);
	}
	fputs("\n", c_code);

	fputs("NI_err\n"
	      "NI_create(enum NewIfc_object obj, NewIfcObj parent, NewIfcObj *newobj) {\n"
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
		fprintf(c_code, "			ret = NewIFC_%s(newobj);\n"
		                "			if (ret != NewIfc_error_none) {\n"
		                "				break;\n"
		                "			}\n"
		                "			(*newobj)->type = obj;\n"
		                "			break;\n", objtypes[i].name);
	}
	fputs("	}\n"
	      "	if (ret == NewIfc_error_none) {\n"
	      "		(*newobj)->parent = parent;\n"
	      "		(*newobj)->higherpeer = NULL;\n"
	      "		(*newobj)->topchild = NULL;\n"
	      "		(*newobj)->bottomchild = NULL;\n"
	      "		if (parent) {\n"
	      "			(*newobj)->root = parent->root;\n"
	      "			(*newobj)->lowerpeer = parent->topchild;\n"
	      "			parent->topchild = *newobj;\n"
	      "			if (parent->bottomchild == NULL) {\n"
	      "				parent->bottomchild = *newobj;\n"
	      "			}\n"
	      "		}\n"
	      "		else {\n"
	      "			(*newobj)->root = *newobj;\n"
	      "			(*newobj)->lowerpeer = NULL;\n"
	      "		}\n"
	      "	}\n"
	      "	return ret;\n"
	      "}\n\n", c_code);

	fputs("#include \"newifc_nongen.c\"\n\n", c_code);

	nitems = sizeof(attributes) / sizeof(attributes[0]);
	for (i = 0; i < nitems; i++) {
		switch (attributes[i].impl) {
			case attr_impl_object:
				if (!attributes[i].read_only) {
					fprintf(c_code, "NI_err\n"
					                "NI_set_%s(NewIfcObj obj, %s value) {\n"
					                "	NI_err ret;\n"
					                "	if (obj == NULL)\n"
					                "		return NewIfc_error_invalid_arg;\n"
					                "	if (NI_set_locked(obj, true)) {\n"
					                "		ret = obj->set(obj, NewIfc_%s, value);\n"
					                "		NI_set_locked(obj, false);\n"
					                "	}\n"
					                "	else\n"
					                "		ret = NewIfc_error_lock_failed;\n"
					                "	return ret;\n"
					                "}\n\n", attributes[i].name, attributes[i].type, attributes[i].name);
				}

				fprintf(c_code, "NI_err\n"
				                "NI_get_%s(NewIfcObj obj, %s* value) {\n"
				                "	NI_err ret;\n"
				                "	if (obj == NULL)\n"
				                "		return NewIfc_error_invalid_arg;\n"
				                "	if (value == NULL)\n"
				                "		return NewIfc_error_invalid_arg;\n"
				                "	if (NI_set_locked(obj, true)) {\n"
				                "		ret = obj->get(obj, NewIfc_%s, value);\n"
				                "		NI_set_locked(obj, false);\n"
				                "	}\n"
				                "	else\n"
				                "		ret = NewIfc_error_lock_failed;\n"
				                "	return ret;\n"
				                "}\n\n", attributes[i].name, attributes[i].type, attributes[i].name);
				break;
			case attr_impl_global:
				if (!attributes[i].read_only) {
					fprintf(c_code, "NI_err\n"
					                "NI_set_%s(NewIfcObj obj, %s value) {\n"
					                "	NI_err ret;\n"
					                "	if (obj == NULL)\n"
					                "		return NewIfc_error_invalid_arg;\n"
					                "	if (NI_set_locked(obj, true)) {\n"
					                "		ret = obj->set(obj, NewIfc_%s, value);\n"
					                "		if (ret != NewIfc_error_none && obj->last_error != NewIfc_error_not_implemented) {\n"
					                "			obj->%s = value;\n"
					                "			obj->last_error = NewIfc_error_none;\n"
					                "		}\n"
					                "		NI_set_locked(obj, false);\n"
					                "	}\n"
					                "	else\n"
					                "		ret = NewIfc_error_lock_failed;\n"
					                "	return ret;\n"
					                "}\n\n", attributes[i].name, attributes[i].type, attributes[i].name, attributes[i].name);
				}

				fprintf(c_code, "NI_err\n"
				                "NI_get_%s(NewIfcObj obj, %s* value) {\n"
				                "	NI_err ret;\n"
				                "	if (obj == NULL)\n"
				                "		return NewIfc_error_invalid_arg;\n"
				                "	if (value == NULL)\n"
				                "		return NewIfc_error_invalid_arg;\n"
				                "	if (NI_set_locked(obj, true)) {\n"
				                "		ret = obj->get(obj, NewIfc_%s, value);\n"
				                "		if ((ret != NewIfc_error_none) && obj->last_error != NewIfc_error_not_implemented) {\n"
				                "			*value = obj->%s;\n"
				                "			obj->last_error = NewIfc_error_none;\n"
				                "		}\n"
				                "		NI_set_locked(obj, false);\n"
				                "	}\n"
				                "	else\n"
				                "		ret = NewIfc_error_lock_failed;\n"
				                "	return ret;\n"
				                "}\n\n", attributes[i].name, attributes[i].type, attributes[i].name, attributes[i].name);
				break;
			case attr_impl_root:
				if (!attributes[i].read_only) {
					fprintf(c_code, "NI_err\n"
					                "NI_set_%s(NewIfcObj obj, %s value) {\n"
					                "	if (obj == NULL)\n"
					                "		return NewIfc_error_invalid_arg;\n"
					                "	NI_err ret = obj->root->set(obj, NewIfc_%s, value);\n"
					                "	obj->last_error = obj->root->last_error;\n"
					                "	return ret;\n"
					                "}\n\n", attributes[i].name, attributes[i].type, attributes[i].name);
				}

				fprintf(c_code, "NI_err\n"
				                "NI_get_%s(NewIfcObj obj, %s* value) {\n"
				                "	if (obj == NULL)\n"
				                "		return NewIfc_error_invalid_arg;\n"
				                "	if (value == NULL)\n"
				                "		return NewIfc_error_invalid_arg;\n"
				                "	NI_err ret = obj->root->get(obj, NewIfc_%s, value);\n"
				                "	obj->last_error = obj->root->last_error;\n"
				                "	return ret;\n"
				                "}\n\n", attributes[i].name, attributes[i].type, attributes[i].name);
				break;
		}
	}
	fclose(c_code);
}
