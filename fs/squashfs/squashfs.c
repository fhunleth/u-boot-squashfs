#include <linux/compat.h>

#include <common.h>
#include <blk.h>
#include <config.h>
#include <exports.h>
#include <squashfs.h>
#include <fs.h>
#include <asm/byteorder.h>
#include <part.h>
#include <malloc.h>
#include <memalign.h>
#include <fs_internal.h>

#include "squashfs_compat.h"
#include "squashfs_fs.h"
#include "squashfs_fs_sb.h"
#include "squashfs_fs_i.h"
#include "squashfs.h"

static struct blk_desc *cur_dev;
static disk_partition_t cur_part_info;
static struct super_block sb;

char *squashfs_devread(struct squashfs_sb_info *fs, int address,
		int byte_len)
{
	lbaint_t sector;
	int byte_offset;
	char *buf;

	buf = malloc(byte_len);
	if (buf == NULL)
		return NULL;

	sector = address >> cur_dev->log2blksz;
	byte_offset = address % cur_dev->blksz;

	if (!fs_devread(cur_dev, &cur_part_info, sector, byte_offset,
		byte_len, buf))
		return NULL;

	return buf;
}

static struct inode *duplicate_inode(struct inode *inode)
{
	struct squashfs_inode_info *ei;
	ei = malloc(sizeof(struct squashfs_inode_info));
	if (ei == NULL) {
		ERROR("Error allocating memory for inode\n");
		return NULL;
	}
	memcpy(ei, squashfs_i(inode),
		sizeof(struct squashfs_inode_info));

	return &ei->vfs_inode;
}

static struct inode *squashfs_findfile(struct super_block *sb,
		const char *filename, char *buf)
{
	char *next;
	char fpath[128];
	char *name = fpath;
	struct inode *inode;
	struct inode *t_inode = NULL;

	strcpy(fpath, filename);

	/* Remove all leading slashes */
	while (*name == '/')
		name++;

	inode = duplicate_inode(sb->s_root->d_inode);

	/*
	 * Handle root-directory ('/')
	 */
	if (!name || *name == '\0')
		return inode;

	for (;;) {
		/* Extract the actual part from the pathname.  */
		next = strchr(name, '/');
		if (next) {
			/* Remove all leading slashes.  */
			while (*next == '/')
				*(next++) = '\0';
		}

		t_inode = squashfs_lookup(inode, name, 0);
		if (t_inode == NULL)
			break;

		/*
		 * Check if directory with this name exists
		 */

		/* Found the node!  */
		if (!next || *next == '\0') {
			if (buf != NULL)
				sprintf(buf, "%s", name);

			free(squashfs_i(inode));
			return t_inode;
		}

		name = next;

		free(squashfs_i(inode));
		inode = t_inode;
	}

	free(squashfs_i(inode));
	return NULL;
}

int squashfs_probe(struct blk_desc *dev_desc, disk_partition_t *info)
{
    int ret;

    debug("squashfs_probe\n");

    cur_dev = dev_desc;
    cur_part_info = *info;

    memset(&sb, 0, sizeof(sb));

    ret = squashfs_mount(&sb, 0);
    if (ret) {
        debug("no valid squashfs found\n");
        return ret;
    }
    return 0;
}

void squashfs_close()
{
    debug("squashfs_close\n");
    squashfs_put_super(&sb);
}

int squashfs_size(const char *filename, loff_t *size)
{
    struct inode *inode;

    inode = squashfs_findfile(&sb, filename, NULL);
    if (!inode)
        return -ENOENT;

    *size = inode->i_size;

    return 0;
}

int squashfs_exists(const char *filename)
{
    loff_t size;

    return squashfs_size(filename, &size) >= 0;
}

static int squashfs_file_open(const char *filename, struct squashfs_page **pagep)
{
    struct inode *inode;
    struct squashfs_page *page;
    int i;

    inode = squashfs_findfile(&sb, filename, NULL);
    if (!inode) {
        debug("squashfs_findfile failed?\n");
        return -ENOENT;
    }

    page = malloc(sizeof(struct squashfs_page));
    page->buf = calloc(32, sizeof(*page->buf));
    for (i = 0; i < 32; i++) {
        page->buf[i] = malloc(PAGE_SIZE);
        if (page->buf[i] == NULL) {
            debug("error allocation read buffer\n");
            goto error;
        }
    }

    page->data_block = 0;
    page->idx = 0;
    page->real_page.inode = inode;

    *pagep = page;

    return 0;

error:
    for (; i > 0; --i)
        free(page->buf[i]);

    free(page->buf);
    free(page);

    return -ENOMEM;
}

static void squashfs_file_close(struct squashfs_page *page)
{
    int i;

    for (i = 0; i < 32; i++)
        free(page->buf[i]);

    free(page->buf);
    free(squashfs_i(page->real_page.inode));
    free(page);
}

