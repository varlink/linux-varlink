#include <linux/bsearch.h>
#include <linux/slab.h>
#include <linux/sort.h>
#include <linux/string.h>

#include "buffer.h"
#include "json-array.h"
#include "json-object.h"
#include "scanner.h"

struct json_field {
	char *name;
	enum json_value_type type;
	union json_value value;
};

struct json_object {
	unsigned int refcount;

	struct json_field **fields;
	unsigned int n_fields;
	unsigned int n_fields_allocated;
	bool writable;
};

static int fields_compare(const void *p1, const void *p2)
{
	struct json_field *f1 = *(struct json_field **)p1;
	struct json_field *f2 = *(struct json_field **)p2;

	return strcmp(f1->name, f2->name);
}

static struct json_field *object_search_field(struct json_object *object,
					      const char *name)
{
	struct json_field _f = {
		.name = (char *)name,
	};
	struct json_field *f = &_f;
	struct json_field **fp;

	fp = bsearch(&f, object->fields, object->n_fields,
		     sizeof(struct json_field *), fields_compare);

	if (fp)
		return *fp;

	return NULL;
}

static struct json_field *field_free(struct json_field *field)
{
	if (!field)
		return NULL;

	kfree(field->name);
	json_value_clear(field->type, &field->value);
	kfree(field);

	return NULL;
}

static int object_insert(struct json_object *object,
			 const char *name,
			 struct json_field **fieldp)
{
	struct json_field *field;
	struct json_field **fp;
	int r = 0;

	field = kzalloc(sizeof(struct json_field), GFP_KERNEL);
	if (!field)
		return -ENOMEM;

	field->name = kstrdup(name, GFP_KERNEL);
	if (!field->name) {
		r = -ENOMEM;
		goto out;
	}

	/* Replace field */
	fp = bsearch(&field, object->fields, object->n_fields,
		     sizeof(struct json_field *), fields_compare);
	if (fp) {
		unsigned int index = fp - object->fields;

		field_free(object->fields[index]);
		object->fields[index] = field;
		*fieldp = field;

		return 0;
	}

	/* Insert field */
	if (object->n_fields == object->n_fields_allocated) {
		object->n_fields_allocated = max(object->n_fields_allocated * 2, 4U);
		object->fields = krealloc(object->fields,
					  object->n_fields_allocated * sizeof(void *),
					  GFP_KERNEL);
		if (!object->fields) {
			r = -ENOMEM;
			goto out;
		}
	}

	object->fields[object->n_fields++] = field;
	*fieldp = field;
	field = NULL;

	sort(object->fields, object->n_fields, sizeof(struct json_field *),
	     fields_compare, NULL);

out:
	field_free(field);
	return r;
}

int json_object_new(struct json_object **objectp)
{
	struct json_object *object = NULL;

	object = kzalloc(sizeof(struct json_object), GFP_KERNEL);
	if (!object)
		return -ENOMEM;

	object->refcount = 1;
	object->writable = true;

	*objectp = object;
	return 0;
}
EXPORT_SYMBOL(json_object_new);

int json_object_new_from_scanner(struct json_object **objectp,
				 struct scanner *scanner)
{
	struct json_object *object = NULL;
	bool first = true;
	int r;

	if (scanner_read_operator(scanner, "{") < 0)
		return -EINVAL;

	r = json_object_new(&object);
	if (r < 0)
		return r;

	while (scanner_peek(scanner) != '}') {
		char *name = NULL;

		if (!first && scanner_read_operator(scanner, ",") < 0) {
			r = -EINVAL;
			goto out;
		}

		r = scanner_read_string(scanner, &name);
		if (r < 0) {
			kfree(name);
			goto out;
		}

		if (scanner_read_operator(scanner, ":") < 0) {
			r = -EINVAL;
			kfree(name);
			goto out;
		}

		/* treat `null` the same as non-existent keys */
		if (scanner_read_keyword(scanner, "null") < 0) {
			struct json_field *field;

			r = object_insert(object, name, &field);
			if (r < 0) {
				kfree(name);
				goto out;
			}

			r = json_value_read_from_scanner(&field->type, &field->value, scanner);
			if (r < 0) {
				kfree(name);
				goto out;
			}
		}

		kfree(name);
		first = false;
	}

