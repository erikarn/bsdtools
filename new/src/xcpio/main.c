#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
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
cpio_archive_extract(const char *base_directory, const char *archive_file,
    bool do_extract, int block_size)
{
	struct cpio_archive *a = NULL;
	int r;

	/* XXX TODO: any error handling! */
	a = cpio_archive_create(archive_file, CPIO_ARCHIVE_MODE_READ);
	if (a == NULL) {
		goto error;
	}
	cpio_archive_set_blocksize(a, block_size);

	if (base_directory == NULL) {
		r = cpio_archive_set_base_directory(a, ".");
	} else {
		r = cpio_archive_set_base_directory(a, base_directory);
	}
	if (r != 0) {
		fprintf(stderr, "ERROR: couldn't set base directory\n");
		goto error;
	}

	if (cpio_archive_open(a) < 0) {
		fprintf(stderr, "ERROR: failed to open archive\n");
		goto error;
	}
	cpio_archive_begin_read(a, do_extract);
	cpio_archive_close(a);
	cpio_archive_free(a);
	return (0);
error:
	if (a != NULL)
		cpio_archive_free(a);
	return (-1);
}

static int
cpio_archive_output_create(const char *base_directory, const char *manifest,
    const char *outfile, int block_size)
{
	struct cpio_archive *a = NULL;
	FILE *fp = NULL;
	int r;

	a = cpio_archive_create(outfile, CPIO_ARCHIVE_MODE_WRITE);
	if (a == NULL) {
		fprintf(stderr, "ERROR: couldn't create archive for output\n");
		return (-1);
	}
	cpio_archive_set_blocksize(a, block_size);

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

	if (base_directory == NULL) {
		r = cpio_archive_set_base_directory(a, ".");
	} else {
		r = cpio_archive_set_base_directory(a, base_directory);
	}
	if (r != 0) {
		fprintf(stderr, "ERROR: couldn't set base directory\n");
		goto error;
	}

	/* XXX TODO: handle errors here; clean up */
	if (cpio_archive_open(a) < 0) {
		fprintf(stderr, "ERROR: failed to open archive\n");
		goto error;
	}
	/* Files are added to the manifest list, create things */
	(void) cpio_archive_write_files(a);
	(void) cpio_archive_close(a);
	(void) cpio_archive_free(a);
	fclose(fp);
	return (0);
error:
	if (a != NULL)
		cpio_archive_free(a);
	if (fp != NULL)
		fclose(fp);
	return (-1);
}

static void
usage(void)
{
	printf("Usage: xcpio [-b <blocksize>] [-c] [-e] [-f <archive>] [-m <manifest>] [-d <directory>]\n");
	printf("  -b <blocksize> : archive read/write block size in bytes\n");
	printf("  -c             : create an archive\n");
	printf("  -d <directory> : base directory for creating/extracting archives\n");
	printf("  -e             : extract from archive\n");
	printf("  -f <archive>   : filename of the archive\n");
	printf("  -l             : list files in archive\n");
	printf("  -m <manifest>  : archive manifest to create with\n");
	exit(127);
}

int
main(int argc, char *argv[])
{
	char *manifest_file = NULL;
	char *archive_file = NULL;
	char *base_directory = NULL;
	bool is_extract = false;
	bool is_create = false;
	bool is_list = false;
	int block_size = DEFAULT_CPIO_BLOCK_SIZE;
	int ch;

	while ((ch = getopt(argc, argv, "b:cd:ef:lm:")) != -1) {
		switch (ch) {
		case 'b':
			block_size = atoi(optarg);
			break;
		case 'c':
			is_create = true;
			break;
		case 'd':
			free(base_directory);
			base_directory = strdup(optarg);
			break;
		case 'e':
			is_extract = true;
			break;
		case 'f':
			free(archive_file);
			archive_file = strdup(optarg);
			break;
		case 'l':
			is_list = true;
			break;
		case 'm':
			free(manifest_file);
			manifest_file = strdup(optarg);
			break;
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	/* Hack! */
	if (is_list == true)
		is_extract = true;

	if (is_extract && is_create) {
		fprintf(stderr, "ERROR: only one of -c and -e is valid.\n");
		exit(127);
	}
	if ((is_extract == false) && (is_create == false)) {
		fprintf(stderr, "ERROR: need either -c, -l or -e\n");
		exit(127);
	}
	if (archive_file == NULL) {
		fprintf(stderr, "ERROR: need -f <archive> to define the "
		    "archive file to operate on\n");
		exit(127);
	}
	if ((is_create == true) && (manifest_file == NULL)) {
		fprintf(stderr, "ERROR: need a manifest file (-m) to create "
		    "an archive\n");
		exit(127);
	}

	if (is_extract) {
		(void) cpio_archive_extract(base_directory, archive_file,
		    ! is_list, block_size);
	} else if (is_create) {
		(void) cpio_archive_output_create(base_directory,
		    manifest_file, archive_file, block_size);
	} else {
		fprintf(stderr, "ERROR: invalid internal state; need either "
		    "create or extract\n");
		exit(127);
	}

	exit (0);
}
