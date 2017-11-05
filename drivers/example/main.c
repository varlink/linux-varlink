#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/json.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/usb.h>
#include <linux/utsname.h>
#include <linux/varlink.h>

#include "org.kernel.sysinfo.varlink.c.inc"
#include "org.kernel.devices.usb.varlink.c.inc"

static struct varlink_service *service;

struct monitor {
	struct list_head node;
	struct varlink_connection *conn;
};
static LIST_HEAD(monitor_list);
static DEFINE_MUTEX(monitor_lock);

static struct monitor *monitor_add(struct varlink_connection *conn)
{
	struct monitor *m;

	m = kmalloc(sizeof(struct monitor), GFP_KERNEL);
	m->conn = conn;

	mutex_lock(&monitor_lock);
	list_add(&m->node, &monitor_list);
	mutex_unlock(&monitor_lock);

	return m;
}

static void monitor_remove(struct varlink_connection *conn, void *userdata)
{
	struct monitor *m = userdata;

	mutex_lock(&monitor_lock);
	list_del(&m->node);
	mutex_unlock(&monitor_lock);

	kfree(m);
}

static int org_kernel_sysinfo_GetInfo(struct varlink_connection *connection,
				      const char *method,
				      struct json_object *parameters,
				      long long flags,
				      void *userdata)
{
	struct json_object *reply = NULL;
	int r;

	r = json_object_new(&reply);
	if (r < 0)
		goto out;

	r = json_object_set_string(reply, "sysname", init_utsname()->sysname);
	if (r < 0)
		goto out;

	r = json_object_set_string(reply, "nodename", init_utsname()->nodename);
	if (r < 0)
		goto out;

	r = json_object_set_string(reply, "release", init_utsname()->release);
	if (r < 0)
		goto out;

	r = json_object_set_string(reply, "version", init_utsname()->version);
	if (r < 0)
		goto out;

	r = json_object_set_string(reply, "machine", init_utsname()->machine);
	if (r < 0)
		goto out;

	r = json_object_set_string(reply, "domainname", init_utsname()->domainname);
	if (r < 0)
		goto out;

	r = varlink_connection_reply(connection, 0, reply);

out:
	json_object_unref(reply);
	return r;
}

struct usb_userdata {
	unsigned long long busnum;
	unsigned long long devnum;
	struct usb_device *d;
};

static int usb_device_find(struct usb_device *d, void *userdata)
{
	struct usb_userdata *user = userdata;

	if (d->bus->busnum == user->busnum && d->devnum == user->devnum) {
		user->d = usb_get_dev(d);

		return 1;
	}

	return 0;
}

static int org_kernel_devices_usb_Info(struct varlink_connection *connection,
				       const char *method,
				       struct json_object *parameters,
				       long long flags,
				       void *userdata)
{
	struct usb_userdata user = {};
	struct json_object *device = NULL;
	int r;

	r = json_object_get_int(parameters, "bus_nr", &user.busnum);
	if (r < 0)
		return r;

	r = json_object_get_int(parameters, "device_nr", &user.devnum);
	if (r < 0)
		return r;


	/* Find device by bus and device number. */
	r = usb_for_each_dev(&user, usb_device_find);
	if (r < 0)
		return r;

	if (r == 0)
		return -ESRCH;

	r = json_object_new(&device);
	if (r < 0)
		goto out;

	r = json_object_set_int(device, "vendor_id",
				le16_to_cpu(user.d->descriptor.idVendor));
	if (r < 0)
		goto out;

	r = json_object_set_int(device, "product_id",
				le16_to_cpu(user.d->descriptor.idProduct));
	if (r < 0)
		goto out;

	r = json_object_set_int(device, "bus_nr", user.d->bus->busnum);
	if (r < 0)
		goto out;

	r = json_object_set_int(device, "device_nr", user.d->devnum);
	if (r < 0)
		goto out;


	if (user.d->product) {
		r = json_object_set_string(device, "product", user.d->product);
		if (r < 0)
			goto out;
	}

	if (user.d->manufacturer) {
		r = json_object_set_string(device, "manufacturer", user.d->manufacturer);
		if (r < 0)
			goto out;
	}

	if (user.d->serial) {
		r = json_object_set_string(device, "serial", user.d->serial);
		if (r < 0)
			goto out;
	}

	r = varlink_connection_reply(connection, 0, device);

out:
	usb_put_dev(user.d);
	json_object_unref(device);
	return r;
}

