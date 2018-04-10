#include <linux/slab.h>
#include <linux/bsearch.h>
#include <linux/sort.h>

#include "connection.h"
#include "interface.h"
#include "org.varlink.service.varlink.c.inc"
#include "service.h"
#include "service-io.h"

struct varlink_service *varlink_service_free(struct varlink_service *service)
{
	unsigned int i;

	if (!service)
		return NULL;

	service_io_unregister(service);

	for (i = 0; i < service->n_ifaces; i++)
		varlink_interface_free(service->ifaces[i]);
	kfree(service->ifaces);

	kfree(service->vendor);
	kfree(service->product);
	kfree(service->version);
	kfree(service->url);
	kfree(service);

	return NULL;
}
EXPORT_SYMBOL(varlink_service_free);

static int service_add_interface(struct varlink_service *service,
				 const char *description)
{
	struct varlink_interface *iface;
	int r;

	r = varlink_interface_new(&iface, description);
	if (r < 0)
		return r;

	if (service->n_ifaces_allocated == service->n_ifaces) {
		service->n_ifaces_allocated = max(2 * service->n_ifaces_allocated, 2U);
		service->ifaces = krealloc(service->ifaces,
					   service->n_ifaces_allocated * sizeof(void *),
					   GFP_KERNEL);
		if (!service->ifaces) {
			varlink_interface_free(iface);
			return -ENOMEM;
		}
	}
	service->ifaces[service->n_ifaces++] = iface;

	return 0;
}

static int ifaces_compare(const void *p1, const void *p2)
{
	struct varlink_interface *i1 = *(struct varlink_interface **)p1;
	struct varlink_interface *i2 = *(struct varlink_interface **)p2;

	return strcmp(i1->name, i2->name);
}

static struct varlink_interface *interface_find(struct varlink_interface
						**ifaces,
						unsigned int n_ifaces,
						const char *name)
{
	struct varlink_interface _iface = {
		.name = (char *)name,
	};
	struct varlink_interface *iface = &_iface;
	struct varlink_interface **ifacep;

	ifacep = bsearch(&iface, ifaces, n_ifaces, sizeof(void *),
			 ifaces_compare);

	if (!ifacep)
		return NULL;

	return *ifacep;
}

static int org_varlink_service_GetInfo(struct varlink_connection *connection,
				       const char *method,
				       struct json_object *parameters,
				       long long flags,
				       void *userdata)
{
	struct varlink_service *service = userdata;
	struct json_array *ifaces = NULL;
	struct json_object *info = NULL;
	unsigned int i;
	int r;

	r = json_object_new(&info);
	if (r < 0)
		goto out;

	json_object_set_string(info, "vendor", service->vendor);
	if (r < 0)
		goto out;

	json_object_set_string(info, "product", service->product);
	if (r < 0)
		goto out;

	json_object_set_string(info, "version", service->version);
	if (r < 0)
		goto out;

	json_object_set_string(info, "url", service->url);
	if (r < 0)
		goto out;

	r = json_array_new(&ifaces);
	if (r < 0)
		goto out;

	for (i = 0; i < service->n_ifaces; i++) {
		r = json_array_append_string(ifaces, service->ifaces[i]->name);
		if (r < 0)
			goto out;
	}

	json_object_set_array(info, "interfaces", ifaces);
	if (r < 0)
		goto out;

	ifaces = NULL;

	r = varlink_connection_reply(connection, 0, info);
	if (r < 0)
		goto out;

	info = NULL;

out:
	json_array_unref(ifaces);
	json_object_unref(info);
	return r;
}

static int org_varlink_service_GetInterfaceDescription(struct varlink_connection
						       *connection,
						       const char *method,
						       struct json_object *parameters,
						       long long flags,
						       void *userdata)
{
	struct varlink_service *service = userdata;
	const char *name;
	struct varlink_interface *iface;
	struct json_object *reply = NULL;
	int r;

	if (json_object_get_string(parameters, "interface", &name) < 0)
		return varlink_connection_error(connection,
						"org.varlink.service.InvalidParameter",
						NULL);

	iface = interface_find(service->ifaces, service->n_ifaces, name);
	if (!iface)
		return varlink_connection_error(connection,
						"org.varlink.service.InterfaceNotFound",
						NULL);

	r = json_object_new(&reply);
	if (r < 0)
		goto out;

	r = json_object_set_string(reply, "description", iface->description);
	if (r < 0)
		goto out;

	r = varlink_connection_reply(connection, 0, reply);

out:
	json_object_unref(reply);
	return r;
}

