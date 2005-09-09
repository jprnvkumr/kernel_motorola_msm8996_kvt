/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2005  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/

#include "fuse_i.h"

#include <linux/pagemap.h>
#include <linux/slab.h>
#include <linux/file.h>
#include <linux/mount.h>
#include <linux/seq_file.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/parser.h>
#include <linux/statfs.h>

MODULE_AUTHOR("Miklos Szeredi <miklos@szeredi.hu>");
MODULE_DESCRIPTION("Filesystem in Userspace");
MODULE_LICENSE("GPL");

spinlock_t fuse_lock;
static kmem_cache_t *fuse_inode_cachep;
static int mount_count;

static int mount_max = 1000;
module_param(mount_max, int, 0644);
MODULE_PARM_DESC(mount_max, "Maximum number of FUSE mounts allowed, if -1 then unlimited (default: 1000)");

#define FUSE_SUPER_MAGIC 0x65735546

struct fuse_mount_data {
	int fd;
	unsigned rootmode;
	unsigned user_id;
};

static struct inode *fuse_alloc_inode(struct super_block *sb)
{
	struct inode *inode;
	struct fuse_inode *fi;

	inode = kmem_cache_alloc(fuse_inode_cachep, SLAB_KERNEL);
	if (!inode)
		return NULL;

	fi = get_fuse_inode(inode);
	fi->i_time = jiffies - 1;
	fi->nodeid = 0;

	return inode;
}

static void fuse_destroy_inode(struct inode *inode)
{
	kmem_cache_free(fuse_inode_cachep, inode);
}

static void fuse_read_inode(struct inode *inode)
{
	/* No op */
}

static void fuse_clear_inode(struct inode *inode)
{
}

void fuse_change_attributes(struct inode *inode, struct fuse_attr *attr)
{
	if (S_ISREG(inode->i_mode) && i_size_read(inode) != attr->size)
		invalidate_inode_pages(inode->i_mapping);

	inode->i_ino     = attr->ino;
	inode->i_mode    = (inode->i_mode & S_IFMT) + (attr->mode & 07777);
	inode->i_nlink   = attr->nlink;
	inode->i_uid     = attr->uid;
	inode->i_gid     = attr->gid;
	i_size_write(inode, attr->size);
	inode->i_blksize = PAGE_CACHE_SIZE;
	inode->i_blocks  = attr->blocks;
	inode->i_atime.tv_sec   = attr->atime;
	inode->i_atime.tv_nsec  = attr->atimensec;
	inode->i_mtime.tv_sec   = attr->mtime;
	inode->i_mtime.tv_nsec  = attr->mtimensec;
	inode->i_ctime.tv_sec   = attr->ctime;
	inode->i_ctime.tv_nsec  = attr->ctimensec;
}

static void fuse_init_inode(struct inode *inode, struct fuse_attr *attr)
{
	inode->i_mode = attr->mode & S_IFMT;
	i_size_write(inode, attr->size);
}

static int fuse_inode_eq(struct inode *inode, void *_nodeidp)
{
	unsigned long nodeid = *(unsigned long *) _nodeidp;
	if (get_node_id(inode) == nodeid)
		return 1;
	else
		return 0;
}

static int fuse_inode_set(struct inode *inode, void *_nodeidp)
{
	unsigned long nodeid = *(unsigned long *) _nodeidp;
	get_fuse_inode(inode)->nodeid = nodeid;
	return 0;
}

struct inode *fuse_iget(struct super_block *sb, unsigned long nodeid,
			int generation, struct fuse_attr *attr, int version)
{
	struct inode *inode;
	struct fuse_conn *fc = get_fuse_conn_super(sb);
	int retried = 0;

 retry:
	inode = iget5_locked(sb, nodeid, fuse_inode_eq, fuse_inode_set, &nodeid);
	if (!inode)
		return NULL;

	if ((inode->i_state & I_NEW)) {
		inode->i_generation = generation;
		inode->i_data.backing_dev_info = &fc->bdi;
		fuse_init_inode(inode, attr);
		unlock_new_inode(inode);
	} else if ((inode->i_mode ^ attr->mode) & S_IFMT) {
		BUG_ON(retried);
		/* Inode has changed type, any I/O on the old should fail */
		make_bad_inode(inode);
		iput(inode);
		retried = 1;
		goto retry;
	}

	fuse_change_attributes(inode, attr);
	inode->i_version = version;
	return inode;
}

