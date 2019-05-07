#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <strings.h>
#include <err.h>
#include <fcntl.h>

#include <sys/param.h>
#include <sys/stat.h>

#include "file_list.h"
#include "cpio_format.h"
#include "cpio_archive.h"

/*
 * sanity check the file name.  This involves stripping
 * out any leading ../ or ./ or / in the filename.
 *
 * Yes, this is a big, big todo.
 */
static char *
cpio_path_sanity_filter(const char *src)
{
	/* XXX BIG TODO HERE! */
	return strdup(src);
}


struct cpio_archive *
cpio_archive_create(const char *file, cpio_archive_mode mode)
{
	struct cpio_archive *a;

	a = calloc(1, sizeof(*a));
	if (a == NULL) {
		warn("%s: calloc", __func__);
		return NULL;
	}
	a->archive_filename = strdup(file);
	a->mode = mode;
	a->fd = -1;

	a->base.fd = AT_FDCWD;

	return a;
}

int
cpio_archive_open(struct cpio_archive *a)
{
	switch (a->mode) {
	case CPIO_ARCHIVE_MODE_READ:
		a->fd = open(a->archive_filename, O_RDONLY);
		break;
	case CPIO_ARCHIVE_MODE_WRITE:
		a->fd = open(a->archive_filename, O_WRONLY | O_CREAT);
		break;
	default:
		a->fd = -1;
		return -1;
	}

	if (a->fd < 0) {
		warn("%s: open (%s)", __func__, a->archive_filename);
		return -1;
	}
	return 0;
}

int
cpio_archive_close(struct cpio_archive *a)
{
	struct cpio_header *c;
	struct stat sb;
	int ret;

	if (a->mode == CPIO_ARCHIVE_MODE_WRITE) {
		bzero(&sb, sizeof(sb));
		c = cpio_header_create(&sb, "TRAILER!!!");
		if (c == NULL) {
			return (-1);
		}

		ret = cpio_header_serialise(a->fd, c);
		if (ret < 1) {
			cpio_header_free(c);
			return (-1);
		}
		cpio_header_free(c);

		close(a->fd);
		a->fd = -1;
		return (0);
	}

	/* For read we're not doing anything else */

	return (0);
}

int
cpio_archive_free(struct cpio_archive *a)
{

	if (a->fd > -1)
		close(a->fd);
	if (a->base.fd > -1)
		close(a->base.fd);
	free(a->archive_filename);
	free(a->base.dirname);
	if (a->read.c != NULL)
		cpio_header_free(a->read.c);
	free(a);
	return (0);
}

int
cpio_archive_set_base_directory(struct cpio_archive *a, const char *dirname)
{
	if (a->base.fd > -1)
		close(a->base.fd);
	if (a->base.dirname)
		free(a->base.dirname);
	a->base.fd = -1;
	a->base.dirname = NULL;

	a->base.dirname = strdup(dirname);
	if (a->base.dirname == NULL) {
		warn("%s: strdup", __func__);
		return (-1);
	}
	a->base.fd = open(dirname, O_RDONLY);
	if (a->base.fd == -1) {
		warn("%s: open (%s)", __func__, dirname);
		return (-1);
	}
	return (0);
}

/*
 * Write a file into the current archive. filename is either a
 * full path or a relative to the defined base path / current working
 * directory.
 */
