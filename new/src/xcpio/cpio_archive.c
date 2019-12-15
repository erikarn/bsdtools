#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
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

	a->files.fl = file_list_create();
	if (a->files.fl == NULL) {
		free(a);
		return (NULL);
	}
	a->archive_filename = strdup(file);
	a->mode = mode;
	a->fd = -1;

	a->base.fd = AT_FDCWD;

	a->block_size = DEFAULT_CPIO_BLOCK_SIZE;

	return a;
}

int
cpio_archive_set_blocksize(struct cpio_archive *a, int block_size)
{
	if (block_size <= 0) {
		return -1;
	}
	a->block_size = block_size;
	return 0;
}

/*
 * Attempt to flush out whatever is in the write buffer.
 *
 * If do_padding is true then pad it out to write a given block size.
 * Then, consume the data in the buffer.
 *
 * If do_padding is false then don't write if the buffer isn't full.
 */
static int
cpio_archive_write_flush(struct cpio_archive *a, int do_padding)
{
	ssize_t ret;

	if (do_padding) {
		/* Make sure the rest of the buffer is zeroed */
		memset(a->write.buf + a->write.len, 0,
		    a->write.size - a->write.len);
	} else {
		/* Don't run if we aren't full */
		if (a->write.size != a->write.len) {
			return (0);
		}
	}

	/* Write it out; error out if it's not the whole thing */
	ret = write(a->fd, a->write.buf, a->write.size);
	if (ret < 0) {
		warn("%s: write failed", __func__);
		return (-1);
	}
	/*
	 * Note: We can't handle partial writes here.
	 */
	if (ret != a->write.size) {
		fprintf(stderr, "%s: partial write (%d, not %d bytes)\n",
		  __func__,
		  (int) ret,
		  a->write.size);
		return (-1);
	}

	/* Consume everything */
	a->write.len = 0;

	return (a->write.size);
}

/*
 * Copy data into the staging write buffer and once a full write
 * size worth of data is available, write it out.  Loop over until
 * everything is written that can be written.
 */
static int
cpio_archive_write_data(struct cpio_archive *a, const char *write_buf,
    int write_len)
{
	int bytes_written = 0;
	int copy_len;
	ssize_t ret;

	do {
		/*
		 * Step 1 - see if anything is in the write buffer.
		 * If it's full then make sure we write it out here.
		 * This will do nothing in the buffer isn't full.
		 */
		if (cpio_archive_write_flush(a, false) < 0) {
			printf("failed to flush\n");
			return (-1);
		}

		/*
		 * Step 1.5 - don't continue if we're out of incoming
		 * data.
		 */
		if (write_len == 0) {
			break;
		}

		/*
		 * Step 2 - copy over data from the incoming buffer,
		 * until the write buffer is full OR we're out of
		 * write buffer data.
		 */
		copy_len = MIN(write_len, a->write.size - a->write.len);
		memcpy(a->write.buf + a->write.len, write_buf, copy_len);
		a->write.len += copy_len;

		/*
		 * Step 3 - consume the data from the incoming buffer.
		 */
		write_len -= copy_len;
		write_buf += copy_len;

		/*
		 * Step 4 - track what we've consumed
		 */
		bytes_written += copy_len;

		/*
		 * Loop over again!  This will keep looping over until
		 * we can't fill the write buffer.
		 */
	} while (a->write.len == a->write.size);

	return bytes_written;
}

int
cpio_archive_open(struct cpio_archive *a)
{
	if (a->block_size == 0) {
		fprintf(stderr, "%s: ERROR: block_size must not be 0\n",
		    __func__);
		return -1;
	}
	switch (a->mode) {
	case CPIO_ARCHIVE_MODE_READ:
		a->fd = open(a->archive_filename, O_RDONLY);
		break;
	case CPIO_ARCHIVE_MODE_WRITE:
		a->fd = open(a->archive_filename, O_WRONLY | O_CREAT, 0644);
		break;
	default:
		a->fd = -1;
		return -1;
	}

	if (a->fd < 0) {
		warn("%s: open (%s)", __func__, a->archive_filename);
		return -1;
	}

	a->write.buf = calloc(1, a->block_size);
	if (a->write.buf == NULL) {
		warn("%s: calloc(%d)", __func__, a->block_size);
		return -1;
	}
	a->write.size = a->block_size;
	a->write.len = 0;
	return 0;
}

