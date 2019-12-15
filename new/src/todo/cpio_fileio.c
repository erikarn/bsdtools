
#if 0
struct cpio_filehandle {
	int fd;
	char *filename;
	int openat_fd;
	size_t block_size;
	size_t buffer_size;
	struct {
		char *buf;
		int len;
		int offset;
	} read_buffer;
	struct {
		char *buf;
		int len;
		int offset;
	} write_buffer;
};
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <err.h>

/*
 * Initialise the fileio layer.
 */
void
cpio_fileio_init(void)
{
}

/*
 * Create a CPIO file handle.
 */
struct cpio_filehandle *
cpio_fileio_create(void)
{
	struct cpio_filehandle *fh;

	fh = calloc(1, sizeof(struct cpio_filehandle));
	if (fh == NULL) {
		warn("%s: calloc", __func__);
		return NULL;
	}

	fh->openat_fd = -1;
	fh->fd = -1;
	return fh;
}

/*
 * Set the file handle path for doing IO.
 */
int
cpio_fileio_set_path(struct cpio_filehandle *fh, const char *filename)
{
}

/*
 * Open the file given the provided configuration.
 */
int
cpio_fileio_open(struct cpio_filehandle *fh)
{
}

/*
 * Open the file given the provided configuration and openat FD.
 */
int
cpio_fileio_openat(struct cpio_filehandle *fh, int openat_fd)
{
}

/*
 * Close the given CPIO file.  Thus flushes any pending
 * IO and will close the handle.
 *
 * Note that it doesn't free the file handle; so it can be
 * reused for opening again.
 */
int
cpio_fileio_close(struct cpio_filehandle *fh)
{
}

/*
 * Free the given CPIO file handle.  If it isn't closed then
 * it will first be flushed and closed.
 */
void
cpio_fileio_free_handle(struct cpio_filehandle *)
{
}

/*
 * Set the underlying block size for reads and writes.
 *
 * This library is used to ensure that seeks, reads and writes are performed
 * in multiples of the underlying block size.
 */
int
cpio_fileio_set_block_size(struct cpio_filehandle *, size_t)
{
}

/*
 * Set the buffer size for reads and writes.
 *
 * This must be a multiple of the block size.
 */
int
cpio_fileio_set_buffer_size(struct cpio_filehandle *, size_t)
{
}

/*
 * Flush data out to the underlying filehandle, up to the point of
 * having not enough data to fill a block to write.
 *
 * This will return how much data is left in the write buffer.
 */
int
cpio_fileio_flush(struct cpio_filehandle *)
{
}

/*
 * Flush data out to the underlying filehandle and zero-pad
 * underlying data.
 *
 * This will return how much data was written, including the
 * zero-padded data.
 */
int
cpio_fileio_flush_all(struct cpio_filehandle *)
{
}

/*
 * Read data from the file handle, ensuring the underlying file reads
 * are multiples of the given block size.
 *
 * This will either returned pre-buffered read data or will read a multiple
 * of the underlying block size and then return enough data to satisfy the
 * caller.
 */
ssize_t
cpio_fileio_read(struct cpio_filehandle *, char *, ssize_t)
{
}

/*
 * Write data from the file handle, ensuring the underlying file writes
 * are multiples of the given block size.
 *
 * This will buffer data until enough exists to write, and then write
 * multiples of the underlying block size.
 *
 * Note that this isn't cache coherent in /any/ way with the read call.
 * If a consumer requires coherency between read/writes (eg to seek around
 * to do database work) then consumers need to flush data to the kernel
 * via a call to cpio_fileio_flush_all().
 *
 * Bigger note : if there isn't enough buffered data when flush is called,
 * the flush will return a warning.
 */
ssize_t
cpio_fileio_write(struct cpio_filehandle *, char *, ssize_t)
{
}

#endif	/* __CPIO_FILEIO_H__ */