static int squashfs_read_buf(struct squashfs_page *page, loff_t pos, void **buf)
{
    unsigned int data_block = pos / (32 * PAGE_SIZE);
    unsigned int data_block_pos = pos % (32 * PAGE_SIZE);
    unsigned int idx = data_block_pos / PAGE_SIZE;

    if (data_block != page->data_block || page->idx == 0) {
        page->idx = 0;
        page->real_page.index = data_block * 32;
        squashfs_readpage(NULL, &page->real_page);
        page->data_block = data_block;
    }

    *buf = page->buf[idx];

    return 0;
}

static int squashfs_read_ll(struct squashfs_page *page, loff_t pos, void *buf, size_t insize)
{
    unsigned int size = insize;
    unsigned int ofs;
    unsigned int now;
    void *pagebuf;

    debug("squashfs_read_ll:pos  %lld insize %d\n", pos, insize);

    /* Read till end of current buffer page */
    ofs = pos % PAGE_SIZE;
    if (ofs) {
        squashfs_read_buf(page, pos, &pagebuf);

        now = min(size, PAGE_SIZE - ofs);
        memcpy(buf, pagebuf + ofs, now);

        size -= now;
        pos += now;
        buf += now;
    }

    /* Do full buffer pages */
    while (size >= PAGE_SIZE) {
        squashfs_read_buf(page, pos, &pagebuf);

        memcpy(buf, pagebuf, PAGE_SIZE);
        size -= PAGE_SIZE;
        pos += PAGE_SIZE;
        buf += PAGE_SIZE;
    }

    /* And the rest */
    if (size) {
        squashfs_read_buf(page, pos, &pagebuf);
        memcpy(buf, pagebuf, size);
        size  = 0;
    }

    return insize;
}

int squashfs_read(const char *filename, void *buf, loff_t offset, loff_t len, loff_t *actread)
{
    struct squashfs_page *page;
    int ret;

    ret = squashfs_file_open(filename, &page);
    if (ret < 0)
        return ret;


    loff_t file_size = page->real_page.inode->i_size;
    debug("squashfs_file_open '%s': offset %lld file_size %lld len %lld\n", filename, offset, file_size, len);
    if (offset > file_size) {
        ret = -EINVAL;
        goto cleanup;
    }

    if (len > 0) {
        if (offset + len > file_size)
            len = file_size - offset;
    } else {
        len = file_size;
    }

    *actread = squashfs_read_ll(page, offset, buf, len);

cleanup:
    squashfs_file_close(page);
    return ret;
}

struct squashfs_dir {
    struct fs_dir_stream parent;
    struct fs_dirent dirent;

    struct dentry root_dentry;

    char d_name[256];
    char root_d_name[256];
};

int squashfs_opendir(const char *filename, struct fs_dir_stream **dirsp)
{
    struct inode *inode;
    struct squashfs_dir *dir;
    char buf[256];

    debug("squashfs_opendir %s\n", filename);
    inode = squashfs_findfile(&sb, filename, buf);
    if (!inode)
        return -ENOENT;

    dir = malloc_cache_aligned(sizeof(*dir));
    if (!dir)
        return -ENOMEM;

    memset(dir, 0, sizeof(*dir));

    dir->root_dentry.d_inode = inode;

    sprintf(dir->d_name, "%s", buf);
    sprintf(dir->root_d_name, "%s", buf);

    *dirsp = (struct fs_dir_stream *)dir;
    return 0;
}

int squashfs_readdir(struct fs_dir_stream *dirs, struct fs_dirent **dentp)
{
    struct squashfs_dir *dir = (struct squashfs_dir *)dirs;
    struct fs_dirent *dent = &dir->dirent;
    struct dentry *root_dentry = &dir->root_dentry;
    struct inode *inode;

    if (squashfs_lookup_next(root_dentry->d_inode,
                 dir->root_d_name,
                 dir->d_name))
        return -ENOENT;

    memset(dent, 0, sizeof(*dent));
    strcpy(dent->name, dir->d_name);

    inode = squashfs_lookup(root_dentry->d_inode, dent->name, 0);
    if (!inode) {
        debug("This shouldn't happen!!! '%s'\n", dent->name);
        return -ENOENT;
    }

    dent->size = inode->i_size;
    switch ((inode->i_mode >> 12) & 15) {
    case DT_DIR:
        debug("'%s' is size %lld, directory (0x%08x)\n", dent->name, dent->size, inode->i_mode);
        dent->type = FS_DT_DIR;
        break;
    case DT_LNK:
        debug("'%s' is size %lld, link (0x%08x)\n", dent->name, dent->size, inode->i_mode);
        dent->type = FS_DT_LNK;
        break;
    default:
        debug("'%s' is size %lld, regular (0x%08x)\n", dent->name, dent->size, inode->i_mode);
        dent->type = FS_DT_REG;
        break;
    }
    free(squashfs_i(inode));

    *dentp = dent;

    return 0;
}

void squashfs_closedir(struct fs_dir_stream *dirs)
{
    struct squashfs_dir *dir = (struct squashfs_dir *)dirs;
    free(squashfs_i(dir->root_dentry.d_inode));
    free(dir);
}

