#ifndef _CONNECTION_H_
#define _CONNECTION_H_

#include <linux/json.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/varlink.h>

#include "buffer.h"
#include "service.h"

struct varlink_connection {
	struct varlink_service *service;

	char *method;
	unsigned long long flags_call;
	unsigned long long flags_reply;

	struct buffer *buffer;
	bool overrun;

	void (*closed_callback)(
		struct varlink_connection *conn,
		void *userdata
	);
	void *closed_userdata;

	struct mutex lock;
	wait_queue_head_t waitq;
};

int varlink_connection_new(struct varlink_connection **connp);
struct varlink_connection *varlink_connection_free(struct varlink_connection
						   *conn);
#endif
