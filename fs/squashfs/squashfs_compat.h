#ifndef SQUASHFS_COMPAT_H
#define SQUASHFS_COMPAT_H

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
#include <asm/atomic.h>
#include <asm-generic/atomic-long.h>
#include <linux/ctype.h>
#include <vsprintf.h>
#include <linux/time.h>
#include <linux/math64.h>
#include <linux/rbtree.h>

#include <linux/printk.h>
#include <fs.h>

#define PAGE_SHIFT 12

struct dentry;
struct file;
struct iattr;
struct kstat;
struct vfsmount;
struct super_block;

#define pgoff_t		unsigned long

/*
 * We "simulate" the Linux page struct much simpler here
 */
struct page {
    pgoff_t index;
    void *addr;
    struct inode *inode;
};

/* linux/magic.h */
#define SQUASHFS_MAGIC          0x73717368

/* linux/include/time.h */
#define NSEC_PER_SEC	1000000000L
#define get_seconds()	0
#define CURRENT_TIME_SEC	((struct timespec) { get_seconds(), 0 })

struct timespec {
    time_t	tv_sec;		/* seconds */
    long	tv_nsec;	/* nanoseconds */
};

/* linux/include/dcache.h */

/*
 * "quick string" -- eases parameter passing, but more importantly
 * saves "metadata" about the string (ie length and the hash).
 *
 * hash comes first so it snuggles against d_parent in the
 * dentry.
 */
struct qstr {
    unsigned int hash;
    unsigned int len;
#ifndef __UBOOT__
    const char *name;
#else
    char *name;
#endif
};

/* include/linux/fs.h */

/* Possible states of 'frozen' field */
enum {
    SB_UNFROZEN = 0,		/* FS is unfrozen */
    SB_FREEZE_WRITE	= 1,		/* Writes, dir ops, ioctls frozen */
    SB_FREEZE_PAGEFAULT = 2,	/* Page faults stopped as well */
    SB_FREEZE_FS = 3,		/* For internal FS use (e.g. to stop
                     * internal threads if needed) */
    SB_FREEZE_COMPLETE = 4,		/* ->freeze_fs finished successfully */
};

#define SB_FREEZE_LEVELS (SB_FREEZE_COMPLETE - 1)

struct sb_writers {
#ifndef __UBOOT__
    /* Counters for counting writers at each level */
    struct percpu_counter	counter[SB_FREEZE_LEVELS];
#endif
    wait_queue_head_t	wait;		/* queue for waiting for
                           writers / faults to finish */
    int			frozen;		/* Is sb frozen? */
    wait_queue_head_t	wait_unfrozen;	/* queue for waiting for
                           sb to be thawed */
#ifdef CONFIG_DEBUG_LOCK_ALLOC
    struct lockdep_map	lock_map[SB_FREEZE_LEVELS];
#endif
};

struct address_space {
    struct inode		*host;		/* owner: inode, block_device */
#ifndef __UBOOT__
    struct radix_tree_root	page_tree;	/* radix tree of all pages */
#endif
    spinlock_t		tree_lock;	/* and lock protecting it */
    unsigned int		i_mmap_writable;/* count VM_SHARED mappings */
    struct rb_root		i_mmap;		/* tree of private and shared mappings */
    struct list_head	i_mmap_nonlinear;/*list VM_NONLINEAR mappings */
    struct mutex		i_mmap_mutex;	/* protect tree, count, list */
    /* Protected by tree_lock together with the radix tree */
    unsigned long		nrpages;	/* number of total pages */
    pgoff_t			writeback_index;/* writeback starts here */
    const struct address_space_operations *a_ops;	/* methods */
    unsigned long		flags;		/* error bits/gfp mask */
#ifndef __UBOOT__
    struct backing_dev_info *backing_dev_info; /* device readahead, etc */
#endif
    spinlock_t		private_lock;	/* for use by the address_space */
    struct list_head	private_list;	/* ditto */
    void			*private_data;	/* ditto */
} __attribute__((aligned(sizeof(long))));

