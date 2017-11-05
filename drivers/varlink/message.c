#include <linux/errno.h>
#include <linux/slab.h>

#include "message.h"

int message_unpack_call(struct json_object *call,
			char **methodp,
			struct json_object **parametersp,
			unsigned long long *flagsp)
{
	const char *call_method;
	struct json_object *call_parameters = NULL;
	char *method = NULL;
	struct json_object *parameters = NULL;
	bool more = false;
	bool oneway = false;
	int r;

	r = json_object_get_string(call, "method", &call_method);
	if (r < 0)
		return -EBADMSG;

	r = json_object_get_object(call, "parameters", &call_parameters);
	if (r < 0 && r != -ENOENT)
		return -EBADMSG;

	r = json_object_get_bool(call, "more", &more);
	if (r < 0 && r != -ENOENT)
		return -EBADMSG;

	r = json_object_get_bool(call, "oneway", &oneway);
	if (r < 0 && r != -ENOENT)
		return -EBADMSG;

	method = kstrdup(call_method, GFP_KERNEL);
	if (!method)
		return -ENOMEM;

	r = 0;

	if (call_parameters) {
		parameters = json_object_ref(call_parameters);
	} else {
		r = json_object_new(&parameters);
		if (r < 0)
			goto out;
	}

	*methodp = method;
	method = NULL;

	*parametersp = parameters;
	parameters = NULL;

	*flagsp = 0;
	if (more)
		*flagsp |= VARLINK_CALL_MORE;
	if (oneway)
		*flagsp |= VARLINK_CALL_ONEWAY;

out:
	kfree(method);
	json_object_unref(parameters);

	return r;
}

int message_pack_reply(const char *error,
		       struct json_object *parameters,
		       unsigned long long flags,
		       struct json_object **replyp)
{
	struct json_object *reply = NULL;
	int r;

	r = json_object_new(&reply);
	if (r < 0)
		return r;

	if (error) {
		r = json_object_set_string(reply, "error", error);
		if (r < 0)
			goto out;
	}

	if (parameters) {
		json_object_set_object(reply, "parameters", parameters);
		if (r < 0)
			goto out;
	}

	if (flags & VARLINK_REPLY_CONTINUES) {
		json_object_set_bool(reply, "continues", true);
		if (r < 0)
			goto out;
	}

	*replyp = reply;
	reply = NULL;

out:
	json_object_unref(reply);
	return r;
}
