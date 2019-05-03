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

int
main(int argc, const char *argv[])
{
	struct cpio_archive *a;

#if 0
	a = cpio_archive_create("/dev/stdout", CPIO_ARCHIVE_MODE_WRITE);
	cpio_archive_open(a);
	cpio_archive_write_file(a, "main.c");
	cpio_archive_close(a);
	cpio_archive_free(a);
#endif
#if 1
	a = cpio_archive_create("/dev/stdin", CPIO_ARCHIVE_MODE_READ);
	cpio_archive_open(a);
	cpio_archive_begin_read(a);
	cpio_archive_close(a);
	cpio_archive_free(a);
#endif


	exit (0);
}