static void fuse_put_super(struct super_block *sb)
{
	struct fuse_conn *fc = get_fuse_conn_super(sb);

	spin_lock(&fuse_lock);
	mount_count --;
	fc->sb = NULL;
	fc->user_id = 0;
	fuse_release_conn(fc);
	*get_fuse_conn_super_p(sb) = NULL;
	spin_unlock(&fuse_lock);
}

enum {
	OPT_FD,
	OPT_ROOTMODE,
	OPT_USER_ID,
	OPT_DEFAULT_PERMISSIONS,
	OPT_ALLOW_OTHER,
	OPT_ALLOW_ROOT,
	OPT_KERNEL_CACHE,
	OPT_ERR
};

static match_table_t tokens = {
	{OPT_FD,			"fd=%u"},
	{OPT_ROOTMODE,			"rootmode=%o"},
	{OPT_USER_ID,			"user_id=%u"},
	{OPT_DEFAULT_PERMISSIONS,	"default_permissions"},
	{OPT_ALLOW_OTHER,		"allow_other"},
	{OPT_ALLOW_ROOT,		"allow_root"},
	{OPT_KERNEL_CACHE,		"kernel_cache"},
	{OPT_ERR,			NULL}
};

static int parse_fuse_opt(char *opt, struct fuse_mount_data *d)
{
	char *p;
	memset(d, 0, sizeof(struct fuse_mount_data));
	d->fd = -1;

	while ((p = strsep(&opt, ",")) != NULL) {
		int token;
		int value;
		substring_t args[MAX_OPT_ARGS];
		if (!*p)
			continue;

		token = match_token(p, tokens, args);
		switch (token) {
		case OPT_FD:
			if (match_int(&args[0], &value))
				return 0;
			d->fd = value;
			break;

		case OPT_ROOTMODE:
			if (match_octal(&args[0], &value))
				return 0;
			d->rootmode = value;
			break;

		case OPT_USER_ID:
			if (match_int(&args[0], &value))
				return 0;
			d->user_id = value;
			break;

		default:
			return 0;
		}
	}
	if (d->fd == -1)
		return 0;

	return 1;
}

static int fuse_show_options(struct seq_file *m, struct vfsmount *mnt)
{
	struct fuse_conn *fc = get_fuse_conn_super(mnt->mnt_sb);

	seq_printf(m, ",user_id=%u", fc->user_id);
	return 0;
}

void fuse_release_conn(struct fuse_conn *fc)
{
	kfree(fc);
}

static struct fuse_conn *new_conn(void)
{
	struct fuse_conn *fc;

	fc = kmalloc(sizeof(*fc), GFP_KERNEL);
	if (fc != NULL) {
		memset(fc, 0, sizeof(*fc));
		fc->sb = NULL;
		fc->user_id = 0;
		fc->bdi.ra_pages = (VM_MAX_READAHEAD * 1024) / PAGE_CACHE_SIZE;
		fc->bdi.unplug_io_fn = default_unplug_io_fn;
	}
	return fc;
}

static struct fuse_conn *get_conn(struct file *file, struct super_block *sb)
{
	struct fuse_conn *fc;

	fc = new_conn();
	if (fc == NULL)
		return NULL;
	spin_lock(&fuse_lock);
	fc->sb = sb;
	spin_unlock(&fuse_lock);
	return fc;
}

static struct inode *get_root_inode(struct super_block *sb, unsigned mode)
{
	struct fuse_attr attr;
	memset(&attr, 0, sizeof(attr));

	attr.mode = mode;
	attr.ino = FUSE_ROOT_ID;
	return fuse_iget(sb, 1, 0, &attr, 0);
}

