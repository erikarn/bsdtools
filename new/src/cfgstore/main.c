#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <strings.h>
#include <err.h>
#include <fcntl.h>

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

int
main(int argc, const char *argv[])
{
	struct cpio_archive *a;


#if 1
	a = cpio_archive_create("/dev/stdout", CPIO_ARCHIVE_MODE_WRITE);
	(void) cpio_archive_set_base_directory(a, ".");
	cpio_archive_open(a);
	for (int i = 0; files[i] != NULL; i++) {
		(void) cpio_archive_write_add_manifest(a, files[i]);
	}
	cpio_archive_write_files(a);
	cpio_archive_close(a);
	cpio_archive_free(a);
#else
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
