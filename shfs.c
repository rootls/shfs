#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/mm.h>
#include <linux/blkdev.h>

#include "shfs.h"

#define P_MSG(fmt, args...)     \
	printk(KERN_INFO "shfs: [%s:%d] " fmt "\n", __FUNCTION__, __LINE__, ##args);
#define P_ERR(fmt, args...)     \
	printk(KERN_ERR "shfs: [%s:%d] " fmt "\n", __FUNCTION__, __LINE__, ##args);

struct inode *shfs_iget(struct super_block *sb, unsigned long ino)
{
	struct inode *inode;
	P_MSG();

	inode = iget_locked(sb, ino);
	if (!inode)
		return ERR_PTR(-ENOMEM);
	if (!(inode->i_state & I_NEW))
		return inode;

	inode->i_mode = 0040000;	/* Directory */
	inode->i_uid = 0;
	inode->i_gid = 0;
	inode->i_size = 0;
	inode->i_atime.tv_sec = 0;
	inode->i_ctime.tv_sec = 0;
	inode->i_mtime.tv_sec = 0;
	inode->i_atime.tv_nsec = 0;

	unlock_new_inode(inode);
	return inode;
}

static int shfs_fill_super(struct super_block *sb, void *data, int silent)
{
	struct buffer_head *bh;
	//struct shfs_disk_super *disk_super;
	struct shfs_mem_super *mem_super;
	struct inode *root;
	int err;
	P_MSG();

	mem_super = kmalloc(sizeof(*mem_super), GFP_KERNEL);
	if (!mem_super) {
		P_ERR("error allocating in-memmory super block structure");
		err = -ENOMEM;
		goto err_mem_super;
	}
	mem_super->block_size = 4096;
	sb->s_fs_info = mem_super;

	bh = sb_bread(sb, mem_super->block_size);
	if (!bh) {
		P_ERR("error reading super block from disk");
		err = -EIO;
		goto err_super;
	}

	root = shfs_iget(sb, SHFS_ROOT_INO);
	if (IS_ERR(root)) {
		P_ERR("failed to get root inode");
		err = -ENOMEM;
		goto err_iget;
	}

	if (!S_ISDIR(root->i_mode)) {
		P_ERR("root not a directory");
		err = -EIO;
		goto err_i_mode;
	}

	sb->s_root = d_alloc_root(root);
	if (!sb->s_root) {
		P_ERR("get root dentry failed");
		iput(root);
		err = -ENOMEM;
		goto err_s_root;
	}

	P_MSG("mounted shfs filesystem");
	return 0;

err_s_root:
	brelse(bh);
err_i_mode:
err_iget:
	iput(root);
err_super:
	kfree(mem_super);
err_mem_super:
	return err;
}

static struct dentry *shfs_mount(struct file_system_type *fs_type,
		int flags, const char *dev_name, void *data)
{
	P_MSG();
	return mount_bdev(fs_type, flags, dev_name, data, shfs_fill_super);
}

static struct file_system_type shfs_fs_type = {
	.name		= "shfs",
	.owner		= THIS_MODULE,
	.mount		= shfs_mount,
	.kill_sb	= kill_block_super,
	.fs_flags	= FS_REQUIRES_DEV,
};

static int __init shfs_init(void)
{
	int err;
	P_MSG();

	err = register_filesystem(&shfs_fs_type);
	if (err)
		return err;
	return 0;
}

static void __exit shfs_exit(void)
{
	P_MSG();
	unregister_filesystem(&shfs_fs_type);
}

MODULE_LICENSE("GPL/MIT");
module_init(shfs_init);
module_exit(shfs_exit);

