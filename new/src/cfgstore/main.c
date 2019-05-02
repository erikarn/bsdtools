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

int
archive_open(void)
{
}

int
archive_a_file(const char *filename)
{
	struct cpio_header *c = NULL;
	struct stat sb;
	int fd = -1;
	int ret;
	ssize_t rret, wret, wlen;
	char buf[1024];

	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		warn("open (%s)", filename);
		goto fail;
	}

	ret = fstat(fd, &sb);
	if (ret != 0) {
		warn("stat (%s)", filename);
		goto fail;
	}

	c = cpio_header_create(&sb, filename);
	if (c == NULL) {
		goto fail;
	}

	(void) cpio_header_serialise(STDOUT_FILENO, c);

	/*
	 * Yeah yeah 1k read/write is tiny, but for this use case it's
	 * fine.
	 */
	while (1) {
		rret = read(fd, buf, 1024);
		if (rret == 0) {
			break;
		}
		if (rret < 0) {
			warn("read");
			goto fail;
		}
		wlen = 0;
		while (wlen < rret) {
			wret = write(STDOUT_FILENO, buf + wlen, rret - wlen);
			if (wret == 0) {
				warn("write");
				goto fail;
			}
			wlen += wret;
		}
	}

	close(fd);
	cpio_header_free(c);
	return (0);

fail:
	if (c)
		cpio_header_free(c);
	if (fd != -1)
		close(fd);
	return (-1);
}

int
archive_end(void)
{
	struct cpio_header *c;
	struct stat sb;
	int ret;

	bzero(&sb, sizeof(sb));
	c = cpio_header_create(&sb, "TRAILER!!!");
	if (c == NULL) {
		return (-1);
	}

	ret = cpio_header_serialise(STDOUT_FILENO, c);
	if (ret < 1) {
		cpio_header_free(c);
		return (-1);
	}
	cpio_header_free(c);
	return (0);

}

int
main(int argc, const char *argv[])
{

	archive_open();
	archive_a_file("main.c");
	archive_end();

	exit (0);
}
