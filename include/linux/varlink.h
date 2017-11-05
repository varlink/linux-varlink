#ifndef _VARLINK_H_
#define _VARLINK_H_

#include "json.h"

/*
 * Keywords/flags of a method call.
 */
enum {
	VARLINK_CALL_MORE = 1,
	VARLINK_CALL_ONEWAY = 2
};

/*
 * Keywords/flags of a method reply.
 */
enum {
	VARLINK_REPLY_CONTINUES = 1
};

struct varlink_service;
struct varlink_connection;

int varlink_service_new(struct varlink_service **servicep,
			const char *device, mode_t mode,
			struct module *owner,
			const char *vendor,
			const char *product,
			const char *version,
			const char *url,
			const char *ifacesv[]);
struct varlink_service *varlink_service_free(struct varlink_service *service);

int varlink_service_register_callback(struct varlink_service *service,
				      const char *method,
				      int (*callback)(
					      struct varlink_connection *connection,
					      const char *method,
					      struct json_object *parameters,
					      long long flags,
					      void *userdata
				      ),
				      void *userdata);

int varlink_connection_reply(struct varlink_connection *connection,
			     long long flags,
			     struct json_object *parameters);
int varlink_connection_error(struct varlink_connection *connection,
			     const char *error,
			     struct json_object *parameters);

void varlink_connection_set_closed_callback(struct varlink_connection *conn,
					    void (*callback)(
						    struct varlink_connection *conn,
						    void *userdata
					    ),
					    void *userdata);
#endif
