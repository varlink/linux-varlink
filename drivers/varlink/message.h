#ifndef _MESSAGE_H_
#define _MESSAGE_H_

#include <linux/json.h>
#include <linux/varlink.h>

int message_unpack_call(struct json_object *call,
			char **methodp,
			struct json_object **parametersp,
			unsigned long long *flagsp);

int message_pack_reply(const char *error,
		       struct json_object *parameters,
		       unsigned long long flags,
		       struct json_object **replyp);
#endif
