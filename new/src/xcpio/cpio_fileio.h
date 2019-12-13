#ifndef	__CPIO_FILEIO_H__
#define	__CPIO_FILEIO_H__

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

/*
 * Initialise the fileio layer.
 */
extern	void cpio_fileio_init(void);

/*
 * Create a CPIO file handle.
 */
extern	struct cpio_filehandle * cpio_fileio_create(void);

/*
 * Set the file handle path for doing IO.
 */
extern	int cpio_fileio_set_path(struct cpio_filehandle *, const char *);

/*
 * Open the file given the provided configuration.
 */
extern	int cpio_fileio_open(struct cpio_filehandle *);

/*
 * Open the file given the provided configuration and openat FD.
 */
extern	int cpio_fileio_openat(struct cpio_filehandle *, int);

/*
 * Close the given CPIO file.  Thus flushes any pending
 * IO and will close the handle.
 *
 * Note that it doesn't free the file handle; so it can be
 * reused for opening again.
 */
extern	int cpio_fileio_close(struct cpio_filehandle *);

/*
 * Free the given CPIO file handle.  If it isn't closed then
 * it will first be flushed and closed.
 */
extern	void cpio_fileio_free_handle(struct cpio_filehandle *);

/*
 * Set the underlying block size for reads and writes.
 *
 * This library is used to ensure that reads and writes are performed
 * in multiples of the underlying block size.
 */
extern	int cpio_fileio_set_block_size(struct cpio_filehandle *, size_t);

/*
 * Set the buffer size for reads and writes.
 *
 * This must be a multiple of the block size.
 */
extern	int cpio_fileio_set_buffer_size(struct cpio_filehandle *, size_t);

/*
 * Flush data out to the underlying filehandle, up to the point of
 * having not enough data to fill a block to write.
 *
 * This will return how much data is left in the write buffer.
 */
extern	int cpio_fileio_flush(struct cpio_filehandle *);

/*
 * Flush data out to the underlying filehandle and zero-pad
 * underlying data.
 *
 * This will return how much data was written, including the
 * zero-padded data.
 */
extern	int cpio_fileio_flush_all(struct cpio_filehandle *);

/*
 * Read data from the file handle, ensuring the underlying file reads
 * are multiples of the given block size.
 *
 * This will either returned pre-buffered read data or will read a multiple
 * of the underlying block size and then return enough data to satisfy the
 * caller.
 */
extern	ssize_t cpio_fileio_read(struct cpio_filehandle *, char *, ssize_t);

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
extern	ssize_t cpio_fileio_write(struct cpio_filehandle *, char *, ssize_t);

#endif	/* __CPIO_FILEIO_H__ */