int varlink_service_new(struct varlink_service **servicep,
			const char *device, mode_t mode,
			struct module *owner,
			const char *vendor,
			const char *product,
			const char *version,
			const char *url,
			const char *ifacesv[])
{
	struct varlink_service *service;
	unsigned int i;
	int r = 0;

	service = kzalloc(sizeof(struct varlink_service), GFP_KERNEL);
	if (!service)
		return -ENOMEM;

	service->owner = owner;
	service->vendor = kstrdup(vendor, GFP_KERNEL);
	service->product = kstrdup(product, GFP_KERNEL);
	service->version = kstrdup(version, GFP_KERNEL);
	service->url = kstrdup(url, GFP_KERNEL);
	if (!service->vendor || !service->product ||
	    !service->version || !service->url) {
		r = -ENOMEM;
		goto out;
	}

	/* Add org.varlink.service interface */
	r = service_add_interface(service, org_varlink_service_varlink);
	if (r < 0)
		goto out;

	r = varlink_service_register_callback(service,
					      "org.varlink.service.GetInfo",
					      org_varlink_service_GetInfo,
					      service);
	if (r < 0)
		goto out;

	r = varlink_service_register_callback(service,
					      "org.varlink.service.GetInterfaceDescription",
					      org_varlink_service_GetInterfaceDescription,
					      service);
	if (r < 0)
		goto out;

	/* Register custom interfaces. */
	for (i = 0; ifacesv[i]; i++) {
		r = service_add_interface(service, ifacesv[i]);
		if (r < 0)
			goto out;
	}

	sort(service->ifaces, service->n_ifaces, sizeof(void *),
	     ifaces_compare, NULL);

	r = service_io_register(service, device, mode);
	if (r < 0)
		goto out;

	*servicep = service;
	service = NULL;

out:
	varlink_service_free(service);
	return r;
}
EXPORT_SYMBOL(varlink_service_new);

int varlink_service_find_interface(struct varlink_service *service,
				   const char *method,
				   struct varlink_interface **ifacep,
				   const char **methodp)
{
	const char *method_name;
	char *interface_name = NULL;
	struct varlink_interface *iface;
	int r = 0;

	/* Split fully-qualified method name in interface and method name. */
	method_name = strrchr(method, '.');
	if (!method_name)
		return -EINVAL;

	interface_name = kstrndup(method, method_name - method, GFP_KERNEL);
	if (!interface_name) {
		r = -ENOMEM;
		goto out;
	}

	method_name++;

	iface = interface_find(service->ifaces, service->n_ifaces,
			       interface_name);
	if (!iface) {
		r = -ESRCH;
		goto out;
	}

	if (ifacep)
		*ifacep = iface;

	if (methodp)
		*methodp = method_name;

out:
	kfree(interface_name);
	return r;
}

int varlink_service_register_callback(struct varlink_service *service,
				      const char *method,
				      int (*callback)(
					      struct varlink_connection *connection,
					      const char *method,
					      struct json_object *parameters,
					      long long flags,
					      void *userdata
				      ),
				      void *userdata)
{
	struct varlink_interface *iface;
	const char *method_name;
	int r;

	r = varlink_service_find_interface(service, method, &iface,
					   &method_name);
	if (r < 0)
		return r;

	return varlink_interface_set_method(iface, method_name, callback, userdata);
}
EXPORT_SYMBOL(varlink_service_register_callback);

int varlink_service_dispatch_call(struct varlink_service *service,
				  struct varlink_connection *connection,
				  struct json_object *parameters)
{
	struct varlink_interface *iface;
	const char *method_name;
	int (*callback)(
		struct varlink_connection *connection,
		const char *method,
		struct json_object *parameters,
		long long flags,
		void *userdata
	);
	void *userdata;
	int r;

	r = varlink_service_find_interface(service, connection->method,
					   &iface, &method_name);
	if (r < 0)
		return varlink_connection_error(connection,
						"org.varlink.service.InterfaceNotFound",
						NULL);

	r = varlink_interface_find_method(iface, method_name, &callback, &userdata);
	if (r < 0)
		return varlink_connection_error(connection,
						"org.varlink.service.MethodNotFound",
						NULL);

	if (!callback)
		return varlink_connection_error(connection,
						"org.varlink.service.MethodNotImplemented",
						NULL);

	return callback(connection, connection->method, parameters,
			connection->flags_call, userdata);
}
