#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <sys/stat.h>
#include "ext2_filesystem.h"

#define BASE_OFFSET 1024 //location of super block in first group
#define BLOCK_OFFSET(block) (BASE_OFFSET + (block-1)*block_size)
#define BLOCK_SIZE 1024
#define POINTER_SIZE 4

int fd = 0;

struct ext2_super_block super;
struct ext2_group_desc group;
struct ext2_inode inode;

int use_bitmap(int bno, uint8_t* bitmap) {
	int index =0, offset =0;
	if (bno == 0) { //if this is the first the bit in the bytes, it is always used so return 1
		return 1; 
	}
	index = (bno-1)/8; //which byte within bitmap stores info of this block #
	offset = (bno-1)%8;

	return (bitmap[index] & (1 << offset));
}

void convert_gmt(char* buffer, time_t rawtime) {
	struct tm* info = gmtime(&rawtime);
	strftime(buffer, 32, "%D %T", info);
}

int main (int argc, char **argv) {
	// read a file system image whose name is specified as a command line argument
	if (argc != 2) {
		fprintf(stderr, "Error: Too many input arguments\n");
	}

	fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "Error opening disk image\n");
		exit(2);
	}

	if (super.s_magic != EXT2_SUPER_MAGIC) {
		fprintf(stderr, "Not an Ext2 file system\n");
		exit(2);
	}

	//SUPERBLOCK 
	pread(fd, &super, sizeof(super), BASE_OFFSET); //need to check return value of pread to make sure reads specified size
	//superblock summary: single new-line terminated line summarizing key file system parameters
	unsigned int total_blocks = super.s_blocks_count; //total # of blocks
	unsigned int total_nodes = super.s_inodes_count; //total # of i-nodes
	unsigned int block_size = 1024 << super.s_log_block_size; //block size (bytes)
	unsigned int node_size = super.s_inode_size; //i-node size (bytes)
	unsigned int block_per_group = super.s_blocks_per_group; //blocks/group
	unsigned int node_per_group = super.s_inodes_per_group; //i-nodes/group
	unsigned int free_node = super.s_first_ino; //first non-reserved i-node
	fprintf(stdout,"SUPERBLOCK,%u,%u,%u,%u,%u,%u,%u\n", total_blocks, total_nodes, block_size, node_size, block_per_group, node_per_group, free_node);

	//GROUP (assuming only single)
	pread(fd, &group, sizeof(&group), BLOCK_OFFSET(2)); //not sure what number to use
	//group summary: scan each group in fd and produce new-line terminated line for each
	int groupnum = 0; //group number starting from 0
	unsigned int total_blocks_group = 1 + (super.s_blocks_count-1) / super.s_blocks_per_group; //total # of blocks in group (super.s_blocks_count)
	unsigned int total_nodes_group = super.s_inodes_count; //total # of i-nodes in group, only 1 group so equivalent
	
	unsigned int free_blocks = group.bg_free_blocks_count; //# of free blocks
	unsigned int free_nodes = group.bg_free_inodes_count; //# of free i-nodes

	unsigned int free_block_bitmap = group.bg_block_bitmap; //block number of free block bitmap for group, first block number that is not used
	unsigned int free_node_bitmap = group.bg_inode_bitmap; //block # of free i-node bitmap for this group, first block number 

	unsigned int node_location = group.bg_inode_table; //block # of first block of i-nodes for group
	fprintf(stdout,"GROUP,%d,%u,%u,%u,%u,%u,%u,%u\n", groupnum, total_blocks_group, total_nodes_group, free_blocks, free_nodes, free_block_bitmap, free_node_bitmap, node_location);
	
	//BLOCK BITMAP
	//scan free block bitmap for each group, for each free block produce new-line terminated line
	uint8_t *block_bitmap = malloc(block_size);
	int block_bitmap_offset = BLOCK_OFFSET(group.bg_block_bitmap);
	pread(fd, &block_bitmap, block_size, free_block_bitmap*block_size);

	//loop through the bitmap and check if each bit is used
	for (int i=0; i < sizeof(block_bitmap); i++) {
		if (use_bitmap(i, block_bitmap) == 0) {
			//free block entries: BFREE, number of free block
			fprintf(stdout, "BFREE, %d\n", i);
		}
	}
	//I-NODE BITMAP
	//scan free i-node bitmap for each group, for each free i-node, produce new-line terminate line
	uint8_t *node_bitmap = malloc(block_size); //need to initialize
	int node_bitmap_offset = BLOCK_OFFSET(group.bg_inode_bitmap);

	pread(fd, &node_bitmap, block_size, free_node_bitmap*block_size);

	//loop through the bitmap and check if each bit is used
	for (int i=0; i < sizeof(node_bitmap); i++) {
		if (use_bitmap(i, node_bitmap) == 0) {
			//free block entries: BFREE, number of free block
			fprintf(stdout, "IFREE, %d\n", i);
		}
	}

	//I-NODES
	//scan i-nodes for each group, for each allocated i-node produce new-line terminated line
	int nodetable_offset = BLOCK_OFFSET(group.bg_inode_table); //group.bg_inode_table*BLOCK_SIZE;

	for (unsigned int i = 0; i < total_nodes_group; i++) {
		pread(fd, &inode, sizeof(&inode), nodetable_offset + sizeof(&inode)*i);

		if (inode.i_links_count != 0 && inode.i_mode != 0) {
			unsigned int node_type =inode.i_mode;
		
			char node_type_opt;
			if (node_type & 0x8000) {
				node_type_opt = 'f';
			}
			else if (node_type & 0x4000) {
				node_type_opt = 'd';
			}
			else if (node_type & 0xA000) {
				node_type_opt = 's';
			}
			else {
				node_type_opt= '?';
			}
			unsigned int node_num = i+1;
			unsigned int node_mode = node_type & 0x0FFF;
			unsigned int node_owner =inode.i_uid; //only want low 12
			unsigned int node_group =inode.i_gid;
			unsigned int link_count = inode.i_links_count;

			char node_update[32]; char mod_time[32]; char lastaccess[32];

			convert_gmt(node_update, (time_t) inode.i_ctime);
			convert_gmt(mod_time, (time_t) inode.i_mtime);
			convert_gmt(lastaccess, (time_t) inode.i_atime);

			unsigned int node_file_size =inode.i_size; //size in bytes
			unsigned int num_blocks_filled =inode.i_blocks;

			//i-node summary: INODE, inode #, file type, mode, owner, group, link count, time of last i-node change, modification time, time of last access, file size, number of 512 byte blocks of disk space taken up this file
			//file-type: f for file, d for directory, s for symbolic link, ? for rest
			//mode: low order 12-bits, octal ... suggested format "%o"
			fprintf(stdout, "INODE, %u,%c,%o,%u,%u,%u,%s,%s,%s,%u,%u", node_num, node_type_opt, node_mode, node_owner, node_group, link_count, node_update, mod_time, lastaccess, node_file_size, num_blocks_filled);
			
			//for ordinary files and directories, the next 15 blocks are block addresses = decimal, 12 direct, 1 single, 1 double, 1 triple
			//for symbolic links, if the file length is less than size of block pointers (60 bytes) the file contains 0 data blocks and fifteen block pointers need not be printed
		
			//DIRECTORY I-NODE ENTRIES
			//for each directory i-node, scan every data block, for each valid (non-0 i-node #) directory entry, produce new-line terminated line
			if (!(node_type_opt == 's' && node_file_size < 60)) {
				for (int a = 0; a < 15; a++) {
					fprintf(stdout, ",%u", inode.i_block[a]);
				}
				fprintf(stdout, "\n");
				if (node_type_opt == 'd') {
					struct ext2_dir_entry direntry;

					//DIRECT BLOCKS
					for (int j = 0; j < 15; j++) {
						if (inode.i_block[j] != 0) {
							int iter = 0;
							while (iter < inode.i_size) {
								pread(fd, &direntry, sizeof(&direntry), BLOCK_OFFSET(2)); //inode.i_block[k]*BLOCK_SIZE + block_offset
								if (direntry.inode != 0) {
									int node_parent_num = i + 1;
									unsigned int direct_offset = iter+j*BLOCK_SIZE;
									unsigned int node_file_num = direntry.inode;
									unsigned int entrylen = direntry.rec_len;
									unsigned int namelen = direntry.name_len;
									char file_name[EXT2_NAME_LEN+1];
									memcpy(file_name, direntry.name, direntry.name_len);
									//directory entries: DIRENT, parent inode #, logical byte offset of entry within directory, inode # of referenced file, entry length, name length, name
									//parent inode number: i-node number of the directory that contains this entry
									fprintf(stdout, "DIRENT, %d,%u,%u,%u,%u,%s\n", node_parent_num, direct_offset, node_file_num, entrylen, namelen, file_name);
									//direntry = (void*) direntry + dorentry->rec_len; //move to next entry
								}
								iter += direntry.rec_len;
							}
						}
					}
					//INDIRECT ENTRIES
					//for each file or directory i-node, scan single indirect blocks and recursively double and triple indirect blocks, 
					//for each non-0 block pointer, produce new-line terminated line
					unsigned int logical_offset = 0;

					logical_offset += 12*BLOCK_SIZE;

					int block_pointer[BLOCK_SIZE];
					int sindblock_pointer[BLOCK_SIZE];
					int dindblock_pointer[BLOCK_SIZE];

					//single indirect block (always held at 13th entry)
					if (inode.i_block[12] !=0) {
						pread(fd, block_pointer, BLOCK_SIZE, BLOCK_OFFSET(inode.i_block[12])); //inode.i_block[12]*BLOCK_SIZE

						//iterate through block pointer stored at indirect block
						for (int b=0; b < BLOCK_SIZE/POINTER_SIZE; b++) {
							if (block_pointer[b] !=0) {
								int iter =0;
								while (iter < BLOCK_SIZE) { //for (int block_offset = 0; block_offset <BLOCK_SIZE)
									pread(fd, &direntry, sizeof(direntry), BLOCK_OFFSET(block_pointer[b]) + iter); //block_pointer[b]* BLOCK_SIZE + iter
									if (direntry.inode != 0) {
										int node_parent_num = i + 1;
										unsigned int direct_offset = logical_offset+iter+b*BLOCK_SIZE; //iter+j*BLOCK_SIZE
										unsigned int node_file_num = direntry.inode;
										unsigned int entrylen = direntry.rec_len;
										unsigned int namelen = direntry.name_len;
										char file_name[EXT2_NAME_LEN+1];
										memcpy(file_name, direntry.name, direntry.name_len);

										//directory entries: DIRENT, parent inode #, logical byte offset of entry within directory, inode # of referenced file, entry length, name length, name
										//parent inode number: i-node number of the directory that contains this entry
										fprintf(stdout, "DIRENT, %d,%u,%u,%u,%u,%s\n", node_parent_num, direct_offset, node_file_num, entrylen, namelen, file_name);
										//direntry = (void*) direntry + dorentry->rec_len; //move to next entry
									}
									iter += direntry.rec_len;
								}
							}
						}
					}

					logical_offset += (BLOCK_SIZE/POINTER_SIZE) * BLOCK_SIZE;

					//double indirect block (always held at 14th entry)
					if (inode.i_block[13] != 0) {
						pread(fd, sindblock_pointer, BLOCK_SIZE, BLOCK_OFFSET(inode.i_block[13])); //inode.i_block[13]*BLOCK_SIZE

						//iterate through single indirect block pointers stored at double indirect block
						for (int c=0; c < BLOCK_SIZE/POINTER_SIZE; c++) {
							if (sindblock_pointer[c] != 0) {
								pread(fd, block_pointer, BLOCK_SIZE, BLOCK_OFFSET(sindblock_pointer[c])); //sindblock_pointer[c]*BLOCK_SIZE
								for (int d=0; d < BLOCK_SIZE/POINTER_SIZE; d++) {
									if (block_pointer[d] != 0) {
										int iter = 0;
										while (iter< BLOCK_SIZE) { //for (int block_offset = 0; block_offset <BLOCK_SIZE)
											pread(fd, &direntry, sizeof(direntry), BLOCK_OFFSET(block_pointer[d]) + iter); //block_pointer[d]* BLOCK_SIZE + iter
											if (direntry.inode != 0) {
												int node_parent_num = i + 1;
												unsigned int direct_offset = logical_offset+iter+d*BLOCK_SIZE + c*(BLOCK_SIZE/POINTER_SIZE)*(BLOCK_SIZE); //iter+j*BLOCK_SIZE
												unsigned int node_file_num = direntry.inode;
												unsigned int entrylen = direntry.rec_len;
												unsigned int namelen = direntry.name_len;
												char file_name[EXT2_NAME_LEN+1];
												memcpy(file_name, direntry.name, direntry.name_len);
												//directory entries: DIRENT, parent inode #, logical byte offset of entry within directory, inode # of referenced file, entry length, name length, name
												//parent inode number: i-node number of the directory that contains this entry
												fprintf(stdout, "DIRENT, %d,%u,%u,%u,%u,%s\n", node_parent_num, direct_offset, node_file_num, entrylen, namelen, file_name);
												//direntry = (void*) direntry + dorentry->rec_len; //move to next entry
											}
											iter += direntry.rec_len;
										}
									}
								}
							}
						}
					}

					logical_offset += (BLOCK_SIZE/POINTER_SIZE) * (BLOCK_SIZE/POINTER_SIZE) * BLOCK_SIZE;

					//triple indirect block (always at 15th entry)
					if (inode.i_block[14] != 0) {
						pread(fd, dindblock_pointer, BLOCK_SIZE, BLOCK_OFFSET(inode.i_block[14])); //inode.i_block[14]*BLOCK_SIZE

						//iterate through double indirect block pointers stored a triple indirect block 
						for (int e = 0; e < BLOCK_SIZE/POINTER_SIZE; e++) {
							if (dindblock_pointer[e] != 0) {
								pread(fd, sindblock_pointer, BLOCK_SIZE, BLOCK_OFFSET(dindblock_pointer[e])); //dindblock_pointer[e]*BLOCK_SIZE

								//iterate through single indirect block pointers stored at double indirect block pointers
								for (int f = 0 ; f < BLOCK_SIZE/POINTER_SIZE; f++) {
									if (sindblock_pointer[f] != 0) {
										pread(fd, block_pointer, BLOCK_SIZE, BLOCK_OFFSET(sindblock_pointer[f])); //sindblock_pointer[f]*BLOCK_SIZE

										//iterate through block pointers stored at single indirect block pointers
										for (int g = 0; g < BLOCK_SIZE/POINTER_SIZE; g++) {
											if (block_pointer[g] != 0) {
												int iter =0;
												while (iter < BLOCK_SIZE) { //for (int block_offset = 0; block_offset <BLOCK_SIZE)
													pread(fd, &direntry, sizeof(direntry), BLOCK_OFFSET(block_pointer[g]) + iter); //block_pointer[g]* BLOCK_SIZE + iter
													if (direntry.inode != 0) {
														int node_parent_num = i + 1;
														unsigned int direct_offset = logical_offset+iter+g*BLOCK_SIZE + f*(BLOCK_SIZE/POINTER_SIZE)*(BLOCK_SIZE)+ e*(BLOCK_SIZE/POINTER_SIZE)*(BLOCK_SIZE/POINTER_SIZE)*(BLOCK_SIZE); //iter+j*BLOCK_SIZE
														unsigned int node_file_num = direntry.inode;
														unsigned int entrylen = direntry.rec_len;
														unsigned int namelen = direntry.name_len;
														char file_name[EXT2_NAME_LEN+1];
														memcpy(file_name, direntry.name, direntry.name_len);
														//directory entries: DIRENT, parent inode #, logical byte offset of entry within directory, inode # of referenced file, entry length, name length, name
														//parent inode number: i-node number of the directory that contains this entry
														fprintf(stdout, "DIRENT, %d,%u,%u,%u,%u,%s\n", node_parent_num, direct_offset, node_file_num, entrylen, namelen, file_name);
														//direntry = (void*) direntry + dorentry->rec_len; //move to next entry
													}
													iter += direntry.rec_len;
												}
											}
										}
									}
								}
							}
						}
					}
				}
				if (node_type_opt == 'd' || node_type_opt == 'f') {
					int block_pointer[BLOCK_SIZE];
					int sindblock_pointer[BLOCK_SIZE];
					int dindblock_pointer[BLOCK_SIZE];

					//single indirect block (will always be 13th entry)
					if (inode.i_block[12] !=0) {
						pread(fd, block_pointer, BLOCK_SIZE, BLOCK_OFFSET(inode.i_block[12]));

						//iterate through block pointer stored at single indirect block
						for (int h=0; h < BLOCK_SIZE/POINTER_SIZE; h++) {
							if (block_pointer[h] != 0) {
								int node_owning_num = i+1; //i-node number of owning file
								int indirec_level = 1; //level of indirection of block scanned: 1 = single, 2 = double, 3 = triple
								unsigned int indirect_offset = 12+h; //logical block offset represented by referenced block, if data block, this is logical block offset within file, if single or double indirect block, this is same as logical offset of first data block it refers to
								unsigned int indirect_block_num = inode.i_block[12]; //block number of indirect block scanned, not highest level block but lower level block that contains block reference reported by this entry
								unsigned int ref_block_num = block_pointer[h]; //block number of referenced block
								fprintf(stdout, "INDIRECT, %d,%d,%u,%u,%u\n", node_owning_num, indirec_level, indirect_offset, indirect_block_num, ref_block_num);
							}
						}
					}

					//double indirect block (always at 14th entry)
					if (inode.i_block[13] !=0) {
						pread(fd, sindblock_pointer, BLOCK_SIZE, BLOCK_OFFSET(inode.i_block[13]));

						//iterate through single indirect block pointers stored at double indirect block
						for (int c=0; c < BLOCK_SIZE/POINTER_SIZE; c++) {
							if (sindblock_pointer[c] != 0) {
								int node_owning_num = i+1; //i-node number of owning file
								int indirec_level = 2; //level of indirection of block scanned: 1 = single, 2 = double, 3 = triple
								unsigned int indirect_offset = 12+(c+1) *(BLOCK_SIZE/POINTER_SIZE); //logical block offset represented by referenced block, if data block, this is logical block offset within file, if single or double indirect block, this is same as logical offset of first data block it refers to
								unsigned int indirect_block_num = inode.i_block[13]; //block number of indirect block scanned, not highest level block but lower level block that contains block reference reported by this entry
								unsigned int ref_block_num = sindblock_pointer[c]; //block number of referenced block
								fprintf(stdout, "INDIRECT, %d,%d,%u,%u,%u\n", node_owning_num, indirec_level, indirect_offset, indirect_block_num, ref_block_num);
							
								pread(fd, block_pointer, BLOCK_SIZE, BLOCK_OFFSET(sindblock_pointer[c]));

								//iterate through block pointer stored at single indirect block
								for (int h=0; h < BLOCK_SIZE/POINTER_SIZE; h++) {
									if (block_pointer[h] != 0) {
										int node_owning_num = i+1; //i-node number of owning file
										int indirec_level = 1; //level of indirection of block scanned: 1 = single, 2 = double, 3 = triple
										unsigned int indirect_offset = 12+(c+1) * (BLOCK_SIZE/POINTER_SIZE) + h; //logical block offset represented by referenced block, if data block, this is logical block offset within file, if single or double indirect block, this is same as logical offset of first data block it refers to
										unsigned int indirect_block_num = sindblock_pointer[c]; //block number of indirect block scanned, not highest level block but lower level block that contains block reference reported by this entry
										unsigned int ref_block_num = block_pointer[h]; //block number of referenced block
										fprintf(stdout, "INDIRECT, %d,%d,%u,%u,%u\n", node_owning_num, indirec_level, indirect_offset, indirect_block_num, ref_block_num);
									}
								}
							}
						}
					}
					//triple indirection block (always at 15th entry)
					if (inode.i_block[14] != 0) {
						pread(fd, dindblock_pointer, BLOCK_SIZE, BLOCK_OFFSET(inode.i_block[14]));

						//iterate through double indirect block pointers stored at triple indirect blocks
						for (int l = 0; l < BLOCK_SIZE/POINTER_SIZE; l++) {
							if (dindblock_pointer[l] > 0) {
								int node_owning_num = i+1; //i-node number of owning file
								int indirec_level = 3; //level of indirection of block scanned: 1 = single, 2 = double, 3 = triple
								unsigned int indirect_offset = 12+(l+1) *(BLOCK_SIZE/POINTER_SIZE)*(BLOCK_SIZE/POINTER_SIZE)+(BLOCK_SIZE/POINTER_SIZE); //logical block offset represented by referenced block, if data block, this is logical block offset within file, if single or double indirect block, this is same as logical offset of first data block it refers to
								unsigned int indirect_block_num = inode.i_block[14]; //block number of indirect block scanned, not highest level block but lower level block that contains block reference reported by this entry
								unsigned int ref_block_num = dindblock_pointer[l]; //block number of referenced block
								fprintf(stdout, "INDIRECT, %d,%d,%u,%u,%u\n", node_owning_num, indirec_level, indirect_offset, indirect_block_num, ref_block_num);
								
								pread(fd, sindblock_pointer, BLOCK_SIZE, dindblock_pointer[l]*BLOCK_SIZE);

								//iterate through single indirect block pointer stored at double indirect block
								for (int m = 0; m < BLOCK_SIZE/POINTER_SIZE; m++) {
									if (sindblock_pointer[m] != 0) {
										int node_owning_num = i+1; //i-node number of owning file
										int indirec_level = 2; //level of indirection of block scanned: 1 = single, 2 = double, 3 = triple
										unsigned int indirect_offset = 12+(l+1) *(BLOCK_SIZE/POINTER_SIZE)*(BLOCK_SIZE/POINTER_SIZE)+(m+1)*(BLOCK_SIZE/POINTER_SIZE); //logical block offset represented by referenced block, if data block, this is logical block offset within file, if single or double indirect block, this is same as logical offset of first data block it refers to
										unsigned int indirect_block_num = dindblock_pointer[l]; //block number of indirect block scanned, not highest level block but lower level block that contains block reference reported by this entry
										unsigned int ref_block_num = sindblock_pointer[m]; //block number of referenced block
										fprintf(stdout, "INDIRECT, %d,%d,%u,%u,%u\n", node_owning_num, indirec_level, indirect_offset, indirect_block_num, ref_block_num);
								
										pread(fd, block_pointer, BLOCK_SIZE, sindblock_pointer[m]*BLOCK_SIZE);

										//iterate through block pointers at indirect block
										for (int n = 0; n < BLOCK_SIZE/POINTER_SIZE; n++) {
											if (block_pointer[n] > 0) {
												int node_owning_num = i+1; //i-node number of owning file
												int indirec_level = 1; //level of indirection of block scanned: 1 = single, 2 = double, 3 = triple
												unsigned int indirect_offset = 12+(l+1) *(BLOCK_SIZE/POINTER_SIZE)*(BLOCK_SIZE/POINTER_SIZE)+(m+1)*(BLOCK_SIZE/POINTER_SIZE)+n; //logical block offset represented by referenced block, if data block, this is logical block offset within file, if single or double indirect block, this is same as logical offset of first data block it refers to
												unsigned int indirect_block_num = sindblock_pointer[m]; //block number of indirect block scanned, not highest level block but lower level block that contains block reference reported by this entry
												unsigned int ref_block_num = block_pointer[n]; //block number of referenced block
												fprintf(stdout, "INDIRECT, %d,%d,%u,%u,%u\n", node_owning_num, indirec_level, indirect_offset, indirect_block_num, ref_block_num);
								
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

}







