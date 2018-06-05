/*
 * SquashFS filesystem implementation for U-Boot
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __U_BOOT_SQUASHFS_H__
#define __U_BOOT_SQUASHFS_H__

struct fs_dir_stream;
struct fs_dirent;

int squashfs_probe(struct blk_desc *, disk_partition_t *);
int squashfs_exists(const char *);
int squashfs_size(const char *, loff_t *);
int squashfs_read(const char *, void *, loff_t, loff_t, loff_t *);
void squashfs_close(void);
int squashfs_opendir(const char *, struct fs_dir_stream **);
int squashfs_readdir(struct fs_dir_stream *, struct fs_dirent **);
void squashfs_closedir(struct fs_dir_stream *);

#endif /* __U_BOOT_SQUASHFS_H__ */