	if (scanner_read_operator(scanner, "}") < 0) {
		r = -EINVAL;
		goto out;
	}

	*objectp = object;
	object = NULL;

out:
	json_object_unref(object);
	return r;
}

int json_object_new_from_string(struct json_object **objectp,
				const char *string)
{
	struct json_object *object = NULL;
	struct scanner *scanner = NULL;
	int r;

	r = scanner_new(&scanner, string, false);
	if (r < 0)
		return r;

	r = json_object_new_from_scanner(&object, scanner);
	if (r < 0)
		goto out;

	if (scanner_peek(scanner) != '\0') {
		r = -EINVAL;
		goto out;
	}

	*objectp = object;
	object = NULL;

out:
	json_object_unref(object);
	scanner_free(scanner);
	return r;
}
EXPORT_SYMBOL(json_object_new_from_string);

struct json_object *json_object_ref(struct json_object *object)
{
	object->refcount++;
	return object;
}
EXPORT_SYMBOL(json_object_ref);

struct json_object *json_object_unref(struct json_object *object)
{
	if (!object)
		return NULL;

	object->refcount--;

	if (object->refcount == 0) {
		unsigned int i;

		for (i = 0; i < object->n_fields; i++)
			field_free(object->fields[i]);

		kfree(object->fields);
		kfree(object);
	}

	return NULL;
}
EXPORT_SYMBOL(json_object_unref);

unsigned int json_object_get_field_names(struct json_object *object,
					 const char ***namesp)
{
	if (namesp) {
		const char **names;
		unsigned int i;

		names = kzalloc((object->n_fields + 1) * sizeof(const char *),
				GFP_KERNEL);
		if (!names)
			return -ENOMEM;

		for (i = 0; i < object->n_fields; i++)
			names[i] = object->fields[i]->name;

		*namesp = names;
	}

	return object->n_fields;
}
EXPORT_SYMBOL(json_object_get_field_names);

int json_object_get_bool(struct json_object *object, const char *field_name,
			 bool *bp)
{
	struct json_field *field;

	field = object_search_field(object, field_name);
	if (!field)
		return -ENOENT;

	if (field->type != JSON_TYPE_BOOL)
		return -EDOM;

	*bp = field->value.b;

	return 0;
}
EXPORT_SYMBOL(json_object_get_bool);

int json_object_get_int(struct json_object *object, const char *field_name,
			long long *ip)
{
	struct json_field *field;

	field = object_search_field(object, field_name);
	if (!field)
		return -ENOENT;

	if (field->type != JSON_TYPE_INT)
		return -EDOM;

	*ip = field->value.i;

	return 0;
}
EXPORT_SYMBOL(json_object_get_int);

int json_object_get_string(struct json_object *object, const char *field_name,
			   const char **stringp)
{
	struct json_field *field;

	field = object_search_field(object, field_name);
	if (!field)
		return -ENOENT;

	if (field->type != JSON_TYPE_STRING)
		return -EDOM;

	*stringp = field->value.s;

	return 0;
}
EXPORT_SYMBOL(json_object_get_string);

int json_object_get_array(struct json_object *object, const char *field_name,
			  struct json_array **arrayp)
{
	struct json_field *field;

	field = object_search_field(object, field_name);
	if (!field)
		return -ENOENT;

	if (field->type != JSON_TYPE_ARRAY)
		return -EDOM;

	*arrayp = field->value.array;

	return 0;
}
EXPORT_SYMBOL(json_object_get_array);

int json_object_get_object(struct json_object *object, const char *field_name,
			   struct json_object **nestedp)
{
	struct json_field *field;