/*
 * Keep mostly read-only and often accessed (especially for
 * the RCU path lookup and 'stat' data) fields at the beginning
 * of the 'struct inode'
 */
struct inode {
    umode_t			i_mode;
    unsigned short		i_opflags;
    kuid_t			i_uid;
    kgid_t			i_gid;
    unsigned int		i_flags;

#ifdef CONFIG_FS_POSIX_ACL
    struct posix_acl	*i_acl;
    struct posix_acl	*i_default_acl;
#endif

    const struct inode_operations	*i_op;
    struct super_block	*i_sb;
    struct address_space	*i_mapping;

#ifdef CONFIG_SECURITY
    void			*i_security;
#endif

    /* Stat data, not accessed from path walking */
    unsigned long		i_ino;
    /*
     * Filesystems may only read i_nlink directly.  They shall use the
     * following functions for modification:
     *
     *    (set|clear|inc|drop)_nlink
     *    inode_(inc|dec)_link_count
     */
    union {
        const unsigned int i_nlink;
        unsigned int __i_nlink;
    };
    dev_t			i_rdev;
    loff_t			i_size;
    struct timespec		i_atime;
    struct timespec		i_mtime;
    struct timespec		i_ctime;
    spinlock_t		i_lock;	/* i_blocks, i_bytes, maybe i_size */
    unsigned short          i_bytes;
    unsigned int		i_blkbits;
    blkcnt_t		i_blocks;

#ifdef __NEED_I_SIZE_ORDERED
    seqcount_t		i_size_seqcount;
#endif

    /* Misc */
    unsigned long		i_state;
    struct mutex		i_mutex;

    unsigned long		dirtied_when;	/* jiffies of first dirtying */

    struct hlist_node	i_hash;
    struct list_head	i_wb_list;	/* backing dev IO list */
    struct list_head	i_lru;		/* inode LRU list */
    struct list_head	i_sb_list;
    union {
        struct hlist_head	i_dentry;
        struct rcu_head		i_rcu;
    };
    u64			i_version;
    atomic_t		i_count;
    atomic_t		i_dio_count;
    atomic_t		i_writecount;
    const struct file_operations	*i_fop;	/* former ->i_op->default_file_ops */
    struct file_lock	*i_flock;
    struct address_space	i_data;
#ifdef CONFIG_QUOTA
    struct dquot		*i_dquot[MAXQUOTAS];
#endif
    struct list_head	i_devices;
    union {
        struct pipe_inode_info	*i_pipe;
        struct block_device	*i_bdev;
        struct cdev		*i_cdev;
    };

    __u32			i_generation;

#ifdef CONFIG_FSNOTIFY
    __u32			i_fsnotify_mask; /* all events this inode cares about */
    struct hlist_head	i_fsnotify_marks;
#endif

#ifdef CONFIG_IMA
    atomic_t		i_readcount; /* struct files open RO */
#endif
    void			*i_private; /* fs or device private pointer */
};

struct super_operations {
    struct inode *(*alloc_inode)(struct super_block *sb);
    void (*destroy_inode)(struct inode *);

    void (*dirty_inode) (struct inode *, int flags);
    int (*write_inode) (struct inode *, struct writeback_control *wbc);
    int (*drop_inode) (struct inode *);
    void (*evict_inode) (struct inode *);
    void (*put_super) (struct super_block *);
    int (*sync_fs)(struct super_block *sb, int wait);
    int (*freeze_fs) (struct super_block *);
    int (*unfreeze_fs) (struct super_block *);
#ifndef __UBOOT__
    int (*statfs) (struct dentry *, struct kstatfs *);
#endif
    int (*remount_fs) (struct super_block *, int *, char *);
    void (*umount_begin) (struct super_block *);

#ifndef __UBOOT__
    int (*show_options)(struct seq_file *, struct dentry *);
    int (*show_devname)(struct seq_file *, struct dentry *);
    int (*show_path)(struct seq_file *, struct dentry *);
    int (*show_stats)(struct seq_file *, struct dentry *);
#endif
#ifdef CONFIG_QUOTA
    ssize_t (*quota_read)(struct super_block *, int, char *, size_t, loff_t);
    ssize_t (*quota_write)(struct super_block *, int, const char *, size_t, loff_t);
#endif
    int (*bdev_try_to_free_page)(struct super_block*, struct page*, gfp_t);
    long (*nr_cached_objects)(struct super_block *, int);
    long (*free_cached_objects)(struct super_block *, long, int);
};