int
cpio_archive_write_file(struct cpio_archive *a, const char *filename)
{
	struct cpio_header *c = NULL;
	struct stat sb;
	int fd = -1;
	int ret;
	ssize_t rret, wret, wlen;
	char buf[1024];

	fd = openat(a->base.fd, filename, O_RDONLY);
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

	(void) cpio_header_serialise(a->fd, c);

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
			wret = write(a->fd, buf + wlen, rret - wlen);
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
cpio_archive_write_add_manifest(struct cpio_archive *a, const char *filename)
{

	/* XXX TODO */
	return -1;
}

/*
 * Begin reading from an archive.  For now just read through the archive
 * and print the headers, complete with validation.  This'll at least tell
 * me that my basic parsing logic is correct.
 */

int
cpio_archive_begin_read(struct cpio_archive *a)
{
	char buf[1024];
	int buf_len = 0;
	ssize_t r;
	size_t cs, cr;
	int rr, retval = 0;
	int target_fd = -1;

	while (1) {

		/* Consume data if we have space */
		if (buf_len < 1024) {
			r = read(a->fd, buf + buf_len, 1024 - buf_len);
			/* Note: EOF is a problem if we are expecting another file. */
			if (r == 0) {
				retval = -1;
				break;
			}
			/* Note: error is a problem */
			if (r < 0) {
				warn("%s: read", __func__);
				retval = -1;
				break;
			}
		}

		buf_len += r;

		if (a->read.c == NULL) {
			char *tmp_fn;

			/*
			 * We don't yet have a header; attempt to parse a header.
			 */
			rr = cpio_header_deserialise(buf, buf_len, &a->read.c);
			if (rr < 0) {
				break;
			}
			if (rr == 0 && buf_len < 1024) {
				/* Not enough data left in the buffer */
				continue;
			}
			if (rr == 0 && buf_len == 1024) {
				/* No header parsable yet? Bail out, someone's playing bad games */
				printf("%s: failed; didn't complete header/filename within 1024 bytes\n", __func__);
				retval = -1;
				break;
			}

			/*
			 * If we have a header then 'rr' is now how many bytes
			 * to consume and the st_size in our header is how many
			 * bytes to consume for the file contents.
			 */
			printf("%s: parsed; got filename %s\n", __func__,
			    a->read.c->filename);

			/* consume the header */
			memmove(buf, buf + rr, buf_len - rr);
			buf_len -= rr;

			a->read.consumed_bytes = 0;

			/* Check for end of archive marker */
			if ((a->read.c->st.st_size == 0) &&
			    (strncmp(a->read.c->filename, "TRAILER!!!", 10)
			      == 0)) {
				/* We're done! */
				break;
			}

			/* attempt to open a file */
			tmp_fn = cpio_path_sanity_filter(a->read.c->filename);
			if (tmp_fn != NULL) {
				/* XXX TODO: mode, owner, ctime/mtime, device id, etc */
				target_fd = openat(a->base.fd, tmp_fn, O_WRONLY | O_CREAT);
				if (target_fd < 0) {
					warn("%s: openat() (%s)", __func__,
					  tmp_fn);
					target_fd = -1;
				}
				free(tmp_fn);
			}

		}

		/*
		 * If we have a header here then attempt to consume bytes up
		 * until our read limit.  See how much data we have left to
		 * consume and only consume up to THAT from the input buffer.
		 */
		cs = a->read.c->st.st_size - a->read.consumed_bytes;
		cr = MIN(cs, buf_len);

		/*
		 * Here is where we could write this to the destination file.
		 * Again, I'm going to be cheap and not loop over the buffer
		 * until it's written; I want to get the rest of this fleshed
		 * out before I worry about better IO pipelines.
		 */
		if (target_fd != -1) {
			ssize_t wr;
			wr = write(target_fd, buf, cr);
			if (wr != cr) {
				fprintf(stderr, "%s: write size mismatch to "
				  "destination file (%s) - wanted %llu bytes, "
				  "wrote %llu bytes\n",
				  __func__,
				  a->read.c->filename,
				  (unsigned long long) cr,
				  (unsigned long long) wr);

				/*
				 * Close target_fd; we'll just consume but
				 * not write the rest of this file contents
				 * out.
				 */
				close(target_fd);
				target_fd = -1;
			}
		}


		/* Consume data */
		memmove(buf, buf + cr, buf_len - cr);
		buf_len -= cr;
		a->read.consumed_bytes += cr;

		/*
		 * If we've hit the end then close this file, free the header.
		 */
		if (a->read.c != NULL && a->read.consumed_bytes == a->read.c->st.st_size) {
			/* close the destination file */
			printf("closing %s\n", a->read.c->filename);
			/* close the state */
			cpio_header_free(a->read.c);
			a->read.c = NULL;
			if (target_fd != -1) {
				close(target_fd);
				target_fd = -1;
			}
		}
	}

	/* Final cleanup */
	if (a->read.c != NULL) {
		cpio_header_free(a->read.c);
		a->read.c = NULL;
	}
	if (target_fd != -1) {
		close(target_fd);
		target_fd = -1;
	}

	return retval;
}