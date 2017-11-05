#include <linux/slab.h>

#include "json-array.h"
#include "json-object.h"
#include "json-value.h"

void json_value_clear(enum json_value_type type, union json_value *value)
{
	switch (type) {
	case JSON_TYPE_BOOL:
	case JSON_TYPE_INT:
		break;

	case JSON_TYPE_STRING:
		kfree(value->s);
		break;

	case JSON_TYPE_ARRAY:
		if (value->array)
			json_array_unref(value->array);
		break;

	case JSON_TYPE_OBJECT:
		if (value->object)
			json_object_unref(value->object);
		break;
	}
}

int json_value_read_from_scanner(enum json_value_type *typep,
				 union json_value *value, struct scanner *scanner)
{
	long long number;
	int r;

	if (scanner_peek(scanner) == '{') {
		r = json_object_new_from_scanner(&value->object, scanner);
		if (r < 0)
			return r;

		*typep = JSON_TYPE_OBJECT;

	} else if (scanner_peek(scanner) == '[') {
		r = json_array_new_from_scanner(&value->array, scanner);
		if (r < 0)
			return r;

		*typep = JSON_TYPE_ARRAY;

	} else if (scanner_read_keyword(scanner, "true") >= 0) {
		value->b = true;
		*typep = JSON_TYPE_BOOL;

	} else if (scanner_read_keyword(scanner, "false") >= 0) {
		value->b = false;
		*typep = JSON_TYPE_BOOL;

	} else if (scanner_peek(scanner) == '"') {
		r = scanner_read_string(scanner, &value->s);
		if (r < 0)
			return r;

		*typep = JSON_TYPE_STRING;

	} else if (scanner_read_number(scanner, &number)) {
		value->i = number;
		*typep = JSON_TYPE_INT;

	} else
		return -EINVAL;

	return 0;
}

static int json_write_string(struct buffer *buffer, const char *s)
{
	int r;

	while (*s != '\0') {
		switch(*s) {
		case '\"':
			r = buffer_printf(buffer, "\\\"");
			if (r < 0)
				return r;
			break;

		case '\\':
			r = buffer_printf(buffer, "\\\\");
			if (r < 0)
				return r;
			break;

		case '\b':
			r = buffer_printf(buffer, "\\b");
			if (r < 0)
				return r;
			break;

		case '\f':
			r = buffer_printf(buffer, "\\f");
			if (r < 0)
				return r;
			break;

		case '\n':
			r = buffer_printf(buffer, "\\n");
			if (r < 0)
				return r;
			break;

		case '\r':
			r = buffer_printf(buffer, "\\r");
			if (r < 0)
				return r;
			break;

		case '\t':
			r = buffer_printf(buffer, "\\t");
			if (r < 0)
				return r;
			break;

		default:
			if (*(unsigned char *)s < 0x20)
				r = buffer_printf(buffer, "\\u%04x", *s);
			else
				r = buffer_printf(buffer, "%c", *s);
			if (r < 0)
				return r;
		}

		s++;
	}

	return 0;
}

int json_value_write_to_buffer(enum json_value_type type,
			       union json_value *value,
			       struct buffer *buffer)
{
	int r;

	switch (type) {
	case JSON_TYPE_BOOL:
		r = buffer_printf(buffer, "%s", value->b ? "true" : "false");
		if (r < 0)
			return r;
		break;

	case JSON_TYPE_INT:
		r = buffer_printf(buffer, "%lld", value->i);
		if (r < 0)
			return r;
		break;

	case JSON_TYPE_STRING:
		r = buffer_printf(buffer, "\"");
		if (r < 0)
			return r;

		r = json_write_string(buffer, value->s);
		if (r < 0)
			return r;

		r = buffer_printf(buffer, "\"");
		if (r < 0)
			return r;
		break;

	case JSON_TYPE_ARRAY:
		r = json_array_write_to_buffer(value->array, buffer);
		if (r < 0)
			return r;
		break;

	case JSON_TYPE_OBJECT:
		r = json_object_write_to_buffer(value->object, buffer);
		if (r < 0)
			return r;
		break;
	}

	return 0;
}
