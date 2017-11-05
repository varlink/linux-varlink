#ifndef _BUFFER_H_
#define _BUFFER_H_

struct buffer;

int buffer_new(struct buffer **bufferp, unsigned int initial_alloc);
struct buffer *buffer_free(struct buffer *buffer);
int buffer_printf(struct buffer *buffer, const char *fmt, ...)
__attribute__ ((format (printf, 2, 3)));
int buffer_add_nul(struct buffer *buffer);
int buffer_steal_data(struct buffer *buffer, char **datap);
int buffer_size(struct buffer *buffer);
#endif
