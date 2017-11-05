#ifndef _SCANNER_H_
#define _SCANNER_H_

struct scanner;

int scanner_new(struct scanner **scannerp, const char *string,
		bool accept_comment);
struct scanner *scanner_free(struct scanner *scanner);

/* Advances the scanner and returns the first character of the next token. */
char scanner_peek(struct scanner *scanner);

int scanner_read_keyword(struct scanner *scanner, const char *keyword);
int scanner_read_number(struct scanner *scanner, long long *numberp);
int scanner_read_string(struct scanner *scanner, char **stringp);
int scanner_read_operator(struct scanner *scanner, const char *op);
int scanner_read_operator_skip(struct scanner *scanner, const char *op);
int scanner_read_word(struct scanner *scanner, char **namep);
#endif
