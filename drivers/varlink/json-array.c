#include <linux/slab.h>

#include "json-array.h"
#include "json-object.h"

struct json_array {
	unsigned int refcount;
	enum json_value_type element_type;

	union json_value *elements;
	unsigned int n_elements;
	unsigned int n_allocated_elements;

	bool writable;
};

static int array_append(struct json_array *array, union json_value **valuep)
{
	union json_value *v;

	if (array->n_elements == array->n_allocated_elements) {
		array->n_allocated_elements = max(array->n_allocated_elements * 2, 8U);
		array->elements = krealloc(array->elements,
					   array->n_allocated_elements * sizeof(union json_value),
					   GFP_KERNEL);
		if (!array->elements)
			return -ENOMEM;
	}

	v = &array->elements[array->n_elements];
	array->n_elements++;

	*valuep = v;
	return 0;
}

enum json_value_type json_array_get_element_type(struct json_array *array)
{
	return array->element_type;
}

int json_array_new(struct json_array **arrayp)
{
	struct json_array *array;

	array = kzalloc(sizeof(struct json_array), GFP_KERNEL);
	if (!array)
		return -ENOMEM;

	array->refcount = 1;
	array->writable = true;

	*arrayp = array;
	return 0;
}
EXPORT_SYMBOL(json_array_new);

int json_array_new_from_scanner(struct json_array **arrayp,
				struct scanner *scanner)
{
	struct json_array *array = NULL;
	bool first = true;
	int r;

	r = json_array_new(&array);
	if (r < 0)
		return r;

	if (scanner_read_operator(scanner, "[") < 0) {
		r = -EINVAL;
		goto out;
	}

	while (scanner_peek(scanner) != ']') {
		enum json_value_type type;
		union json_value *value;

		if (!first && scanner_read_operator(scanner, ",") < 0) {
			r = -EINVAL;
			goto out;
		}

		r = array_append(array, &value);
		if (r < 0)
			goto out;

		r = json_value_read_from_scanner(&type, value, scanner);
		if (r < 0)
			goto out;

		if (first) {
			array->element_type = type;
		} else if (type != array->element_type) {
			r = -EDOM;
			goto out;
		}

		first = false;
	}

	if (scanner_read_operator(scanner, "]") < 0) {
		r = -EINVAL;
		goto out;
	}

	*arrayp = array;
	array = NULL;

out:
	json_array_unref(array);
	return r;
}

struct json_array *json_array_ref(struct json_array *array)
{
	array->refcount++;
	return array;
}
EXPORT_SYMBOL(json_array_ref);

struct json_array *json_array_unref(struct json_array *array)
{
	if (!array)
		return NULL;

	array->refcount--;

	if (array->refcount == 0) {
		unsigned int i;

		for (i = 0; i < array->n_elements; i++)
			json_value_clear(array->element_type, &array->elements[i]);

		kfree(array->elements);
		kfree(array);
	}

	return NULL;
}
EXPORT_SYMBOL(json_array_unref);

unsigned int json_array_get_n_elements(struct json_array *array)
{
	return array->n_elements;
}
EXPORT_SYMBOL(json_array_get_n_elements);

int json_array_get_bool(struct json_array *array, unsigned int index,
			bool *bp)
{
	if (index >= array->n_elements)
		return -EBADSLT;

	if (array->element_type != JSON_TYPE_BOOL)
		return -EDOM;

	*bp = array->elements[index].b;

	return 0;
}
EXPORT_SYMBOL(json_array_get_bool);

int json_array_get_int(struct json_array *array, unsigned int index,
		       long long *ip)
{
	if (index >= array->n_elements)
		return -EBADSLT;

	if (array->element_type != JSON_TYPE_INT)
		return -EDOM;

	*ip = array->elements[index].i;

	return 0;
}
EXPORT_SYMBOL(json_array_get_int);

int json_array_get_string(struct json_array *array, unsigned int index,
			  const char **stringp)
{
	if (index >= array->n_elements)
		return -EBADSLT;

	if (array->element_type != JSON_TYPE_STRING)
		return -EDOM;

	*stringp = array->elements[index].s;

	return 0;
}
EXPORT_SYMBOL(json_array_get_string);

