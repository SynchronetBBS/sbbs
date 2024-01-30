#ifndef INTERNAL_MACROS_H
#define INTERNAL_MACROS_H

#define SET_VARS           \
	va_list ap;         \
	char *buf; \

#define SET_BOOL(st, field) do {    \
	st->field = va_arg(ap, int); \
} while(0)

#define SET_UINT32_T(st, field) do {     \
	st->field = va_arg(ap, uint32_t); \
} while(0);

#define SET_UINT16_T(st, field) do {     \
	st->field = va_arg(ap, uint16_t); \
} while(0);

#define SET_STRING(st, field) do {                                 \
	buf = strdup(va_arg(ap, const char *));                     \
	if (buf == NULL) {                                           \
		st->api.last_error = NewIfc_error_allocation_failure; \
		break;                                                 \
	}                                                               \
	free(st->field);                                                 \
	st->field = buf;                                                  \
} while(0)


#define GET_VARS   \
	va_list ap; \

#define GET_BOOL(st, field)     *(va_arg(ap, bool *))     = st->field
#define GET_UINT32_T(st, field) *(va_arg(ap, uint32_t *)) = st->field;
#define GET_UINT16_T(st, field) *(va_arg(ap, uint16_t *)) = st->field;
#define GET_STRING(st, field)   *(va_arg(ap, char **))     = st->field;

#endif
