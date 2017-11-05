#ifndef _JSON_ARRAY_H_
#define _JSON_ARRAY_H_

#include <linux/json.h>

#include "buffer.h"
#include "json-value.h"
#include "scanner.h"

int json_array_new_from_scanner(struct json_array **arrayp,
				struct scanner *scanner);
int json_array_get_value(struct json_array *array, unsigned int index,
			 union json_value **valuep);
enum json_value_type json_array_get_element_type(struct json_array *array);
int json_array_write_to_buffer(struct json_array *array, struct buffer *buffer);
#endif