int json_array_get_array(struct json_array *array, unsigned int index,
			 struct json_array **elementp)
{
	if (index >= array->n_elements)
		return -EBADSLT;

	if (array->element_type != JSON_TYPE_ARRAY)
		return -EDOM;

	*elementp = array->elements[index].array;

	return 0;
}
EXPORT_SYMBOL(json_array_get_array);

int json_array_get_object(struct json_array *array, unsigned int index,
			  struct json_object **objectp)
{
	if (index >= array->n_elements)
		return -EBADSLT;

	if (array->element_type != JSON_TYPE_OBJECT)
		return -EDOM;

	*objectp = array->elements[index].object;

	return 0;
}
EXPORT_SYMBOL(json_array_get_object);

int json_array_get_value(struct json_array *array, unsigned int index,
			 union json_value **valuep)
{
	if (index >= array->n_elements)
		return -EBADSLT;

	*valuep = &array->elements[index];

	return 0;
}

int json_array_append_bool(struct json_array *array, bool b)
{
	union json_value *v;
	int r;

	if (!array->writable)
		return -EROFS;

	if (array->n_elements == 0)
		array->element_type = JSON_TYPE_BOOL;
	else if (array->element_type != JSON_TYPE_BOOL)
		return -EDOM;

	r = array_append(array, &v);
	if (r < 0)
		return r;

	v->b = b;

	return 0;
}
EXPORT_SYMBOL(json_array_append_bool);

int json_array_append_int(struct json_array *array, long long i)
{
	union json_value *v;
	int r;

	if (!array->writable)
		return -EROFS;

	if (array->n_elements == 0)
		array->element_type = JSON_TYPE_INT;
	else if (array->element_type != JSON_TYPE_INT)
		return -EDOM;

	r = array_append(array, &v);
	if (r < 0)
		return r;

	v->i = i;

	return 0;
}
EXPORT_SYMBOL(json_array_append_int);

int json_array_append_string(struct json_array *array, const char *string)
{
	union json_value *v;
	int r;

	if (!array->writable)
		return -EROFS;

	if (array->n_elements == 0)
		array->element_type = JSON_TYPE_STRING;
	else if (array->element_type != JSON_TYPE_STRING)
		return -EDOM;

	r = array_append(array, &v);
	if (r < 0)
		return r;

	v->s = kstrdup(string, GFP_KERNEL);

	return 0;
}
EXPORT_SYMBOL(json_array_append_string);

int json_array_append_array(struct json_array *array,
			    struct json_array *element)
{
	union json_value *v;
	int r;

	if (!array->writable)
		return -EROFS;

	if (array->n_elements == 0)
		array->element_type = JSON_TYPE_ARRAY;
	else if (array->element_type != JSON_TYPE_ARRAY)
		return -EDOM;

	r = array_append(array, &v);
	if (r < 0)
		return r;

	v->array = json_array_ref(element);

	return 0;
}
EXPORT_SYMBOL(json_array_append_array);

int json_array_append_object(struct json_array *array,
			     struct json_object *object)
{
	union json_value *v;
	int r;

	if (!array->writable)
		return -EROFS;

	if (array->n_elements == 0)
		array->element_type = JSON_TYPE_OBJECT;
	else if (array->element_type != JSON_TYPE_OBJECT)
		return -EDOM;

	r = array_append(array, &v);
	if (r < 0)
		return r;

	v->object = json_object_ref(object);

	return 0;
}
EXPORT_SYMBOL(json_array_append_object);

int json_array_write_to_buffer(struct json_array *array, struct buffer *buffer)
{
	unsigned int i;
	int r;

	if (array->n_elements == 0) {
		r = buffer_printf(buffer, "[]");
		if (r < 0)
			return r;

		return 0;
	}

	r = buffer_printf(buffer, "[");
	if (r < 0)
		return r;

	for (i = 0; i < array->n_elements; i++) {
		if (i > 0) {
			r = buffer_printf(buffer, ",");
			if (r < 0)
				return r;
		}

		r = json_value_write_to_buffer(array->element_type, &array->elements[i],
					       buffer);
		if (r < 0)
			return r;
	}

	return buffer_printf(buffer, "]");
}
