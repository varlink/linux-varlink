#include <linux/slab.h>

#include "buffer.h"

struct buffer {
	char *data;
	unsigned int size;
	unsigned int allocated;
};

int buffer_new(struct buffer **bufferp, unsigned int initial_alloc)
{
	struct buffer *buffer;

	buffer = kzalloc(sizeof(struct buffer), GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	buffer->allocated = initial_alloc;
	buffer->data = kmalloc(initial_alloc, GFP_KERNEL);

	*bufferp = buffer;

	return 0;
}

struct buffer *buffer_free(struct buffer *buffer)
{
	if (!buffer)
		return NULL;

	kfree(buffer->data);
	kfree(buffer);

	return NULL;
}

int buffer_steal_data(struct buffer *buffer, char **datap)
{
	int size = buffer->size;

	if (datap)
		*datap = buffer->data;

	buffer->data = NULL;
	buffer->size = 0;
	buffer->allocated = 0;

	return size;
}

static int buffer_ensure_size(struct buffer *buffer, unsigned int n)
{
	unsigned int need;

	need = buffer->size + n;

	if (need > buffer->allocated) {
		do {
			buffer->allocated *= 2;
		} while (need > buffer->allocated);

		buffer->data = krealloc(buffer->data, buffer->allocated,
					GFP_KERNEL);
		if (!buffer->data)
			return -ENOMEM;
	}

	return 0;
}

int buffer_printf(struct buffer *buffer, const char *fmt, ...)
{
	int r;

	for (;;) {
		va_list ap;

		va_start(ap, fmt);
		r = vsnprintf((char *)buffer->data + buffer->size,
			      buffer->allocated - buffer->size, fmt, ap);
		va_end(ap);

		if (r < 0)
			return r;

		if ((unsigned int)r < buffer->allocated - buffer->size) {
			buffer->size += r;
			break;
		}

		r = buffer_ensure_size(buffer, r + 1);
		if (r < 0)
			return r;
	}

	return 0;
}

int buffer_add_nul(struct buffer *buffer)
{
	int r;

	r = buffer_ensure_size(buffer, 1);
	if (r < 0)
		return r;

	buffer->data[buffer->size] = '\0';
	buffer->size++;

	return 0;
}

int buffer_size(struct buffer *buffer)
{
	if (!buffer)
		return 0;

	return buffer->size;
}