struct super_block {
    struct list_head	s_list;		/* Keep this first */
    dev_t			s_dev;		/* search index; _not_ kdev_t */
    unsigned char		s_blocksize_bits;
    unsigned long		s_blocksize;
    loff_t			s_maxbytes;	/* Max file size */
    struct file_system_type	*s_type;
    const struct super_operations	*s_op;
    const struct dquot_operations	*dq_op;
    const struct quotactl_ops	*s_qcop;
    const struct export_operations *s_export_op;
    unsigned long		s_flags;
    unsigned long		s_magic;
    struct dentry		*s_root;
    struct rw_semaphore	s_umount;
    int			s_count;
    atomic_t		s_active;
#ifdef CONFIG_SECURITY
    void                    *s_security;
#endif
    const struct xattr_handler **s_xattr;

    struct list_head	s_inodes;	/* all inodes */
#ifndef __UBOOT__
    struct hlist_bl_head	s_anon;		/* anonymous dentries for (nfs) exporting */
#endif
    struct list_head	s_mounts;	/* list of mounts; _not_ for fs use */
    struct block_device	*s_bdev;
#ifndef __UBOOT__
    struct backing_dev_info *s_bdi;
#endif
    struct mtd_info		*s_mtd;
    struct hlist_node	s_instances;
#ifndef __UBOOT__
    struct quota_info	s_dquot;	/* Diskquota specific options */
#endif

    struct sb_writers	s_writers;

    char s_id[32];				/* Informational name */
    u8 s_uuid[16];				/* UUID */

    void 			*s_fs_info;	/* Filesystem private info */
    unsigned int		s_max_links;
#ifndef __UBOOT__
    fmode_t			s_mode;
#endif

    /* Granularity of c/m/atime in ns.
       Cannot be worse than a second */
    u32		   s_time_gran;

    /*
     * The next field is for VFS *only*. No filesystems have any business
     * even looking at it. You had been warned.
     */
    struct mutex s_vfs_rename_mutex;	/* Kludge */

    /*
     * Filesystem subtype.  If non-empty the filesystem type field
     * in /proc/mounts will be "type.subtype"
     */
    char *s_subtype;

#ifndef __UBOOT__
    /*
     * Saved mount options for lazy filesystems using
     * generic_show_options()
     */
    char __rcu *s_options;
#endif
    const struct dentry_operations *s_d_op; /* default d_op for dentries */

    /*
     * Saved pool identifier for cleancache (-1 means none)
     */
    int cleancache_poolid;

#ifndef __UBOOT__
    struct shrinker s_shrink;	/* per-sb shrinker handle */
#endif

    /* Number of inodes with nlink == 0 but still referenced */
    atomic_long_t s_remove_count;

    /* Being remounted read-only */
    int s_readonly_remount;

    /* AIO completions deferred from interrupt context */
    struct workqueue_struct *s_dio_done_wq;

#ifndef __UBOOT__
    /*
     * Keep the lru lists last in the structure so they always sit on their
     * own individual cachelines.
     */
    struct list_lru		s_dentry_lru ____cacheline_aligned_in_smp;
    struct list_lru		s_inode_lru ____cacheline_aligned_in_smp;
#endif
    struct rcu_head		rcu;
};

