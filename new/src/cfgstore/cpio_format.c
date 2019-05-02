#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <err.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "cpio_format.h"

/*
 * CPIO archives are pretty simple.
 *
 * 6	magic	magic number "070707"
 * 6	dev	device where file resides
 * 6	ino	I-number of file
 * 6	mode	file mode
 * 6	uid	owner user ID
 * 6	gid	owner group ID
 * 6	nlink	number of links to file
 * 6	rdev	device major/minor for special file
 * 11	mtime	modify time of file
 * 6	namesize	length of file name
 * 11	filesize	length of file to follow
 *
 * Finally, the final file in the archive is an empty file named TRAILER!! .
 */

/*
 * Create a cpio_header with the given filename and stat.
 */
struct cpio_header *
cpio_header_create(struct stat *st, const char *fn)
{
	struct cpio_header *c;

	c = calloc(1, sizeof(*c));
	if (c == NULL) {
		warn("%s: calloc", __func__);
		return (NULL);
	}
	memcpy(&c->st, st, sizeof(struct stat));
	c->filename = strdup(fn);
	return (c);
}

/*
 * Free the given cpio_header struct.
 */
void
cpio_header_free(struct cpio_header *c)
{
	free(c->filename);
	free(c);
}

/*
 * Write the given struct stat header to the output stream.
 *
 * Returns 1 if OK, 0 if EOF, -1 if error.
 */
int
cpio_header_serialise(int fd, struct cpio_header *c)
{
	char buf[256];
	int len;
	ssize_t r;

	if (c->filename == NULL) {
		return (-1);
	}
	/* For now, just printf something for debugging */
	len = snprintf(buf, 256, "%llu %llu %llu %d %d %d %d %d %llu %llu %llu",
		(unsigned long long)070707,
		(unsigned long long)c->st.st_dev,
		(unsigned long long)c->st.st_ino,
		c->st.st_mode,
		c->st.st_uid,
		c->st.st_gid,
		c->st.st_nlink,
		c->st.st_rdev,
		(unsigned long long) (c->st.st_mtime),
		(unsigned long long) strlen(c->filename),
		(unsigned long long) c->st.st_size);

	/*
	 * We should error out if the length doesn't exactly match the
	 * expected header size, when we DO have that header size..
	 */
	if (len < 0) {
		warn("%s: snprintf", __func__);
		return (-1);
	}
	r = write(fd, buf, len);
	/* TODO: handle EINTR? */
	if (r == 0) {
		return (0);
	}
	if (r != len) {
		warn("%s; short write", __func__);
		return (-1);
	}
	return (1);
}

/*
 * Parse the given input stream buffer and populate a cpio_header
 * struct.
 *
 * Returns -1 on error, 0 on "not enough data", and a positive number
 * + a cpio_header if the entire header and filename was read.
 */
int cpio_header_deserialise(const char *buf, int len,
	    struct cpio_header **hdr)
{
	return (-1);
}
