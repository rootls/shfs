#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/mm.h>
#include <linux/blkdev.h>

#include "shfs.h"

#define P_MSG(fmt, args...)     \
	printk(KERN_INFO "shfs: [%s:%d] " fmt "\n", __FUNCTION__, __LINE__, ##args)
#define P_ERR(fmt, args...)     \
	printk(KERN_ERR "shfs: [%s:%d] " fmt "\n", __FUNCTION__, __LINE__, ##args)


static int shfs_readdir(struct file *fp, void *dirent, filldir_t filldir)
{
	struct inode *dir = fp->f_dentry->d_inode;
	struct shfs_inode *shinode = (struct shfs_inode *)dir->i_private;
	int size = shinode->size;
	P_MSG();

	P_MSG("%d %d %d %d", shinode->blk_ptr[0], shinode->uid, shinode->gid, shinode->mode);
	if (fp->f_pos >= size)
		return 0;

	return 0;
}

static int shfs_release(struct inode *dir, struct file *fp)
{
	P_MSG();
	return 0;
}

const struct file_operations shfs_dir_fops = {
	.read		= generic_read_dir,
	.readdir	= shfs_readdir,
	.release	= shfs_release,
};

const struct inode_operations shfs_dir_iops = {
//	.create		= shfs_create,
//	.lookup		= shfs_lookup,
//	.mkdir		= shfs_mkdir,
//	.rmdir		= shfs_rmdir,
//	.rename		= shfs_rename,
};

struct inode *shfs_iget(struct super_block *sb, int ino)
{
	struct inode *inode;
	struct shfs_mem_super *mem_super;
	struct shfs_inode *shinode;
	struct buffer_head *bh;
	int blknum;
	int offset;
	P_MSG();

	mem_super = (struct shfs_mem_super *)sb->s_fs_info;

	inode = iget_locked(sb, ino);
	if (!inode)
		return ERR_PTR(-ENOMEM);
	if (!(inode->i_state & I_NEW))
		return inode;

	shinode = kmalloc(sizeof(struct shfs_inode), GFP_KERNEL);
	if (!shinode) {
		P_ERR("failed to allocate memory for shfs_inode");
		iget_failed(inode);
		return ERR_PTR(-EIO);
	}

	blknum = mem_super->inode_table + (ino * sizeof(struct shfs_inode) / mem_super->block_size);
	offset = ino * sizeof(struct shfs_inode) / mem_super->block_size;
	bh = sb_bread(sb, blknum);
	if (!bh) {
		P_ERR("error reading inode block");
		kfree(shinode);
		iget_failed(inode);
		return ERR_PTR(-EIO);
	}
	memcpy(shinode, bh->b_data + offset, sizeof(struct shfs_inode));

	inode->i_mode = le16_to_cpu(shinode->mode);
	inode->i_uid = le16_to_cpu(shinode->uid);
	inode->i_gid = le16_to_cpu(shinode->gid);
	inode->i_size = le16_to_cpu(shinode->size);
	inode->i_atime.tv_sec = le32_to_cpu(shinode->time);
	inode->i_ctime.tv_sec = le32_to_cpu(shinode->time);
	inode->i_mtime.tv_sec = le32_to_cpu(shinode->time);
	inode->i_atime.tv_nsec = le32_to_cpu(shinode->time);
	inode->i_private = shinode;

	//P_MSG("%d %d %d %d: %d %d", inode->i_mode, inode->i_uid, inode->i_gid,
	//		inode->i_size, blknum, offset);

	/* setup directory and file operations */
	if (S_ISREG(inode->i_mode)) {
	} else if (S_ISDIR(inode->i_mode)) {
		inode->i_op = &shfs_dir_iops;
		inode->i_fop = &shfs_dir_fops;
	} else {
		P_ERR("invalid inode");
		kfree(shinode);
		iget_failed(inode);
		brelse(bh);
		return ERR_PTR(-EIO);
	}

	brelse(bh);
	unlock_new_inode(inode);
	return inode;
}

static int shfs_fill_super(struct super_block *sb, void *data, int silent)
{
	struct buffer_head *bh;
	struct shfs_disk_super *disk_super;
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

	bh = sb_bread(sb, 0);
	if (!bh) {
		P_ERR("error reading super block from disk");
		err = -EIO;
		goto err_super;
	}

	disk_super = (struct shfs_disk_super *)(bh->b_data);
	P_MSG("%d %d %d %d %d", le32_to_cpu(disk_super->magic),
			le32_to_cpu(disk_super->block_size),
			le32_to_cpu(disk_super->block_bitmap),
			le32_to_cpu(disk_super->inode_bitmap),
			le32_to_cpu(disk_super->inode_table)
	);

	if (le32_to_cpu(disk_super->block_size) != 4096) {
		P_ERR("invalid block size");
		err = -EIO;
		goto err_blk_size;
	}

	mem_super->block_size = 4096;
	mem_super->block_bitmap = le32_to_cpu(disk_super->block_bitmap);
	mem_super->inode_bitmap = le32_to_cpu(disk_super->inode_bitmap);
	mem_super->inode_table = le32_to_cpu(disk_super->inode_table);

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
err_i_mode:
	iput(root);
err_iget:
err_blk_size:
	brelse(bh);
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