struct file_system_type {
    const char *name;
    int fs_flags;
#define FS_REQUIRES_DEV		1
#define FS_BINARY_MOUNTDATA	2
#define FS_HAS_SUBTYPE		4
#define FS_USERNS_MOUNT		8	/* Can be mounted by userns root */
#define FS_USERNS_DEV_MOUNT	16 /* A userns mount does not imply MNT_NODEV */
#define FS_RENAME_DOES_D_MOVE	32768	/* FS will handle d_move() during rename() internally. */
    struct dentry *(*mount) (struct file_system_type *, int,
               const char *, void *);
    void (*kill_sb) (struct super_block *);
    struct module *owner;
    struct file_system_type * next;
    struct hlist_head fs_supers;

#ifndef __UBOOT__
    struct lock_class_key s_lock_key;
    struct lock_class_key s_umount_key;
    struct lock_class_key s_vfs_rename_key;
    struct lock_class_key s_writers_key[SB_FREEZE_LEVELS];

    struct lock_class_key i_lock_key;
    struct lock_class_key i_mutex_key;
    struct lock_class_key i_mutex_dir_key;
#endif
};
/* include/linux/mount.h */
struct vfsmount {
    struct dentry *mnt_root;	/* root of the mounted tree */
    struct super_block *mnt_sb;	/* pointer to superblock */
    int mnt_flags;
};

struct path {
    struct vfsmount *mnt;
    struct dentry *dentry;
};

struct file {
    struct path		f_path;
#define f_dentry	f_path.dentry
#define f_vfsmnt	f_path.mnt
    const struct file_operations	*f_op;
    unsigned int 		f_flags;
    loff_t			f_pos;
    unsigned int		f_uid, f_gid;

    u64			f_version;
#ifdef CONFIG_SECURITY
    void			*f_security;
#endif
    /* needed for tty driver, and maybe others */
    void			*private_data;

#ifdef CONFIG_EPOLL
    /* Used by fs/eventpoll.c to link all the hooks to this file */
    struct list_head	f_ep_links;
    spinlock_t		f_ep_lock;
#endif /* #ifdef CONFIG_EPOLL */
#ifdef CONFIG_DEBUG_WRITECOUNT
    unsigned long f_mnt_write_state;
#endif
};

/*
 * get_seconds() not really needed in the read-only implmentation
 */
#define get_seconds()		0

/* 4k page size */
#define PAGE_CACHE_SHIFT	12
#define PAGE_CACHE_SIZE		(1 << PAGE_CACHE_SHIFT)

/* Page cache limit. The filesystems should put that into their s_maxbytes
   limits, otherwise bad things can happen in VM. */
#if BITS_PER_LONG==32
#define MAX_LFS_FILESIZE	(((u64)PAGE_CACHE_SIZE << (BITS_PER_LONG-1))-1)
#elif BITS_PER_LONG==64
#define MAX_LFS_FILESIZE 	0x7fffffffffffffffUL
#endif

/*
 * These are the fs-independent mount-flags: up to 32 flags are supported
 */
#define MS_RDONLY	 1	/* Mount read-only */
#define MS_NOSUID	 2	/* Ignore suid and sgid bits */
#define MS_NODEV	 4	/* Disallow access to device special files */
#define MS_NOEXEC	 8	/* Disallow program execution */
#define MS_SYNCHRONOUS	16	/* Writes are synced at once */
#define MS_REMOUNT	32	/* Alter flags of a mounted FS */
#define MS_MANDLOCK	64	/* Allow mandatory locks on an FS */
#define MS_DIRSYNC	128	/* Directory modifications are synchronous */
#define MS_NOATIME	1024	/* Do not update access times. */
#define MS_NODIRATIME	2048	/* Do not update directory access times */
#define MS_BIND		4096
#define MS_MOVE		8192
#define MS_REC		16384
#define MS_VERBOSE	32768	/* War is peace. Verbosity is silence.
                   MS_VERBOSE is deprecated. */
#define MS_SILENT	32768
#define MS_POSIXACL	(1<<16)	/* VFS does not apply the umask */
#define MS_UNBINDABLE	(1<<17)	/* change to unbindable */
#define MS_PRIVATE	(1<<18)	/* change to private */
#define MS_SLAVE	(1<<19)	/* change to slave */
#define MS_SHARED	(1<<20)	/* change to shared */
#define MS_RELATIME	(1<<21)	/* Update atime relative to mtime/ctime. */
#define MS_KERNMOUNT	(1<<22) /* this is a kern_mount call */
#define MS_I_VERSION	(1<<23) /* Update inode I_version field */
#define MS_ACTIVE	(1<<30)
#define MS_NOUSER	(1<<31)

