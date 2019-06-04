#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <err.h>

#include "file_list.h"

static int
file_list_grow(struct file_list *f, int size)
{
	char **fl;
	int i;

	if (f->nentries > f->nsize) {
		fprintf(stderr, "%s: inconsitent internal sizing\n", __func__);
		return (-1);
	}

	/* If we have space then don't reallocate */
	if (f->nentries < f->nsize) {
		return (0);
	}

	fl = calloc(size, sizeof(char *));
	if (fl == NULL) {
		warn("%s: calloc", __func__);
		return (-1);
	}

	for (i = 0; i < f->nentries; i++) {
		fl[i] = f->file_list[i];
	}

	free(f->file_list);
	f->file_list = fl;
	f->nsize = size;
	return (0);
}

struct file_list *
file_list_create(void)
{
	struct file_list *f;

	f = calloc(1, sizeof(*f));
	if (f == NULL) {
		warn("%s: calloc", __func__);
		return (NULL);
	}

	return (f);
}

void
file_list_free(struct file_list *f)
{
	file_list_flush(f);
	if (f->file_list) {
		free(f->file_list);
	}
	free(f);
}

void
file_list_flush(struct file_list *f)
{
	int i;

	for (i = 0; i < f->nentries; i++) {
		free(f->file_list[i]);
	}
	f->nentries = 0;
}

int
file_list_add_entry(struct file_list *f, const char *str)
{
	int r;

	/*
	 * If there's not enough space then add 16 more entries.
	 * It's an arbitrary number but right now we're running
	 * on little embedded things with limited RAM and this
	 * should be used for small archives.
	 */
	if (f->nentries == f->nsize) {
		r = file_list_grow(f, f->nsize + 16);
		if (r != 0) {
			return (r);
		}
	}
	f->file_list[f->nentries] = strdup(str);
	f->nentries++;
	return (0);
}
