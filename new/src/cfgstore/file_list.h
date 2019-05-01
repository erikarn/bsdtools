#ifndef	__FILE_LSIT_H__
#define	__FILE_LSIT_H__

struct file_list {
	int nentries;
	int nsize;
	char **file_list;
};

extern	struct file_list * file_list_create(void);
extern	void file_list_free(struct file_list *);
extern	void file_list_flush(struct file_list *);
extern	int file_list_add_entry(struct file_list *, const char *);
/* For now, hard-code iteration; will replace with an iterator function later */

#endif
