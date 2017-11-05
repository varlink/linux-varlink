#include <linux/idr.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/varlink.h>

#include "connection.h"
#include "message.h"
#include "service-io.h"

static DEFINE_IDR(service_io_idr);
static DEFINE_MUTEX(service_io_lock);

static int service_io_fop_open(struct inode *inode, struct file *file)
{
	struct varlink_connection *conn;
	struct varlink_service *service;
	int r;

	mutex_lock(&service_io_lock);
	service = idr_find(&service_io_idr, MINOR(inode->i_rdev));
	if (!service) {
		mutex_unlock(&service_io_lock);
		return -ENOENT;
	}

	if (!try_module_get(service->owner))
		return -ENODEV;

	r = varlink_connection_new(&conn);
	if (r < 0)
		return r;

	conn->service = service;

	file->private_data = conn;
	mutex_unlock(&service_io_lock);

	return 0;
}

static int service_io_fop_release(struct inode *inode, struct file *file)
{
	struct varlink_connection *conn = file->private_data;

	module_put(conn->service->owner);
	varlink_connection_free(conn);

	return 0;
}

static ssize_t service_io_fop_read(struct file *file, char __user *buf,
				   size_t count, loff_t *ppos)
{
	struct varlink_connection *conn = file->private_data;
	char *data = NULL;
	ssize_t size;

	/* Signal once, that we lost one or more messages. */
	mutex_lock(&conn->lock);
	if (conn->overrun) {
		conn->overrun = false;
		size = -ENOBUFS;
		goto out;
	}

	if (buffer_size(conn->buffer) == 0) {
		size = -EAGAIN;
		goto out;
	}

	size = buffer_steal_data(conn->buffer, &data);
	if (copy_to_user(buf, data, size))
		size = -EFAULT;

	kfree(data);
	conn->buffer = buffer_free(conn->buffer);

out:
	mutex_unlock(&conn->lock);
	return size;
}

static ssize_t service_io_fop_write(struct file *file, const char __user *buf,
				    size_t count, loff_t *ppos)
{
	struct varlink_connection *conn = file->private_data;
	char *data = NULL;
	struct json_object *call = NULL;
	struct json_object *parameters = NULL;
	int r;

	if (conn->method)
		return -EBUSY;

	if (count > 128 * 1024)
		return -EMSGSIZE;

	data = memdup_user(buf, count);
	if (IS_ERR(data))
		return PTR_ERR(data);

	r = json_object_new_from_string(&call, data);
	if (r < 0)
		goto out;

	r = message_unpack_call(call,
				&conn->method,
				&parameters,
				&conn->flags_call);
	if (r < 0)
		goto out;

	if (conn->flags_call & VARLINK_CALL_ONEWAY &&
	    conn->flags_call & VARLINK_CALL_MORE) {
		r = -EPROTO;
		goto out;
	}

	r = varlink_service_dispatch_call(conn->service,
					  conn,
					  parameters);

out:
	if (!(conn->flags_reply & VARLINK_REPLY_CONTINUES)) {
		kfree(conn->method);
		conn->method = NULL;
		conn->flags_call = 0;
		conn->flags_reply = 0;
	}

	json_object_unref(parameters);
	json_object_unref(call);
	kfree(data);

	return r;
}

static unsigned int service_io_fop_poll(struct file *file,
					struct poll_table_struct *wait)
{
	struct varlink_connection *conn = file->private_data;

	poll_wait(file, &conn->waitq, wait);

	if (buffer_size(conn->buffer) > 0)
		return POLLIN | POLLRDNORM;

	return 0;
}

static const struct file_operations service_io_fops = {
	.owner = THIS_MODULE,
	.open = service_io_fop_open,
	.release = service_io_fop_release,
	.read = service_io_fop_read,
	.write = service_io_fop_write,
	.poll = service_io_fop_poll,
	.llseek = noop_llseek
};

int service_io_register(struct varlink_service *service,
			const char *device, mode_t mode)
{
	int r;

	service->misc.fops = &service_io_fops;
	service->misc.minor = MISC_DYNAMIC_MINOR;
	service->misc.name = kstrdup(device, GFP_KERNEL);
	service->misc.mode = mode;
	r = misc_register(&service->misc);
	if (r < 0)
		return r;

	mutex_lock(&service_io_lock);
	r = idr_alloc(&service_io_idr, service, service->misc.minor, 0, GFP_KERNEL);
	if (r <= 0) {
		misc_deregister(&service->misc);
		return -EEXIST;
	}
	mutex_unlock(&service_io_lock);

	return 0;
}

void service_io_unregister(struct varlink_service *service)
{
	if (service->misc.minor <= 0)
		return;

	mutex_lock(&service_io_lock);
	idr_remove(&service_io_idr, service->misc.minor);
	mutex_unlock(&service_io_lock);

	misc_deregister(&service->misc);
	kfree(service->misc.name);
}