static struct super_operations fuse_super_operations = {
	.alloc_inode    = fuse_alloc_inode,
	.destroy_inode  = fuse_destroy_inode,
	.read_inode	= fuse_read_inode,
	.clear_inode	= fuse_clear_inode,
	.put_super	= fuse_put_super,
	.show_options	= fuse_show_options,
};

static int inc_mount_count(void)
{
	int success = 0;
	spin_lock(&fuse_lock);
	mount_count ++;
	if (mount_max == -1 || mount_count <= mount_max)
		success = 1;
	spin_unlock(&fuse_lock);
	return success;
}

static int fuse_fill_super(struct super_block *sb, void *data, int silent)
{
	struct fuse_conn *fc;
	struct inode *root;
	struct fuse_mount_data d;
	struct file *file;
	int err;

	if (!parse_fuse_opt((char *) data, &d))
		return -EINVAL;

	sb->s_blocksize = PAGE_CACHE_SIZE;
	sb->s_blocksize_bits = PAGE_CACHE_SHIFT;
	sb->s_magic = FUSE_SUPER_MAGIC;
	sb->s_op = &fuse_super_operations;
	sb->s_maxbytes = MAX_LFS_FILESIZE;

	file = fget(d.fd);
	if (!file)
		return -EINVAL;

	fc = get_conn(file, sb);
	fput(file);
	if (fc == NULL)
		return -EINVAL;

	fc->user_id = d.user_id;

	*get_fuse_conn_super_p(sb) = fc;

	err = -ENFILE;
	if (!inc_mount_count() && current->uid != 0)
		goto err;

	err = -ENOMEM;
	root = get_root_inode(sb, d.rootmode);
	if (root == NULL)
		goto err;

	sb->s_root = d_alloc_root(root);
	if (!sb->s_root) {
		iput(root);
		goto err;
	}
	return 0;

 err:
	spin_lock(&fuse_lock);
	mount_count --;
	fc->sb = NULL;
	fuse_release_conn(fc);
	spin_unlock(&fuse_lock);
	*get_fuse_conn_super_p(sb) = NULL;
	return err;
}

static struct super_block *fuse_get_sb(struct file_system_type *fs_type,
				       int flags, const char *dev_name,
				       void *raw_data)
{
	return get_sb_nodev(fs_type, flags, raw_data, fuse_fill_super);
}

static struct file_system_type fuse_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "fuse",
	.get_sb		= fuse_get_sb,
	.kill_sb	= kill_anon_super,
};

static void fuse_inode_init_once(void *foo, kmem_cache_t *cachep,
				 unsigned long flags)
{
	struct inode * inode = foo;

	if ((flags & (SLAB_CTOR_VERIFY|SLAB_CTOR_CONSTRUCTOR)) ==
	    SLAB_CTOR_CONSTRUCTOR)
		inode_init_once(inode);
}

static int __init fuse_fs_init(void)
{
	int err;

	err = register_filesystem(&fuse_fs_type);
	if (err)
		printk("fuse: failed to register filesystem\n");
	else {
		fuse_inode_cachep = kmem_cache_create("fuse_inode",
						      sizeof(struct fuse_inode),
						      0, SLAB_HWCACHE_ALIGN,
						      fuse_inode_init_once, NULL);
		if (!fuse_inode_cachep) {
			unregister_filesystem(&fuse_fs_type);
			err = -ENOMEM;
		}
	}

	return err;
}

static void fuse_fs_cleanup(void)
{
	unregister_filesystem(&fuse_fs_type);
	kmem_cache_destroy(fuse_inode_cachep);
}

static int __init fuse_init(void)
{
	int res;

	printk("fuse init (API version %i.%i)\n",
	       FUSE_KERNEL_VERSION, FUSE_KERNEL_MINOR_VERSION);

	spin_lock_init(&fuse_lock);
	res = fuse_fs_init();
	if (res)
		goto err;

	return 0;

 err:
	return res;
}

static void __exit fuse_exit(void)
{
	printk(KERN_DEBUG "fuse exit\n");

	fuse_fs_cleanup();
}

module_init(fuse_init);
module_exit(fuse_exit);
