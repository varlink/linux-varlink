#include <linux/json.h>
#include <linux/slab.h>

#include "buffer.h"
#include "scanner.h"

struct scanner {
	const char *string;
	const char *p;
	bool comment;
};

int scanner_new(struct scanner **scannerp,
		const char *string,
		bool accept_comment)
{
	struct scanner *scanner;

	scanner = kzalloc(sizeof(struct scanner), GFP_KERNEL);
	if (!scanner)
		return -ENOMEM;

	scanner->string = string;
	scanner->p = scanner->string;
	scanner->comment = accept_comment;

	*scannerp = scanner;
	return 0;
}

struct scanner *scanner_free(struct scanner *scanner)
{
	kfree(scanner);
	return NULL;
}

static const char *scanner_advance(struct scanner *scanner)
{
	for (;;) {
		switch (*scanner->p) {
		case ' ':
		case '\t':
		case '\n':
			scanner->p++;
			break;

		case '#':
			if (scanner->comment) {
				scanner->p = strchrnul(scanner->p, '\n');
				break;
			}
		/* fall through */

		default:
			return scanner->p;
		}
	}
}

char scanner_peek(struct scanner *scanner)
{
	scanner_advance(scanner);

	return *scanner->p;
}

static int unhex(char d, unsigned char *valuep)
{
	switch (d) {
	case '0' ... '9':
		*valuep = d - '0';
		return 0;

	case 'a' ... 'f':
		*valuep = d - 'a' + 0x0a;
		return 0;

	case 'A' ... 'F':
		*valuep = d - 'A' + 0x0a;
		return 0;

	default:
		return -EINVAL;
	}
}

static int read_unicode_char(const char *p, struct buffer *buffer)
{
	unsigned int i;
	unsigned char digits[4];
	unsigned short cp;
	int r;

	for (i = 0; i < 4; i++) {
		if (p[i] == '\0')
			return -EINVAL;

		r = unhex(p[i], &digits[i]);
		if (r < 0)
			return r;
	}

	cp = digits[0] << 12 | digits[1] << 8 | digits[2] << 4 | digits[3];

	if (cp <= 0x007f) {
		r = buffer_printf(buffer, "%c", (char)cp);
		if (r < 0)
			return r;

	} else if (cp <= 0x07ff) {
		r = buffer_printf(buffer, "%c", (char)(0xc0 | (cp >> 6)));
		if (r < 0)
			return r;

		r = buffer_printf(buffer, "%c", (char)(0x80 | (cp & 0x3f)));
		if (r < 0)
			return r;
	}

	else {
		r = buffer_printf(buffer, "%c", (char)(0xe0 | (cp >> 12)));
		if (r < 0)
			return r;

		r = buffer_printf(buffer, "%c", (char)(0x80 | ((cp >> 6) & 0x3f)));
		if (r < 0)
			return r;

		r = buffer_printf(buffer, "%c", (char)(0x80 | (cp & 0x3f)));
		if (r < 0)
			return r;
	}

	return 0;
}

static unsigned int scanner_word_len(struct scanner *scanner)
{
	unsigned int i;

	scanner_advance(scanner);

	switch (*scanner->p) {
	case 'a' ... 'z':
	case 'A' ... 'Z':
		break;

	default:
		return 0;
	}

	for (i = 1;; i++) {
		switch (scanner->p[i]) {
		case '0' ... '9':
		case 'a' ... 'z':
		case 'A' ... 'Z':
		case '_':
		case '.':
			break;

		default:
			return i;
		}
	}
}

int scanner_read_keyword(struct scanner *scanner,
			 const char *keyword)
{
	unsigned int word_len = scanner_word_len(scanner);
	unsigned int keyword_len = strlen(keyword);

	if (word_len != keyword_len)
		return -EINVAL;

	if (strncmp(scanner->p, keyword, word_len) != 0)
		return -EINVAL;

	scanner->p += word_len;

	return 0;
}

int scanner_read_number(struct scanner *scanner, long long *numberp)
{
	long long number;
	char *end;

	scanner_advance(scanner);

	number = simple_strtol(scanner->p, &end, 10);
	if (end == scanner->p)
		return -EINVAL;

	scanner->p = end;
	*numberp = number;

	return 0;
}

int scanner_read_string(struct scanner *scanner, char **stringp)
{
	struct buffer *buffer = NULL;
	const char *p;
	int r;

	p = scanner_advance(scanner);
	if (*p != '"')
		return -EINVAL;

	p++;

	r = buffer_new(&buffer, 8);
	if (r < 0)
		return r;

	for (;;) {
		if (*p == '\0') {
			r = EINVAL;
			goto out;
		}

		if (*p == '"') {
			p++;
			break;
		}

		if (*p == '\\') {
			p++;

			switch (*p) {
			case '"':
				r = buffer_printf(buffer, "\"");
				if (r < 0)
					goto out;
				break;

			case '\\':
				r = buffer_printf(buffer, "\\");
				if (r < 0)
					goto out;
				break;

			case '/':
				r = buffer_printf(buffer, "/");
				if (r < 0)
					goto out;
				break;

			case 'b':
				r = buffer_printf(buffer, "\b");
				if (r < 0)
					goto out;
				break;

			case 'f':
				r = buffer_printf(buffer, "\f");
				if (r < 0)
					goto out;
				break;

			case 'n':
				r = buffer_printf(buffer, "\n");
				if (r < 0)
					goto out;
				break;

			case 'r':
				r = buffer_printf(buffer, "\r");
				if (r < 0)
					goto out;
				break;

			case 't':
				r = buffer_printf(buffer, "\t");
				if (r < 0)
					goto out;
				break;

			case 'u':
				r = read_unicode_char(p + 1, buffer);
				if (r < 0)
					goto out;

				p += 4;
				break;

			default:
				r = -EINVAL;
				goto out;
			}

		} else {
			r = buffer_printf(buffer, "%c", *p);
			if (r < 0)
				goto out;
		}

		p++;
	}

	buffer_steal_data(buffer, stringp);
	scanner->p = p;

out:
	buffer_free(buffer);
	return r;
}

int scanner_read_word(struct scanner *scanner, char **namep)
{
	unsigned int len = scanner_word_len(scanner);

	if (namep) {
		char *name;

		name = kstrndup(scanner->p, len, GFP_KERNEL);
		if (!name)
			return -ENOMEM;

		*namep = name;
	}

	scanner->p += len;
	return 0;
}

int scanner_read_operator_skip(struct scanner *scanner, const char *op)
{
	const char *p;

	scanner_advance(scanner);

	p = strstr(scanner->p, op);
	if (!p)
		return -EINVAL;

	scanner->p += p - scanner->p + strlen(op);
	return 0;
}


int scanner_read_operator(struct scanner *scanner, const char *op)
{
	unsigned int length = strlen(op);

	scanner_advance(scanner);

	if (strncmp(scanner->p, op, length) != 0)
		return -EINVAL;

	scanner->p += length;
	return 0;
}
