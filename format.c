#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

/* Commands :
 * $dd if=/dev/zero of=loopback.img bs=1024 count=204800
 * $losetup /dev/loop0 loopback.img
 * $format /dev/loop0
 */
int fd;

struct shfs_disk_super {
	uint32_t magic;
	uint32_t block_size;
	uint32_t block_bitmap;
	uint32_t inode_bitmap;
	uint32_t inode_table;
};

struct shfs_inode {
	uint32_t uid;
	uint32_t gid;
	uint16_t type;
	uint16_t perm;
	uint32_t create_time;
	uint32_t blk_ptr[4];
};

struct shfs_dir_entry {
	uint32_t inode;
	uint32_t size;
	uint32_t type;
	uint32_t len;
	char name[16];
};

int main(int argc, char *argv[])
{
	/* Read file system name from parameters */
	if (argc != 2) {
		printf("\nUsage : format [/dev/sda1]\n\n");
		exit(EXIT_FAILURE);
	}

	/* Open file system */
	fd = open(argv[1], O_RDWR);
	if (fd <= 0) {
		perror("error opening filesystem");
		exit(EXIT_FAILURE);
	}

	if (write_super() < 0) {
		goto err;
	}

	if (write_block_bitmap() < 0) {
		goto err;
	}

	if (write_inode_bitmap() < 0) {
		goto err;
	}

	if (write_inode_table() < 0) {
		goto err;
	}

	close(fd);
	exit(EXIT_SUCCESS);
err:
	close(fd);
	exit(EXIT_FAILURE);
}

int write_super(void)
{
	struct shfs_disk_super sb;

	sb.magic	= 0x1012F4DD;
	sb.block_size	= 4096;
	sb.block_bitmap = 1;
	sb.inode_bitmap = 2;
	sb.inode_table  = 3;

	/* Seek to block 0 */
	if (lseek(fd, 0 * 4096, 0) < 0) {
		perror("Failed to seek to superblock");
		return -1;
	}

	/* Write superblock */
	write(fd, &sb, sizeof(sb));
	printf("Wrote superblock : %lu bytes\n", sizeof(sb));

	return 0;
}

int write_block_bitmap(void)
{
	unsigned char bitmap[4096] = {0};
	int c;

	/* Resetting bitmap to dummy data */
	for (c = 0; c < sizeof(bitmap); c++) {
		bitmap[c] = 0x00;
	}

	/* Marking blocks in use by superblock, block bitmap, inode bitmap, inode_table */
	bitmap[0] = 0x0F;

	/* Seek to block 1 */
	if (lseek(fd, 1 * 4096, 0) < 0) {
		perror("Failed to seek to block bitmap");
		return -1;
	}

	/* Write block bitmap */
	write(fd, bitmap, sizeof(bitmap));
	printf("Wrote block bitmap : %lu bytes\n", sizeof(bitmap));
}

int write_inode_bitmap(void)
{
	unsigned char bitmap[4096] = {0};
	int c;

	/* Resetting bitmap to dummy data */
	for (c = 0; c < sizeof(bitmap); c++) {
		bitmap[c] = 0x00;
	}

	/* Seek to block 2 */
	if (lseek(fd, 2 * 4096, 0) < 0) {
		perror("Failed to seek to inode bitmap");
		return -1;
	}

	/* Write inode bitmap */
	write(fd, bitmap, sizeof(bitmap));
	printf("Wrote inode bitmap : %lu bytes\n", sizeof(bitmap));
}

int write_inode_table(void)
{
	struct shfs_inode inode_table[128] = {0};
	int c;

	/* Seek to block 3 */
	if (lseek(fd, 3 * 4096, 0) < 0) {
		perror("Failed to seek to itable");
		return -1;
	}

	/* Write inode table */
	write(fd, inode_table, sizeof(inode_table));
	printf("Wrote inode table : %lu bytes\n", sizeof(inode_table));
}

