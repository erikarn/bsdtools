#ifndef	__CPIO_FORMAT_H__
#define	__CPIO_FORMAT_H__

struct cpio_header {
	struct stat st;
	char *filename;
};

/*
 * Create a cpio_header with the given filename and stat.
 */
extern	struct cpio_header * cpio_header_create(struct stat *st, const char *fn);

/*
 * Free the given cpio_header struct.
 */
extern	void cpio_header_free(struct cpio_header *);

/*
 * Write the given struct stat header to the output stream.
 *
 * Returns 1 if OK, 0 if EOF, -1 if error.
 */
extern	int cpio_header_serialise(int fd, struct cpio_header *c);

/*
 * Parse the given input stream buffer and populate a cpio_header
 * struct.
 *
 * Returns -1 on error, 0 on "not enough data", and a positive number
 * + a cpio_header if the entire header and filename was read.
 */
extern	int cpio_header_deserialise(const char *buf, int len,
	    struct cpio_header **hdr);


#endif	/* __CPIO_FORMAT_H__ */