#define I_NEW			8

/* Inode flags - they have nothing to superblock flags now */

#define S_SYNC		1	/* Writes are synced at once */
#define S_NOATIME	2	/* Do not update access times */
#define S_APPEND	4	/* Append-only file */
#define S_IMMUTABLE	8	/* Immutable file */
#define S_DEAD		16	/* removed, but still open directory */
#define S_NOQUOTA	32	/* Inode is not counted to quota */
#define S_DIRSYNC	64	/* Directory modifications are synchronous */
#define S_NOCMTIME	128	/* Do not update file c/mtime */
#define S_SWAPFILE	256	/* Do not truncate: swapon got its bmaps */
#define S_PRIVATE	512	/* Inode is fs-internal */

/* include/linux/stat.h */

#define S_IFMT  00170000
#define S_IFSOCK 0140000
#define S_IFLNK	 0120000
#define S_IFREG  0100000
#define S_IFBLK  0060000
#define S_IFDIR  0040000
#define S_IFCHR  0020000
#define S_IFIFO  0010000
#define S_ISUID  0004000
#define S_ISGID  0002000
#define S_ISVTX  0001000

/* include/linux/fs.h */

/*
 * File types
 *
 * NOTE! These match bits 12..15 of stat.st_mode
 * (ie "(i_mode >> 12) & 15").
 */
#define DT_UNKNOWN	0
#define DT_FIFO		1
#define DT_CHR		2
#define DT_DIR		4
#define DT_BLK		6
#define DT_REG		8
#define DT_LNK		10
#define DT_SOCK		12
#define DT_WHT		14

#define I_DIRTY_SYNC		1
#define I_DIRTY_DATASYNC	2
#define I_DIRTY_PAGES		4
#define I_NEW			8
#define I_WILL_FREE		16
#define I_FREEING		32
#define I_CLEAR			64
#define __I_LOCK		7
#define I_LOCK			(1 << __I_LOCK)
#define __I_SYNC		8
#define I_SYNC			(1 << __I_SYNC)

#define I_DIRTY (I_DIRTY_SYNC | I_DIRTY_DATASYNC | I_DIRTY_PAGES)

static inline loff_t i_size_read(const struct inode *inode)
{
        return inode->i_size;
}

/* linux/include/dcache.h */

#define DNAME_INLINE_LEN_MIN 36

struct dentry {
    unsigned int d_flags;		/* protected by d_lock */
    spinlock_t d_lock;		/* per dentry lock */
    struct inode *d_inode;		/* Where the name belongs to - NULL is
                     * negative */
    /*
     * The next three fields are touched by __d_lookup.  Place them here
     * so they all fit in a cache line.
     */
    struct hlist_node d_hash;	/* lookup hash list */
    struct dentry *d_parent;	/* parent directory */
    struct qstr d_name;

    struct list_head d_lru;		/* LRU list */
    /*
     * d_child and d_rcu can share memory
     */
    struct list_head d_subdirs;	/* our children */
    struct list_head d_alias;	/* inode alias list */
    unsigned long d_time;		/* used by d_revalidate */
    struct super_block *d_sb;	/* The root of the dentry tree */
    void *d_fsdata;			/* fs-specific data */
#ifdef CONFIG_PROFILING
    struct dcookie_struct *d_cookie; /* cookie, if any */
#endif
    int d_mounted;
    unsigned char d_iname[DNAME_INLINE_LEN_MIN];	/* small names */
};

static inline ino_t parent_ino(struct dentry *dentry)
{
    ino_t res;

    spin_lock(&dentry->d_lock);
    res = dentry->d_parent->d_inode->i_ino;
    spin_unlock(&dentry->d_lock);
    return res;
}

#endif // SQUASHFS_COMPAT_H
