#ifndef _INTERFACE_H_
#define _INTERFACE_H_

#include <linux/json.h>
#include <linux/varlink.h>

struct varlink_interface {
	char *name;
	char *description;

	/* List of methods with registered callbacks */
	struct method {
		char *name;
		int (*callback)(
			struct varlink_connection *connection,
			const char *method,
			struct json_object *parameters,
			long long flags,
			void *userdata
		);
		void *userdata;
	} *methods;
	unsigned int n_methods;
	unsigned int n_methods_allocated;

	/* List of defined errors which can be returned */
	char **errors;
	unsigned int n_errors;
	unsigned int n_errors_allocated;

	/* Only used during parsing to ensure uniqueness of identifiers */
	char **members;
	unsigned int n_members;
	unsigned int n_members_allocated;
};

int varlink_interface_new(struct varlink_interface **interfacep,
			  const char *description);
struct varlink_interface *varlink_interface_free(struct varlink_interface
						 *iface);

int varlink_interface_find_error(struct varlink_interface *iface,
				 const char *error);

int varlink_interface_find_method(struct varlink_interface *iface,
				  const char *method,
				  int (**callbackp)(
					  struct varlink_connection *connection,
					  const char *method,
					  struct json_object *parameters,
					  long long flags,
					  void *userdata
				  ),
				  void **userdatap);
int varlink_interface_set_method(struct varlink_interface *iface,
				 const char *method,
				 int (*callbackp)(
					 struct varlink_connection *connection,
					 const char *method,
					 struct json_object *parameters,
					 long long flags,
					 void *userdata
				 ),
				 void *userdatap);
#endif