	field = object_search_field(object, field_name);
	if (!field)
		return -ENOENT;

	if (field->type != JSON_TYPE_OBJECT)
		return -EDOM;

	*nestedp = field->value.object;

	return 0;
}
EXPORT_SYMBOL(json_object_get_object);

int json_object_set_bool(struct json_object *object, const char *field_name,
			 bool b)
{
	struct json_field *field;
	int r;

	if (!object->writable)
		return -EROFS;

	r = object_insert(object, field_name, &field);
	if (r < 0)
		return r;

	field->type = JSON_TYPE_BOOL;
	field->value.b = b;

	return 0;
}
EXPORT_SYMBOL(json_object_set_bool);

int json_object_set_int(struct json_object *object, const char *field_name,
			long long i)
{
	struct json_field *field;
	int r;

	if (!object->writable)
		return -EROFS;

	r = object_insert(object, field_name, &field);
	if (r < 0)
		return r;

	field->type = JSON_TYPE_INT;
	field->value.i = i;

	return 0;
}
EXPORT_SYMBOL(json_object_set_int);

int json_object_set_string(struct json_object *object, const char *field_name,
			   const char *string)
{
	struct json_field *field;
	int r;

	if (!object->writable)
		return -EROFS;

	r = object_insert(object, field_name, &field);
	if (r < 0)
		return r;

	field->type = JSON_TYPE_STRING;
	field->value.s = kstrdup(string, GFP_KERNEL);

	return 0;
}
EXPORT_SYMBOL(json_object_set_string);

int json_object_set_array(struct json_object *object, const char *field_name,
			  struct json_array *array)
{
	struct json_field *field;
	int r;

	if (!object->writable)
		return -EROFS;

	r = object_insert(object, field_name, &field);
	if (r < 0)
		return r;

	field->type = JSON_TYPE_ARRAY;
	field->value.array = json_array_ref(array);

	return 0;
}
EXPORT_SYMBOL(json_object_set_array);

int json_object_set_object(struct json_object *object, const char *field_name,
			   struct json_object *nested)
{
	struct json_field *field;
	int r;

	if (!object->writable)
		return -EROFS;

	r = object_insert(object, field_name, &field);
	if (r < 0)
		return r;

	field->type = JSON_TYPE_OBJECT;
	field->value.object = json_object_ref(nested);

	return 0;
}
EXPORT_SYMBOL(json_object_set_object);

int json_object_write_to_buffer(struct json_object *object,
				struct buffer *buffer)
{
	unsigned int n_fields;
	const char **field_names = NULL;
	unsigned int i;
	int r;

	n_fields = json_object_get_field_names(object, &field_names);

	if (n_fields == 0) {
		r = buffer_printf(buffer, "{}");
		goto out;
	}

	r = buffer_printf(buffer, "{");
	if (r < 0)
		return r;

	for (i = 0; i < n_fields; i++) {
		struct json_field *field;

		if (i != 0) {
			r = buffer_printf(buffer, ",");
			if (r < 0)
				goto out;
		}

		r = buffer_printf(buffer, "\"%s\":", field_names[i]);
		if (r < 0)
			goto out;

		field = object_search_field(object, field_names[i]);
		if (!field) {
			r = -ENOENT;
			goto out;
		}

		r = json_value_write_to_buffer(field->type, &field->value,
					       buffer);
		if (r < 0)
			goto out;
	}

	r = buffer_printf(buffer, "}");

out:
	kfree(field_names);
	return r;
}

int json_object_to_string(struct json_object *object, char **stringp)
{
	struct buffer *buffer = NULL;
	int r;

	r = buffer_new(&buffer, 32);
	if (r < 0)
		return r;

	r = json_object_write_to_buffer(object, buffer);
	if (r < 0)
		goto out;

	r = buffer_steal_data(buffer, stringp);

out:
	buffer_free(buffer);
	return r;
}
EXPORT_SYMBOL(json_object_to_string);
