#ifndef _SERVICE_H_
#define _SERVICE_H_

#include <linux/json.h>
#include <linux/varlink.h>
#include <linux/miscdevice.h>

#include "service.h"

struct varlink_service {
	struct module *owner;

	char *vendor;
	char *product;
	char *version;
	char *url;

	struct varlink_interface **ifaces;
	unsigned int n_ifaces;
	unsigned int n_ifaces_allocated;

	struct miscdevice misc;
};

int varlink_service_find_interface(struct varlink_service *service,
				   const char *member,
				   struct varlink_interface **ifacep,
				   const char **memberp);

int varlink_service_dispatch_call(struct varlink_service *service,
				  struct varlink_connection *connection,
				  struct json_object *parameters);
#endif
