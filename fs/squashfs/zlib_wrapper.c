/*
 * Squashfs - a compressed read only filesystem for Linux
 *
 * Copyright (c) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * zlib_wrapper.c
 */

#include <linux/compat.h>
#include <u-boot/zlib.h>

#include "squashfs_compat.h"
#include "squashfs_fs.h"
#include "squashfs_fs_sb.h"
#include "squashfs.h"
#include "decompressor.h"
#include "page_actor.h"

static void *zlib_init(struct squashfs_sb_info *dummy, void *buff)
{
	z_stream *stream = calloc(sizeof(z_stream), 1);
	if (stream == NULL)
		goto failed;

	stream->zalloc = gzalloc;
	stream->zfree = gzfree;
	return stream;

failed:
	ERROR("Failed to allocate zlib workspace\n");
	kfree(stream);
	return ERR_PTR(-ENOMEM);
}


static void zlib_free(void *strm)
{
	z_stream *stream = strm;
	kfree(stream);
}


static int zlib_uncompress(struct squashfs_sb_info *msblk, void *strm,
	char **bh, int b, int offset, int length,
	struct squashfs_page_actor *output)
{
	int zlib_err, zlib_init = 0, k = 0;
	z_stream *stream = strm;

	stream->avail_out = PAGE_SIZE;
	stream->next_out = squashfs_first_page(output);
	stream->avail_in = 0;

	do {
		if (stream->avail_in == 0 && k < b) {
			int avail = min(length, msblk->devblksize - offset);
			length -= avail;
			stream->next_in = bh[k] + offset;
			stream->avail_in = avail;
			offset = 0;
		}

		if (stream->avail_out == 0) {
			stream->next_out = squashfs_next_page(output);
			if (stream->next_out != NULL)
				stream->avail_out = PAGE_SIZE;
		}

		if (!zlib_init) {
			zlib_err = inflateInit2(stream, MAX_WBITS);
			if (zlib_err != Z_OK) {
				squashfs_finish_page(output);
				goto out;
			}
			zlib_init = 1;
		}

		zlib_err = inflate(stream, Z_SYNC_FLUSH);

		if (stream->avail_in == 0 && k < b)
			k++;
	} while (zlib_err == Z_OK);

	squashfs_finish_page(output);

	if (zlib_err != Z_STREAM_END)
		goto out;

	zlib_err = inflateEnd(stream);
	if (zlib_err != Z_OK)
		goto out;

	if (k < b)
		goto out;

	return stream->total_out;

out:
	return -EIO;
}

const struct squashfs_decompressor squashfs_zlib_comp_ops = {
	.init = zlib_init,
	.free = zlib_free,
	.decompress = zlib_uncompress,
	.id = ZLIB_COMPRESSION,
	.name = "zlib",
	.supported = 1
};

