/*
 * Squashfs - a compressed read only filesystem for Linux
 *
 * Copyright (c) 2002, 2003, 2004, 2005, 2006, 2007, 2008
 * Phillip Lougher <phillip@squashfs.org.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * squashfs.h
 */

#include <linux/printk.h>
#include <fs.h>

struct squashfs_page {
	struct page real_page;
	char **buf;
	int idx;
	int data_block;
};

static inline struct squashfs_page *squashfs_page(struct page *page)
{
	return container_of(page, struct squashfs_page, real_page);
}
#define TRACE(s, args...)	pr_debug("SQUASHFS: "s, ## args)

#define ERROR(s, args...)	pr_err("SQUASHFS error: "s, ## args)

#define WARNING(s, args...)	pr_warn("SQUASHFS: "s, ## args)

struct inode *iget_locked_squashfs(struct super_block *sb, unsigned long ino);
char *squashfs_devread(struct squashfs_sb_info *fs, int address,
        int byte_len);
extern int squashfs_mount(struct super_block *sb, int silent);
extern void squashfs_put_super(struct super_block *sb);

/* block.c */
extern int squashfs_read_data(struct super_block *, u64, int, u64 *,
				struct squashfs_page_actor *);

/* cache.c */
extern struct squashfs_cache *squashfs_cache_init(char *, int, int);
extern void squashfs_cache_delete(struct squashfs_cache *);
extern struct squashfs_cache_entry *squashfs_cache_get(struct super_block *,
				struct squashfs_cache *, u64, int);
extern void squashfs_cache_put(struct squashfs_cache_entry *);
extern int squashfs_copy_data(void *, struct squashfs_cache_entry *, int, int);
extern int squashfs_read_metadata(struct super_block *, void *, u64 *,
				int *, int);
extern struct squashfs_cache_entry *squashfs_get_fragment(struct super_block *,
				u64, int);
extern struct squashfs_cache_entry *squashfs_get_datablock(struct super_block *,
				u64, int);
extern void *squashfs_read_table(struct super_block *, u64, int);

/* decompressor.c */
extern const struct squashfs_decompressor *squashfs_lookup_decompressor(int);
extern void *squashfs_decompressor_setup(struct super_block *, unsigned short);

/* decompressor_xxx.c */
extern void *squashfs_decompressor_create(struct squashfs_sb_info *, void *);
extern void squashfs_decompressor_destroy(struct squashfs_sb_info *);
extern int squashfs_decompress(struct squashfs_sb_info *, char **,
	int, int, int, struct squashfs_page_actor *);
extern int squashfs_max_decompressors(void);

/* fragment.c */
extern int squashfs_frag_lookup(struct super_block *, unsigned int, u64 *);
extern __le64 *squashfs_read_fragment_index_table(struct super_block *,
				u64, u64, unsigned int);

/* file.c */
void squashfs_copy_cache(struct page *, struct squashfs_cache_entry *, int,
				int);
extern int squashfs_readpage(struct file *file, struct page *page);

/* file_xxx.c */
extern int squashfs_readpage_block(struct page *, u64, int);

/* id.c */
extern int squashfs_get_id(struct super_block *, unsigned int, unsigned int *);
extern __le64 *squashfs_read_id_index_table(struct super_block *, u64, u64,
				unsigned short);

/* inode.c */
extern struct inode *squashfs_iget(struct super_block *, long long,
				unsigned int);
extern int squashfs_read_inode(struct inode *, long long);

/*
 * Inodes, files,  decompressor and xattr operations
 */

/* dir.c */
extern const struct file_operations squashfs_dir_ops;

/* export.c */
extern const struct export_operations squashfs_export_ops;

/* file.c */
extern const struct address_space_operations squashfs_aops;

/* inode.c */
extern const struct inode_operations squashfs_inode_ops;

/* namei.c */
extern struct inode *squashfs_lookup(struct inode *dir, const char *cur_name,
				 unsigned int flags);
extern int squashfs_lookup_next(struct inode *dir,
		char *root_name, char *cur_name);

/* symlink.c */
extern const struct address_space_operations squashfs_symlink_aops;
extern const struct inode_operations squashfs_symlink_inode_ops;
