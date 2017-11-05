#include <linux/bsearch.h>
#include <linux/json.h>
#include <linux/slab.h>
#include <linux/sort.h>
#include <linux/types.h>
#include <linux/varlink.h>

#include "interface.h"
#include "scanner.h"

struct varlink_interface *varlink_interface_free(struct varlink_interface
						 *iface)
{
	unsigned int i;

	if (!iface)
		return NULL;

	for (i = 0; i < iface->n_members; i++)
		kfree(iface->members[i]);
	kfree(iface->members);

	for (i = 0; i < iface->n_methods; i++)
		kfree(iface->methods[i].name);
	kfree(iface->methods);

	for (i = 0; i < iface->n_errors; i++)
		kfree(iface->errors[i]);
	kfree(iface->errors);

	kfree(iface->description);
	kfree(iface->name);
	kfree(iface);

	return NULL;
}

static int interface_name_valid(const char *name)
{
	unsigned int len;
	bool has_dot = false;
	bool has_alpha = false;
	unsigned int i;

	len = strlen(name);
	if (len < 3 || len > 255)
		return -EINVAL;

	if (name[0] == '.' || name[len - 1] == '.')
		return -EINVAL;

	if (name[0] == '-' || name[len - 1] == '-')
		return -EINVAL;

	for (i = 0; i < len; i += 1) {
		switch (name[i]) {
		case 'a' ... 'z':
			has_alpha = true;
			break;

		case '0' ... '9':
			break;

		case '.':
			if (name[i - 1] == '.')
				return -EINVAL;

			if (name[i - 1] == '.')
				return -EINVAL;

			if (!has_alpha)
				return -EINVAL;

			has_dot = true;
			break;

		case '-':
			if (name[i - 1] == '.')
				return -EINVAL;

			break;

		default:
			return -EINVAL;
		}
	}

	if (!has_dot || !has_alpha)
		return -EINVAL;

	return 0;
}

static int member_name_valid(const char *member)
{
	unsigned int len = strlen(member);
	unsigned int i;

	switch (*member) {
	case 'A' ... 'Z':
		break;

	default:
		return -EINVAL;
	}

	for (i = 1; i < len; i += 1) {
		switch (member[i]) {
		case '0' ... '9':
		case 'a' ... 'z':
		case 'A' ... 'Z':
			break;

		default:
			return -EINVAL;
		}
	}

	return 0;
}

static int members_compare(const void *p1, const void *p2)
{
	const char *m1 = *(const char **)p1;
	const char *m2 = *(const char **)p2;

	return strcmp(m1, m2);
}

static int methods_compare(const void *p1, const void *p2)
{
	struct method *m1 = (struct method *)p1;
	struct method *m2 = (struct method *)p2;

	return strcmp(m1->name, m2->name);
}

static int interface_validate(struct varlink_interface *iface)
{
	unsigned int i;
	int r;

	r = interface_name_valid(iface->name);
	if (r < 0)
		return r;

	/* Ensure that all types, methods, errors are unique across the iface. */
	sort(iface->members, iface->n_members, sizeof(char *),
	     members_compare, NULL);

	for (i = 0; i + 1 < iface->n_members; i++)
		if (strcmp(iface->members[i],
			   iface->members[i + 1]) == 0)
			return -ENOTUNIQ;

	/* Ensure that all names match "[A-z][A-Za-z0-9]*" */
	for (i = 0; i < iface->n_members; i++) {
		r = member_name_valid(iface->members[i]);
		if (r < 0)
			return r;
	}

	/* Only needed for validation. */
	for (i = 0; i < iface->n_members; i++)
		kfree(iface->members[i]);
	kfree(iface->members);
	iface->members = NULL;
	iface->n_members = 0;

	sort(iface->methods, iface->n_methods, sizeof(struct method),
	     methods_compare, NULL);

	sort(iface->errors, iface->n_errors, sizeof(char *),
	     members_compare, NULL);

	return 0;
}

static int interface_read_type(struct varlink_interface *iface,
			       struct scanner *scanner)
{
	char *word = NULL;
	int r;

	r = scanner_read_word(scanner, &word);
	if (r < 0)
		return r;

	if (scanner_read_operator(scanner, "(") < 0 ||
	    scanner_read_operator_skip(scanner, ")") < 0) {
		kfree(word);
		return -EINVAL;
	}

	if (iface->n_members_allocated == iface->n_members) {
		iface->n_members_allocated = max(2 * iface->n_members_allocated, 4U);
		iface->members = krealloc(iface->members,
					  iface->n_members_allocated * sizeof(char *),
					  GFP_KERNEL);
		if (!iface->members)
			return -ENOMEM;
	}
	iface->members[iface->n_members++] = word;

	return 0;
}

static int interface_read_method(struct varlink_interface *iface,
				 struct scanner *scanner)
{
	char *word = NULL;
	struct method *m;
	int r;

	r = scanner_read_word(scanner, &word);
	if (r < 0)
		return r;

	if (scanner_read_operator(scanner, "(") < 0 ||
	    scanner_read_operator_skip(scanner, ")") < 0 ||
	    scanner_read_operator(scanner, "->") < 0 ||
	    scanner_read_operator(scanner, "(") < 0 ||
	    scanner_read_operator_skip(scanner, ")") < 0) {
		kfree(word);
		return -EINVAL;
	}

