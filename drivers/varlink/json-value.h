#ifndef _JSON_VALUE_H_
#define _JSON_VALUE_H_

#include <linux/json.h>

#include "buffer.h"
#include "json-object.h"
#include "scanner.h"

enum json_value_type {
	JSON_TYPE_ARRAY,
	JSON_TYPE_BOOL,
	JSON_TYPE_INT,
	JSON_TYPE_STRING,
	JSON_TYPE_OBJECT
};

union json_value {
	struct json_array *array;
	bool b;
	long long i;
	char *s;
	struct json_object *object;
};

int json_value_read_from_scanner(enum json_value_type *typep,
				 union json_value *value, struct scanner *scanner);
int json_value_write_to_buffer(enum json_value_type, union json_value *value,
			       struct buffer *buffer);
void json_value_clear(enum json_value_type type, union json_value *value);
#endif
