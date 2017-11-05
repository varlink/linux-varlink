#include <linux/json.h>
#include <linux/slab.h>
#include <linux/varlink.h>

#include "buffer.h"
#include "connection.h"
#include "interface.h"
#include "json-object.h"
#include "message.h"

int varlink_connection_new(struct varlink_connection **connp)
{
	struct varlink_connection *conn;

	conn = kzalloc(sizeof(struct varlink_connection), GFP_KERNEL);
	if (!conn)
		return -ENOMEM;

	mutex_init(&conn->lock);
	init_waitqueue_head(&conn->waitq);

	*connp = conn;
	return 0;
}

struct varlink_connection
*varlink_connection_free(struct varlink_connection *conn)
{
	if (conn->closed_callback)
		conn->closed_callback(conn, conn->closed_userdata);

	kfree(conn->method);

	buffer_free(conn->buffer);
	kfree(conn);

	return NULL;
}

static int connection_reply(struct varlink_connection *conn,
			    const char *error,
			    long long flags,
			    struct json_object *parameters)
{
	struct json_object *reply;
	int r;

	if (conn->flags_call & VARLINK_CALL_ONEWAY)
		return 0;

	if (flags & VARLINK_REPLY_CONTINUES &&
	    !(conn->flags_call & VARLINK_CALL_MORE))
		return -EPROTO;

	r = message_pack_reply(error, parameters, flags, &reply);
	if (r < 0)
		return r;

	mutex_lock(&conn->lock);
	if (!conn->buffer) {
		r = buffer_new(&conn->buffer, 256);
		if (r < 0)
			goto out;
	}

	if (buffer_size(conn->buffer) > 128 * 1024) {
		r = -ENOBUFS;
		conn->overrun = true;
		goto out;
	}

	r = json_object_write_to_buffer(reply, conn->buffer);
	if (r < 0)
		goto out;

	r = buffer_add_nul(conn->buffer);
	if (r < 0)
		goto out;

	conn->flags_reply = flags;
	wake_up_interruptible(&conn->waitq);

out:
	mutex_unlock(&conn->lock);
	json_object_unref(reply);
	return r;
}

int varlink_connection_reply(struct varlink_connection *conn,
				    long long flags,
				    struct json_object *parameters)
{
	return connection_reply(conn, NULL, flags, parameters);
}
EXPORT_SYMBOL(varlink_connection_reply);

int varlink_connection_error(struct varlink_connection *conn,
			     const char *error,
			     struct json_object *parameters)
{
	struct varlink_interface *iface_error;
	const char *member_error;
	int r;

	r = varlink_service_find_interface(conn->service, error,
					   &iface_error, &member_error);
	if (r < 0)
		return r;

	r = varlink_interface_find_error(iface_error, member_error);
	if (r < 0)
		return r;

	if (strcmp(iface_error->name, "org.varlink.service") != 0) {
		struct varlink_interface *iface_method;

		r = varlink_service_find_interface(conn->service, conn->method,
						   &iface_method, NULL);
		if (r < 0)
			return r;

		if (strcmp(iface_error->name, iface_method->name) != 0)
			return -EINVAL;
	}

	return connection_reply(conn, error, 0, parameters);
}
EXPORT_SYMBOL(varlink_connection_error);

void varlink_connection_set_closed_callback(struct varlink_connection *conn,
					    void (*callback)(
						    struct varlink_connection *conn,
						    void *userdata
					    ),
					    void *userdata)
{
	conn->closed_callback = callback;
	conn->closed_userdata = userdata;
}
EXPORT_SYMBOL(varlink_connection_set_closed_callback);
