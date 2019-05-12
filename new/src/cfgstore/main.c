#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <err.h>
#include <fcntl.h>
#include <limits.h>

#include <sys/stat.h>

#include "file_list.h"
#include "cpio_format.h"
#include "cpio_archive.h"

const char *files[] = {
	"cpio_archive.c",
	"cpio_format.c",
	"file_list.c",
	"main.c",
	NULL
};

static void
char_trim_crlf(char *s)
{
	int l;

	l = strlen(s);
	if (l == 0) {
		return;
	}
	l--;
	do {
		if (s[l] == '\r' || s[l] == '\n') {
			s[l] = '\0';
			l--;
		} else {
			break;
		}
	} while (l > 0);
}

static int
cpio_archive_output_create(const char *manifest, const char *outfile)
{
	struct cpio_archive *a;
	FILE *fp;

	a = cpio_archive_create(outfile, CPIO_ARCHIVE_MODE_WRITE);
	if (a == NULL) {
		fprintf(stderr, "ERROR: couldn't create archive for output\n");
		return (-1);
	}

	fp = fopen(manifest, "r");
	if (fp == NULL) {
		warn("%s: fopen('%s')", __func__, manifest);
		cpio_archive_free(a);
		return (-1);
	}

	while (!feof(fp)) {
		char filename[PATH_MAX];
		char *p;

		p = fgets(filename, PATH_MAX - 1, fp);
		/* Handle EOF here too */
		if (p == NULL) {
			break;
		}

		/* Trim trailing \r\n */
		char_trim_crlf(p);

		fprintf(stderr, "adding: '%s'\n", p);

		if (cpio_archive_write_add_manifest(a, p) != 0) {
			fprintf(stderr,
			    "ERROR: couldn't add file '%s' to archive\n",
			    filename);
			continue;
		}
	}

	/* XXX TODO: handle errors here; clean up */
	(void) cpio_archive_set_base_directory(a, ".");
	(void) cpio_archive_open(a);
	/* Files are added to the manifest list, create things */
	(void) cpio_archive_write_files(a);
	(void) cpio_archive_close(a);
	(void) cpio_archive_free(a);
	fclose(fp);
	return (0);
}

int
main(int argc, const char *argv[])
{
#if 1
	cpio_archive_output_create("manifest", "archive.cfgstore");
#else
	struct cpio_archive *a;
	a = cpio_archive_create("/dev/stdin", CPIO_ARCHIVE_MODE_READ);
	// Set the base directory for file operations
	// note this sets it on any files read/written from the archive,
	// not the archive itself!
	(void) cpio_archive_set_base_directory(a, ".");
	cpio_archive_open(a);
	cpio_archive_begin_read(a);
	cpio_archive_close(a);
	cpio_archive_free(a);
#endif


	exit (0);
}
