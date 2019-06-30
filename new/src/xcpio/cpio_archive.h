#ifndef	__CPIO_ARCHIVE_H__
#define	__CPIO_ARCHIVE_H__

#define	XCPIO_WRITE_BUF_SIZE	1024
#define	XCPIO_READ_BUF_SIZE	1024

typedef enum {
	CPIO_ARCHIVE_MODE_NONE,
	CPIO_ARCHIVE_MODE_READ,
	CPIO_ARCHIVE_MODE_WRITE,
} cpio_archive_mode;

struct cpio_archive {
	char *archive_filename;
	int fd;
	cpio_archive_mode mode;

	struct {
		char *dirname;
		int fd;
	} base;

	struct {
		struct cpio_header *c;
		size_t consumed_bytes;
	} read;

	struct {
		struct file_list *fl;
	} files;
};

extern	struct cpio_archive * cpio_archive_create(const char *file, cpio_archive_mode mode);
extern	int cpio_archive_open(struct cpio_archive *a);
extern	int cpio_archive_close(struct cpio_archive *a);
extern	int cpio_archive_free(struct cpio_archive *a);
extern	int cpio_archive_write_file(struct cpio_archive *a, const char *filename);
extern	int cpio_archive_write_files(struct cpio_archive *a);
extern	int cpio_archive_write_add_manifest(struct cpio_archive *a, const char *filename);
extern	int cpio_archive_begin_read(struct cpio_archive *a, bool do_extract);
extern	int cpio_archive_set_base_directory(struct cpio_archive *a, const char *dir);

#endif	/* _CPIO_ARCHIVE_H__ */