int
cpio_archive_close(struct cpio_archive *a)
{
	struct cpio_header *c;
	struct stat sb;
	int ret;

	if (a->mode == CPIO_ARCHIVE_MODE_WRITE) {
		char *sbuf;
		int slen;
		bzero(&sb, sizeof(sb));
		c = cpio_header_create(&sb, "TRAILER!!!");
		if (c == NULL) {
			return (-1);
		}

		sbuf = cpio_header_serialise(a->fd, c, &slen);
		if (sbuf == NULL) {
			cpio_header_free(c);
			return (-1);
		}
		cpio_archive_write_data(a, sbuf, slen);
		free(sbuf); sbuf = NULL;
		cpio_header_free(c);

		/*
		 * Flush out any pending data; make sure it's padded.
		 */
		cpio_archive_write_flush(a, true);

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
	file_list_free(a->files.fl);
	a->files.fl = NULL;
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
	/* XXX TODO: make this 4x the block size.. */
	char buf[XCPIO_WRITE_BUF_SIZE];
	char *sbuf;
	int slen;

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

	sbuf = cpio_header_serialise(a->fd, c, &slen);
	if (sbuf == NULL) {
		goto fail;
	}
	if (cpio_archive_write_data(a, sbuf, slen) <= 0) {
		free(sbuf);
		goto fail;
	}
	free(sbuf); sbuf = NULL;

	/*
	 * Yeah yeah 1k read/write is tiny, but for this use case it's
	 * fine.
	 */
	while (1) {
		/* Note: this reads from the file we opened */
		rret = read(fd, buf, XCPIO_WRITE_BUF_SIZE);
		if (rret == 0) {
			break;
		}
		if (rret < 0) {
			warn("read");
			goto fail;
		}
		wlen = 0;
		while (wlen < rret) {
			/*
			 * Note: this writes to the underlying device
			 * and THIS must be block sized writes!
			 */
			wret = cpio_archive_write_data(a, buf + wlen, rret - wlen);
			if (wret <= 0) {
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

	return (file_list_add_entry(a->files.fl, filename));
}

/*
 * Write the files to the current archive.  This iterates over the file list
 * but does not flush/empty it.
 */
int
cpio_archive_write_files(struct cpio_archive *a)
{
	int i;

	/* XXX TODO: iterator */

	for (i = 0; i < a->files.fl->nentries; i++) {
		/*
		 * For now don't error out if we fail to write a file;
		 * just log a warning and continue.
		 */
		if (cpio_archive_write_file(a, a->files.fl->file_list[i]) != 0) {
			fprintf(stderr, "%s: failed to write file (%s)\n",
			    __func__,
			    a->files.fl->file_list[i]);
		}
	}

	return (0);
}

/*
 * Begin reading from an archive.
 */
int
cpio_archive_begin_read(struct cpio_archive *a, bool do_extract)
{
	char *buf;
	int buf_len = 0;
	int buf_size = a->block_size * 4;
	ssize_t r;
	size_t cs, cr;
	int rr, retval = 0;
	int target_fd = -1;

	/*
	 * Note: the buf size is greater than block_size so
	 * we can read in block_size reads and potentially
	 * read over the block boundary whilst accumulating
	 * data like header info.
	 *
	 * Yes, the important thing here is that stuff like
	 * the individual files in the archive aren't aligned
	 * to block sizes; only the actual archive itself
	 * is block size aligned.
	 *
	 * Hopefully (!) nothing dumb like file names will
	 * cause the header parsing to exceed 4x the block size.
	 */
	buf = calloc(1, buf_size);
	if (buf == NULL) {
		warn("%s: calloc(%d)", __func__, buf_size);
		return (-1);
	}

	while (1) {

		/* Consume data if we have space */
		if (buf_len < (buf_size - a->block_size)) {
			/*
			 * This is where we need to make sure
			 * that the blocks are read in a fixed block size,
			 * rather than "always fill the buffer."
			 */
			r = read(a->fd, buf + buf_len, a->block_size);
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
			if (rr == 0 && buf_len < buf_size) {
				/* Not enough data left in the buffer */
				continue;
			}
			/*
			 * Check if we've got our header before we ran out
			 * of incoming space.
			 *
			 * Importantly, we need to make sure that we can't
			 * read any FURTHER data, rather than it's full.
			 */
			if (rr == 0 && buf_len > (buf_size - a->block_size)) {
				/*
				 * No header parsable yet? Bail out, someone's
				 * playing bad games!
				 */
				printf("%s: failed; didn't complete header/filename within %d bytes\n",
				    __func__, buf_size);
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

			/*
			 * Note: this logic ONLY handles creating files for
			 * now.
			 * This needs to be extended to handle block/char
			 * devices, symlinks and hardlinks.
			 */

			/* attempt to open a file to write to */
			if (do_extract) {
				tmp_fn = cpio_path_sanity_filter(a->read.c->filename);
				if (tmp_fn != NULL) {
					/* XXX TODO: mode, owner, ctime/mtime, device id, etc */
					target_fd = openat(a->base.fd, tmp_fn, O_WRONLY | O_CREAT,
					    a->read.c->st.st_mode);
					if (target_fd < 0) {
						warn("%s: openat() (%s)", __func__,
						  tmp_fn);
						target_fd = -1;
					}

					/*
					 * Set the file ownership.  For now don't warn;
					 * it'll fail if you're non-root.
					 */
					if ((target_fd >= 0) &&
					    (fchown(target_fd, a->read.c->st.st_uid,
					    a->read.c->st.st_gid) != 0)) {
#if 0
						warn("%s: fchown (%s) (%llu/%llu)",
						    __func__,
						    a->read.c->filename,
						    (unsigned long long) a->read.c->st.st_uid,
						    (unsigned long long) a->read.c->st.st_gid);
#endif
					}
					free(tmp_fn);
				}
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
			/*
			 * Note: this is the write to the target file.
			 *
			 * Ideally this would also be buffered in memory and
			 * do larger writes for efficiency.
			 */
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
	if (buf != NULL) {
		free(buf);
	}

	return retval;
}