	if (iface->n_members_allocated == iface->n_members) {
		iface->n_members_allocated = max(2 * iface->n_members_allocated, 4U);
		iface->members = krealloc(iface->members,
					  iface->n_members_allocated * sizeof(char *),
					  GFP_KERNEL);
		if (!iface->members)
			return -ENOMEM;
	}
	iface->members[iface->n_members++] = kstrdup(word, GFP_KERNEL);

	if (iface->n_methods_allocated == iface->n_methods) {
		iface->n_methods_allocated = max(2 * iface->n_methods_allocated, 4U);
		iface->methods = krealloc(iface->methods,
					  iface->n_methods_allocated * sizeof(struct method),
					  GFP_KERNEL);
		if (!iface->methods)
			return -ENOMEM;
	}
	m = &iface->methods[iface->n_methods++];
	memset(m, 0, sizeof(struct method));
	m->name = word;

	return 0;
}

static int interface_read_error(struct varlink_interface *iface,
				struct scanner *scanner)
{
	char *word = NULL;
	int r;

	r = scanner_read_word(scanner, &word);
	if (r < 0)
		return r;

	if (scanner_read_operator(scanner, "(") < 0 ||
	    scanner_read_operator_skip(scanner, ")") < 0) {
		kfree(word);
		return -EINVAL;
	}

	if (iface->n_members_allocated == iface->n_members) {
		iface->n_members_allocated = max(2 * iface->n_members_allocated, 16U);
		iface->members = krealloc(iface->members,
					  iface->n_members_allocated * sizeof(char *),
					  GFP_KERNEL);
		if (!iface->members)
			return -ENOMEM;
	}
	iface->members[iface->n_members++] = kstrdup(word, GFP_KERNEL);

	if (iface->n_errors_allocated == iface->n_errors) {
		iface->n_errors_allocated = max(2 * iface->n_errors_allocated, 4U);
		iface->errors = krealloc(iface->errors,
					 iface->n_errors_allocated * sizeof(char *),
					 GFP_KERNEL);
		if (!iface->errors)
			return -ENOMEM;
	}
	iface->errors[iface->n_errors++] = word;

	return 0;
}

int varlink_interface_new(struct varlink_interface **interfacep,
			  const char *description)
{
	struct scanner *scanner;
	struct varlink_interface *iface = NULL;
	int r;

	r = scanner_new(&scanner, description, true);
	if (r < 0)
		return r;

	iface = kzalloc(sizeof(struct varlink_interface), GFP_KERNEL);
	if (!iface)
		return -ENOMEM;

	iface->description = kstrdup(description, GFP_KERNEL);
	if (!iface->description)
		return -ENOMEM;

	if (scanner_read_keyword(scanner, "interface") < 0) {
		r = -EINVAL;
		goto out;
	}

	r = scanner_read_word(scanner, &iface->name);
	if (r < 0)
		goto out;

	while (scanner_peek(scanner) != '\0') {
		if (scanner_read_keyword(scanner, "type") >= 0) {
			r = interface_read_type(iface, scanner);
			if (r < 0)
				goto out;

		} else if (scanner_read_keyword(scanner, "method") >= 0) {
			r = interface_read_method(iface, scanner);
			if (r < 0)
				goto out;

		} else if (scanner_read_keyword(scanner, "error") >= 0) {
			r = interface_read_error(iface, scanner);
			if (r < 0)
				goto out;

		} else {
			r = -EINVAL;
			goto out;
		}
	}

	r = interface_validate(iface);
	if (r < 0)
		goto out;

	*interfacep = iface;
	iface = NULL;

out:
	varlink_interface_free(iface);
	scanner_free(scanner);

	return r;
}

int varlink_interface_find_error(struct varlink_interface *iface,
				 const char *error)
{
	if (bsearch(&error, iface->errors, iface->n_errors, sizeof(char *),
		    members_compare) == NULL)
		return -ESRCH;

	return 0;
}

static struct method *interface_find_method(struct varlink_interface *iface,
					    const char *method)
{
	struct method m = {
		.name = (char *)method,
	};

	return bsearch(&m, iface->methods, iface->n_methods,
		       sizeof(struct method), methods_compare);
}

int varlink_interface_find_method(struct varlink_interface *iface,
				  const char *method,
				  int (**callbackp)(
					  struct varlink_connection *connection,
					  const char *method,
					  struct json_object *parameters,
					  long long flags,
					  void *userdata
				  ),
				  void **userdatap)
{
	struct method *m;

	m = interface_find_method(iface, method);
	if (!m)
		return -ESRCH;

	*callbackp = m->callback;
	*userdatap = m->userdata;

	return 0;
}

int varlink_interface_set_method(struct varlink_interface *iface,
				 const char *method,
				 int (*callback)(
					 struct varlink_connection *connection,
					 const char *method,
					 struct json_object *parameters,
					 long long flags,
					 void *userdata
				 ),
				 void *userdata)
{
	struct method *m;

	m = interface_find_method(iface, method);
	if (!m)
		return -ESRCH;

	m->callback = callback;
	m->userdata = userdata;

	return 0;
}
