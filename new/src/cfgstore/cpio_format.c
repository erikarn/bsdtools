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
 * Finally, the final file in the archive is an empty file named TRAILER!!! .
 */

static struct cpio_header *
cpio_header_allocate(void)
{
	struct cpio_header *c;

	c = calloc(1, sizeof(*c));
	if (c == NULL) {
		warn("%s: calloc", __func__);
		return (NULL);
	}
	return (c);
}

/*
 * Create a cpio_header with the given filename and stat.
 */
struct cpio_header *
cpio_header_create(struct stat *st, const char *fn)
{
	struct cpio_header *c;

	c = cpio_header_allocate();
	if (c == NULL)
		return (NULL);

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
	/*
	 * This is more annoying than it should be.
	 *
	 * Each of these numerical fields needs to be 6 octal digits long, save the 11 digit ones.
	 * But, the width isn't limiting the maximum value.
	 *
	 * So, let's cheat. 6 octal digits is 6*3 = 18 bits long. 11*3 = 33 bits long.
	 */
	len = snprintf(buf, 256, "%6.6llo%6.6llo%6.6llo%6.6o%6.6o%6.6o%6.6o%6.6o%11.11llo%6.6llo%11.11llo",
		(unsigned long long)070707,
		(unsigned long long)(c->st.st_dev & 0x3ffff),
		(unsigned long long)(c->st.st_ino & 0x3ffff),
		c->st.st_mode & 0x3ffff,
		c->st.st_uid & 0x3ffff,
		c->st.st_gid & 0x3ffff,
		c->st.st_nlink & 0x3ffff,
		c->st.st_rdev & 0x3ffff,
		((unsigned long long) (c->st.st_mtime)) & 0x1ffffffffULL,
		((unsigned long long) strlen(c->filename) + 1) & 0x3ffff,
		((unsigned long long) (c->st.st_size)) & 0x1ffffffffULL);

	/*
	 * We should error out if the length doesn't exactly match the
	 * expected header size, when we DO have that header size..
	 */
	if (len != (6+6+6+6+6+6+6+6+11+6+11)) {
		warn("%s: snprintf (%d bytes)", __func__, len);
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
	/* Now write the filename + trailing NUL; it's part of the header */
	r = write(fd, c->filename, strlen(c->filename) + 1);
	if (r == 0) {
		return (0);
	}
	if (r != strlen(c->filename) + 1) {
		warn("%s: short write", __func__);
		return (-1);
	}
	return (1);
}

/*
 * Parse the given input stream buffer and populate a cpio_header
 * struct.
 *
 * Returns -1 on error, 0 on "not enough data", and a positive number
 * indiciating the header size (ie, what to skip to get to the file contenst)
 * + a cpio_header if the entire header and filename was read.
 */
int
cpio_header_deserialise(const char *buf, int len,
	    struct cpio_header **hdr)
{
	struct cpio_header *h = NULL;
	char pbuf[32];
	unsigned long long n;
	int filename_len;

	/* We need at least this many bytes for a CPIO header */
	if (len < 76) {
		return (0);
	}

	/*
	 * This is a bit more annoying than serialising it.
	 * To be paranoid let's create temporary staging strings
	 * for each value, then strtoull() to convert it to the
	 * octal value.
	 */
	h = cpio_header_allocate();
	if (h == NULL) {
		return (-1);
	}

	/* magic */
	memcpy(pbuf, buf + 0, 6); pbuf[6] = '\0';
	n = strtoull(pbuf, NULL, 8);
	if (n != 070707) {
		fprintf(stderr, "%s; bad magic\n", __func__);
		goto fail;
	}

	/* dev */
	memcpy(pbuf, buf + 6, 6); pbuf[6] = '\0';
	n = strtoull(pbuf, NULL, 8);
	h->st.st_dev = n;

	/* inode */
	memcpy(pbuf, buf + 12, 6); pbuf[6] = '\0';
	n = strtoull(pbuf, NULL, 8);
	h->st.st_ino = n;

	/* mode */
	memcpy(pbuf, buf + 18, 6); pbuf[6] = '\0';
	n = strtoull(pbuf, NULL, 8);
	h->st.st_mode = n;

	/* uid */
	memcpy(pbuf, buf + 24, 6); pbuf[6] = '\0';
	n = strtoull(pbuf, NULL, 8);
	h->st.st_mode = n;

	/* gid */
	memcpy(pbuf, buf + 30, 6); pbuf[6] = '\0';
	n = strtoull(pbuf, NULL, 8);
	h->st.st_mode = n;

	/* nlink */
	memcpy(pbuf, buf + 36, 6); pbuf[6] = '\0';
	n = strtoull(pbuf, NULL, 8);
	h->st.st_mode = n;

	/* rdev */
	memcpy(pbuf, buf + 42, 6); pbuf[6] = '\0';
	n = strtoull(pbuf, NULL, 8);
	h->st.st_mode = n;

	/* mtime - 11 */
	memcpy(pbuf, buf + 48, 11); pbuf[11] = '\0';
	n = strtoull(pbuf, NULL, 8);
	h->st.st_mtime = n;

	/* filename length */
	memcpy(pbuf, buf + 59, 6); pbuf[6] = '\0';
	n = strtoull(pbuf, NULL, 8);
	filename_len = n;

	/* file length - 11 */
	memcpy(pbuf, buf + 65, 11); pbuf[11] = '\0';
	h->st.st_size = strtoull(pbuf, NULL, 8);

	/*
	 * Check that we have enough bytes for the filename size
	 * that was provided.  If not then we need more data.
	 */
	if (len < 76 + filename_len) {
		cpio_header_free(h);
		return (0);
	}

	/*
	 * Ok, our temporary cpio_header has all the bits.
	 * Return it and how many bytes we consumed.
	 */
	h->filename = strndup(buf + 76, filename_len);
	if (h->filename == NULL) {
		warn("%s: strndup (%d bytes)", __func__, filename_len);
		goto fail;
	}

	/*
	 * Paranoia!
	 */
	h->filename[filename_len] = '\0';

	*hdr = h;

	/*
	 * And finally, how much data to skip!
	 */
	return (76 + filename_len);

fail:
	if (h) {
		cpio_header_free(h);
	}
	return (-1);
}
