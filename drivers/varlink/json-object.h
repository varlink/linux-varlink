#ifndef _JSON_OBJECT_H_
#define _JSON_OBJECT_H_

#include <linux/json.h>

#include "buffer.h"
#include "scanner.h"

int json_object_new_from_scanner(struct json_object **objectp,
				 struct scanner *scanner);
int json_object_write_json(struct json_object *object, struct buffer *buffer);
int json_object_write_to_buffer(struct json_object *object,
				struct buffer *buffer);
#endif
