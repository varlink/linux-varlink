#ifndef _JSON_H_
#define _JSON_H_

#include <linux/types.h>

struct json_object;
struct json_array;

int json_object_new(struct json_object **objectp);
struct json_object *json_object_ref(struct json_object *object);
struct json_object *json_object_unref(struct json_object *object);
unsigned int json_object_get_field_names(struct json_object *object,
					 const char ***namesp);

int json_object_new_from_string(struct json_object **objectp,
				const char *string);
int json_object_to_string(struct json_object *object, char **stringp);

int json_object_get_bool(struct json_object *object, const char *field,
			 bool *bp);
int json_object_get_int(struct json_object *object, const char *field,
			long long *ip);
int json_object_get_string(struct json_object *object, const char *field,
			   const char **stringp);
int json_object_get_array(struct json_object *object, const char *field,
			  struct json_array **arrayp);
int json_object_get_object(struct json_object *object, const char *field,
			   struct json_object **nestedp);

int json_object_set_bool(struct json_object *object, const char *field, bool b);
int json_object_set_int(struct json_object *object, const char *field,
			long long i);
int json_object_set_string(struct json_object *object, const char *field,
			   const char *string);
int json_object_set_array(struct json_object *object, const char *field,
			  struct json_array *array);
int json_object_set_object(struct json_object *object, const char *field,
			   struct json_object *nested);

int json_array_new(struct json_array **arrayp);
struct json_array *json_array_ref(struct json_array *array);
struct json_array *json_array_unref(struct json_array *array);
unsigned int json_array_get_n_elements(struct json_array *array);

int json_array_get_bool(struct json_array *array, unsigned int index, bool *bp);
int json_array_get_int(struct json_array *array, unsigned int index,
		       long long *ip);
int json_array_get_string(struct json_array *array, unsigned int index,
			  const char **stringp);
int json_array_get_array(struct json_array *array, unsigned int index,
			 struct json_array **elementp);
int json_array_get_object(struct json_array *array, unsigned int index,
			  struct json_object **objectp);

int json_array_append_bool(struct json_array *array, bool b);
int json_array_append_int(struct json_array *array, long long i);
int json_array_append_string(struct json_array *array, const char *string);
int json_array_append_array(struct json_array *array,
			    struct json_array *element);
int json_array_append_object(struct json_array *array,
			     struct json_object *object);
#endif