static int usb_device_add(struct usb_device *usb_device, void *userdata)
{
	struct json_array *devices = userdata;
	struct json_object *device = NULL;
	int r;

	r = json_object_new(&device);
	if (r < 0)
		goto out;

	r = json_object_set_int(device, "vendor_id",
				le16_to_cpu(usb_device->descriptor.idVendor));
	if (r < 0)
		goto out;

	r = json_object_set_int(device, "product_id",
				le16_to_cpu(usb_device->descriptor.idProduct));
	if (r < 0)
		goto out;

	r = json_object_set_int(device, "bus_nr", usb_device->bus->busnum);
	if (r < 0)
		goto out;

	r = json_object_set_int(device, "device_nr", usb_device->devnum);
	if (r < 0)
		goto out;

	r = json_object_set_string(device, "product", usb_device->product ?: "");
	if (r < 0)
		goto out;

	r = json_object_set_string(device, "manufacturer",
				   usb_device->manufacturer ?: "");
	if (r < 0)
		goto out;

	r = json_object_set_string(device, "serial", usb_device->serial ?: "");
	if (r < 0)
		goto out;

	r = json_array_append_object(devices, device);

out:
	json_object_unref(device);
	return r;
}

static int org_kernel_devices_usb_Monitor(struct varlink_connection *connection,
					  const char *method,
					  struct json_object *parameters,
					  long long flags,
					  void *userdata)
{
	struct json_array *devices = NULL;
	struct json_object *reply = NULL;
	int r;

	r = json_array_new(&devices);
	if (r < 0)
		return r;

	/* Add all current devices to the array */
	r = usb_for_each_dev(devices, usb_device_add);
	if (r < 0)
		goto out;

	r = json_object_new(&reply);
	if (r < 0)
		return r;

	r = json_object_set_string(reply, "event", "current");
	if (r < 0)
		return r;

	r = json_object_set_array(reply, "devices", devices);
	if (r < 0)
		return r;

	if (flags & VARLINK_CALL_MORE) {
		struct monitor *m;

		r = varlink_connection_reply(connection,
					     VARLINK_REPLY_CONTINUES,
					     reply);

		m = monitor_add(connection);
		varlink_connection_set_closed_callback(connection,
						       monitor_remove, m);

	} else
		r = varlink_connection_reply(connection, 0, reply);

out:
	json_object_unref(reply);
	json_array_unref(devices);
	return r;
}

static int usb_bus_notify(struct notifier_block *nb, unsigned long val,
			  void *userdata)
{
	struct usb_device *d = userdata;
	struct json_array *devices = NULL;
	const char *action = NULL;
	struct json_object *reply = NULL;
	struct monitor *m;
	int r;

	r = json_array_new(&devices);
	if (r < 0)
		goto out;

	r = usb_device_add(d, devices);
	if (r < 0)
		goto out;

	if (val == USB_DEVICE_ADD)
		action = "add";
	else if (val == USB_DEVICE_REMOVE)
		action = "remove";

	r = json_object_new(&reply);
	if (r < 0)
		goto out;

	r = json_object_set_string(reply, "event", action);
	if (r < 0)
		goto out;

	r = json_object_set_array(reply, "devices", devices);
	if (r < 0)
		goto out;

	mutex_lock(&monitor_lock);
	list_for_each_entry(m, &monitor_list, node)
		varlink_connection_reply(m->conn, VARLINK_REPLY_CONTINUES, reply);
	mutex_unlock(&monitor_lock);

out:
	json_array_unref(devices);
	json_object_unref(reply);
	return NOTIFY_OK;
}

static struct notifier_block usb_bus_notifier = {
	.notifier_call = usb_bus_notify,
};

static int __init example_init(void)
{
	struct varlink_service *s = NULL;
	const char *ifaces[] = {
		org_kernel_sysinfo_varlink,
		org_kernel_devices_usb_varlink,
		NULL
	};
	int r;

	r = varlink_service_new(&s,
				"org.kernel.example", 0666,
				THIS_MODULE,
				"Linux",
				"Varlink Example",
				"1.0",
				"http://kernel.org",
				ifaces);
	if (r < 0)
		goto out;

	r = varlink_service_register_callback(s,
					      "org.kernel.sysinfo.GetInfo",
					      org_kernel_sysinfo_GetInfo, NULL);
	if (r < 0)
		goto out;

	r = varlink_service_register_callback(s,
					      "org.kernel.devices.usb.Info",
					      org_kernel_devices_usb_Info, NULL);
	if (r < 0)
		return r;

	r = varlink_service_register_callback(s,
					      "org.kernel.devices.usb.Monitor",
					      org_kernel_devices_usb_Monitor, NULL);
	if (r < 0)
		return r;

	service = s;
	s = NULL;

	usb_register_notify(&usb_bus_notifier);
	pr_info("initialized\n");

out:
	varlink_service_free(s);
	return 0;
}

static void __exit example_exit(void)
{
	usb_unregister_notify(&usb_bus_notifier);
	varlink_service_free(service);
}

module_init(example_init);
module_exit(example_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Varlink Example");
