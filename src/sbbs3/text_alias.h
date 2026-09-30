/* Former text.dat string ID names, still accepted as aliases of the current names */

#ifndef TEXT_ALIAS_H_
#define TEXT_ALIAS_H_

static const struct {
	const char* old_name;
	const char* name;
} text_id_alias[] = {
	{ "NScanPmQ", "PM" },
	{ "NScanAmQ", "AM" },
};

#define TEXT_ID_ALIASES (sizeof text_id_alias / sizeof text_id_alias[0])

#endif /* Don't add anything after this line */
