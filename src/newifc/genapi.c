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
	{"child_height", "uint16_t", attr_impl_global, 1},
	{"child_width", "uint16_t", attr_impl_global, 1},
	{"child_xpos", "uint16_t", attr_impl_global, 1},
	{"child_ypos", "uint16_t", attr_impl_global, 1},
	{"dirty", "bool", attr_impl_root, 1},
	{"height", "uint16_t", attr_impl_global, 0},
	{"locked", "bool", attr_impl_root, 0},
	{"locked_by_me", "bool", attr_impl_root, 1},
	{"show_help", "bool", attr_impl_object, 0},
	{"show_title", "bool", attr_impl_object, 0},
	{"title", "const char *", attr_impl_object, 0},
	{"transparent", "bool", attr_impl_object, 0},
	{"width", "uint16_t", attr_impl_global, 0},
	{"xpos", "uint16_t", attr_impl_global, 0},
	{"ypos", "uint16_t", attr_impl_global, 0},
};

struct error_info {
	const char *name;
};

const struct error_info
error_inf[] = {
	{"error_none"},
	{"error_allocation_failure"},
	{"error_out_of_range"},
	{"error_not_implemented"},
	{"error_lock_failed"},
	{"error_needs_parent"},
	{"error_wont_fit"},
};

int
main(int argc, char **argv)
{
	FILE *header;
	FILE *internal_header;
	FILE *c_code;
	size_t nitems;
	size_t i;

	header = fopen("newifc.h", "wb");
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

	fputs("enum NewIfc_error {\n", header);
	nitems = sizeof(error_inf) / sizeof(error_inf[0]);
	for (i = 0; i < nitems; i++) {
		fprintf(header, "\tNewIfc_%s,\n", error_inf[i].name);
	}
	fputs("};\n\n", header);

	fputs("enum NewIfc_object {\n", header);
	nitems = sizeof(objtypes) / sizeof(objtypes[0]);
	for (i = 0; i < nitems; i++) {
		fprintf(header, "\tNewIfc_%s,\n", objtypes[i].name);
	}
	fputs("};\n\n", header);

	fputs("NewIfcObj NI_copy(NewIfcObj obj);\n", header);
	fputs("NewIfcObj NI_create(enum NewIfc_object obj, NewIfcObj parent);\n", header);
	fputs("enum NewIfc_error NI_error(NewIfcObj obj);\n", header);
	fputs("bool NI_walk_children(NewIfcObj obj, bool (*cb)(NewIfcObj obj, void *cb_data), void *cbdata);\n\n", header);

	nitems = sizeof(attributes) / sizeof(attributes[0]);
	for (i = 0; i < nitems; i++) {
		if (!attributes[i].read_only)
			fprintf(header, "bool NI_set_%s(NewIfcObj obj, %s value);\n", attributes[i].name, attributes[i].type);
		fprintf(header, "bool NI_get_%s(NewIfcObj obj, %s* value);\n", attributes[i].name, attributes[i].type);
	}
	fputs("\n#endif\n", header);
	fclose(header);


	internal_header = fopen("newifc_internal.h", "wb");
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
	      "\tbool (*set)(NewIfcObj niobj, const int attr, ...);\n"
	      "\tbool (*get)(NewIfcObj niobj, const int attr, ...);\n"
	      "\tNewIfcObj (*copy)(NewIfcObj obj);\n"
	      "\tNewIfcObj root;\n"
	      "\tNewIfcObj parent;\n"
	      "\tNewIfcObj higherpeer;\n"
	      "\tNewIfcObj lowerpeer;\n"
	      "\tNewIfcObj topchild;\n"
	      "\tNewIfcObj bottomchild;\n"
	      "\tenum NewIfc_object type;\n"
	      "\tenum NewIfc_error last_error;\n"
	      "\tuint16_t height;\n"
	      "\tuint16_t width;\n"
	      "\tuint16_t xpos;\n"
	      "\tuint16_t ypos;\n"
	      "\tuint16_t child_height;\n"
	      "\tuint16_t child_width;\n"
	      "\tuint16_t child_xpos;\n"
	      "\tuint16_t child_ypos;\n"
	      "};\n\n", internal_header);

	fputs("enum NewIfc_attribute {\n", internal_header);
	nitems = sizeof(attributes) / sizeof(attributes[0]);
	for (i = 0; i < nitems; i++) {
		fprintf(internal_header, "\tNewIfc_%s,\n", attributes[i].name);
	}
	fputs("};\n\n", internal_header);

	fputs("#endif\n", internal_header);
	fclose(internal_header);


	c_code = fopen("newifc.c", "wb");
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

	fputs("NewIfcObj\n"
	      "NI_create(enum NewIfc_object obj, NewIfcObj parent) {\n"
	      "\tNewIfcObj ret = NULL;\n"
	      "\n"
	      "\tswitch(obj) {\n", c_code);
	nitems = sizeof(objtypes) / sizeof(objtypes[0]);
	for (i = 0; i < nitems; i++) {
		fprintf(c_code, "\t\tcase NewIfc_%s:\n", objtypes[i].name);
		if (objtypes[i].needs_parent) {
			fputs("\t\t\tif (parent == NULL) {\n"
			                "\t\t\t\tbreak;\n"
			                "\t\t\t}\n", c_code);
		}
		else {
			fputs("\t\t\tif (parent != NULL) {\n"
			                "\t\t\t\tbreak;\n"
			                "\t\t\t}\n", c_code);
		}
		fprintf(c_code, "\t\t\tret = NewIFC_%s();\n"
		                "\t\t\tif (ret == NULL) {\n"
		                "\t\t\t\tbreak;\n"
		                "\t\t\t}\n"
		                "\t\t\tret->type = obj;\n"
		                "\t\t\tbreak;\n", objtypes[i].name);
	}
	fputs("\t}\n"
	      "\tif (ret) {\n"
	      "\t\tret->parent = parent;\n"
	      "\t\tret->higherpeer = NULL;\n"
	      "\t\tret->topchild = NULL;\n"
	      "\t\tret->bottomchild = NULL;\n"
	      "\t\tif (parent) {\n"
	      "\t\t\tret->root = parent->root;\n"
	      "\t\t\tret->lowerpeer = parent->topchild;\n"
	      "\t\t\tparent->topchild = ret;\n"
	      "\t\t\tif (parent->bottomchild == NULL) {\n"
	      "\t\t\t\tparent->bottomchild = ret;\n"
	      "\t\t\t}\n"
	      "\t\t}\n"
	      "\t\telse {\n"
	      "\t\t\tret->root = ret;\n"
	      "\t\t\tret->lowerpeer = NULL;\n"
	      "\t\t}\n"
	      "\t}\n"
	      "\treturn ret;\n"
	      "};\n\n", c_code);

	fputs("#include \"newifc_nongen.c\"\n\n", c_code);

	nitems = sizeof(attributes) / sizeof(attributes[0]);
	for (i = 0; i < nitems; i++) {
		switch (attributes[i].impl) {
			case attr_impl_object:
				if (!attributes[i].read_only) {
					fprintf(c_code, "bool\n"
							"NI_set_%s(NewIfcObj obj, %s value) {\n"
							"\tbool ret;\n"
							"\tif (NI_set_locked(obj, true)) {\n"
							"\t\tret = obj->set(obj, NewIfc_%s, value);\n"
							"\t\tNI_set_locked(obj, false);\n"
							"\t}\n"
							"\telse\n"
							"\t\tret = false;\n"
							"\treturn ret;\n"
							"}\n\n", attributes[i].name, attributes[i].type, attributes[i].name);
				}

				fprintf(c_code, "bool\n"
						"NI_get_%s(NewIfcObj obj, %s* value) {\n"
						"\tbool ret;\n"
						"\tif (NI_set_locked(obj, true)) {\n"
						"\t\tret = obj->get(obj, NewIfc_%s, value);\n"
						"\t\tNI_set_locked(obj, false);\n"
						"\t}\n"
						"\telse\n"
						"\t\tret = false;\n"
						"\treturn ret;\n"
						"}\n\n", attributes[i].name, attributes[i].type, attributes[i].name);
				break;
			case attr_impl_global:
				if (!attributes[i].read_only) {
					fprintf(c_code, "bool\n"
							"NI_set_%s(NewIfcObj obj, %s value) {\n"
							"\tbool ret;\n"
							"\tif (NI_set_locked(obj, true)) {\n"
							"\t\tif ((!obj->set(obj, NewIfc_%s, value)) && obj->last_error != NewIfc_error_not_implemented) {\n"
							"\t\t\tobj->%s = value;\n"
							"\t\t\tobj->last_error = NewIfc_error_none;\n"
							"\t\t}\n"
							"\t\tNI_set_locked(obj, false);\n"
							"\t}\n"
							"\telse\n"
							"\t\tret = false;\n"
							"\treturn ret;\n"
							"}\n\n", attributes[i].name, attributes[i].type, attributes[i].name, attributes[i].name);
				}

				fprintf(c_code, "bool\n"
				                "NI_get_%s(NewIfcObj obj, %s* value) {\n"
				                "\tbool ret;\n"
				                "\tif (NI_set_locked(obj, true)) {\n"
						"\t\tif ((!obj->get(obj, NewIfc_%s, value)) && obj->last_error != NewIfc_error_not_implemented) {\n"
						"\t\t\t*value = obj->%s;\n"
						"\t\t\tobj->last_error = NewIfc_error_none;\n"
						"\t\t}\n"
						"\t\tNI_set_locked(obj, false);\n"
						"\t}\n"
						"\telse\n"
						"\t\tret = false;\n"
						"\treturn ret;\n"
						"}\n\n", attributes[i].name, attributes[i].type, attributes[i].name, attributes[i].name);
				break;
			case attr_impl_root:
				if (!attributes[i].read_only) {
					fprintf(c_code, "bool\n"
							"NI_set_%s(NewIfcObj obj, %s value) {\n"
							"\tbool ret = obj->root->set(obj, NewIfc_%s, value);\n"
							"\tobj->last_error = obj->root->last_error;\n"
							"\treturn ret;\n"
							"}\n\n", attributes[i].name, attributes[i].type, attributes[i].name);
				}

				fprintf(c_code, "bool\n"
						"NI_get_%s(NewIfcObj obj, %s* value) {\n"
						"\tbool ret = obj->root->get(obj, NewIfc_%s, value);\n"
						"\tobj->last_error = obj->root->last_error;\n"
						"\treturn ret;\n"
						"}\n\n", attributes[i].name, attributes[i].type, attributes[i].name);
				break;
		}
	}
	fclose(c_code);
}
