#ifndef __SHFS_H
#define __SHFS_H

#define SHFS_ROOT_INO	1

struct shfs_disk_super {
	__le32 magic;
	__le32 block_size;
	__le32 block_bitmap;
	__le32 inode_bitmap;
	__le32 inode_table;
};

struct shfs_mem_super {
	int block_size;
	int block_bitmap;
	int inode_bitmap;
	int inode_table;
};

struct shfs_inode {
	__le16 uid;
	__le16 gid;
	__le16 mode;
	__le16 size;
	__le16 perm;
	__le16 dummy;
	__le32 time;
	__le32 blk_ptr[4];
};

struct shfs_dir_entry {
	__le32 inode;
	__le32 size;
	__le32 type;
	__le32 len;
	char name[16];
};

#endif

