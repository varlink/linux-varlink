#ifndef _SERVICE_IO_H_
#define _SERVICE_IO_H_

#include "service.h"

int service_io_register(struct varlink_service *service,
			const char *device, mode_t mode);
void service_io_unregister(struct varlink_service *service);
#endif
