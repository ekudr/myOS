#include <common.h>
#include <part.h>
#include <ext4.h>
#include <linux/errno.h>
#include <div64.h>
#include <bmp.h>   /// check delete with loading bmp

extern disk_t boot_disk;


extern struct ext_filesystem ext_fs;

struct ext2_data *ext4fs_root;
struct ext2fs_node *ext4fs_file;
uint32 *ext4fs_indir1_block;
int ext4fs_indir1_size;
int ext4fs_indir1_blkno = -1;
uint32 *ext4fs_indir2_block;
int ext4fs_indir2_size;
int ext4fs_indir2_blkno = -1;

uint32 *ext4fs_indir3_block;
int ext4fs_indir3_size;
int ext4fs_indir3_blkno = -1;
struct ext2_inode *g_parent_inode;

static int symlinknest;


int 
ext4fs_devread(_uint64_t sector, int byte_offset, int byte_len,
		   char *buffer) {
	return fs_devread(sector, byte_offset, byte_len, buffer);
}


int 
ext4_read_superblock(char *buffer) {
//	struct ext_filesystem *fs = &ext_fs;
	int sect = SUPERBLOCK_START >> 9/*fs->dev_desc->log2blksz*/;
	int off = SUPERBLOCK_START % 512/*fs->dev_desc->blksz*/;

	return ext4fs_devread(sect, off, SUPERBLOCK_SIZE,
				buffer);
}

void 
ext4fs_free_node(struct ext2fs_node *node, struct ext2fs_node *currroot) {
	if ((node != &ext4fs_root->diropen) && (node != currroot))
		mfree(node);
}

// Block number of the inode table
uint64_t 
ext4fs_bg_get_inode_table_id(const struct ext2_block_group *bg,
				      const struct ext_filesystem *fs)
{
	uint64_t block_nr = bg->inode_table_id;
	if (fs->gdsize == 64)
		block_nr +=
			(uint64_t)bg->inode_table_id_high << 32;
	return block_nr;
}

static struct ext4_extent_header *
ext4fs_get_extent_block(struct ext2_data *data, struct ext_block_cache *cache,
		        struct ext4_extent_header *ext_block, uint32_t fileblock, int log2_blksz) {
	struct ext4_extent_idx *index;
	unsigned long long block;
	int blksz = EXT2_BLOCK_SIZE(data);
	int i;

	while (1) {
		index = (struct ext4_extent_idx *)(ext_block + 1);

		if (ext_block->eh_magic != EXT4_EXT_MAGIC)
			return NULL;
// debug("[EXT_FS] %s ext_block->eh_magic 0x%X ext_block->eh_depth %d\n", __func__, ext_block->eh_magic, ext_block->eh_depth);
		if (ext_block->eh_depth == 0)
			return ext_block;

		i = -1;
		do {
			i++;
			if (i >= ext_block->eh_entries)
				break;
		} while (fileblock >= index[i].ei_block);

		/*
		 * If first logical block number is higher than requested fileblock,
		 * it is a sparse file. This is handled on upper layer.
		 */
		if (i > 0)
			i--;

		block = index[i].ei_leaf_hi;
		block = (block << 32) + index[i].ei_leaf_lo;
		block <<= log2_blksz;
		if (!ext_cache_read(cache, (uint64_t)block, blksz))
			return NULL;
		ext_block = (struct ext4_extent_header *)cache->buf;
	}
}


static int 
ext4fs_blockgroup (struct ext2_data *data, int group, struct ext2_block_group *blkgrp) {
	long int blkno;
	unsigned int blkoff, desc_per_blk;
	int log2blksz = 9; // get_fs()->dev_desc->log2blksz;  CHECK
	int desc_size = ext_fs.gdsize;

	if (desc_size == 0)
		return EINVAL;
	desc_per_blk = EXT2_BLOCK_SIZE(data) / desc_size;

	if (desc_per_blk == 0)
		return EINVAL;
	blkno = data->sblock.first_data_block + 1 +
			group / desc_per_blk;
	blkoff = (group % desc_per_blk) * desc_size;

//	debug("[EXT_FS] ext4fs read %d group descriptor (blkno %ld blkoff %u)\n",
//	      group, blkno, blkoff);

	return ext4fs_devread((uint64_t)blkno <<
			      (LOG2_BLOCK_SIZE(data) - log2blksz),
			      blkoff, desc_size, (char *)blkgrp);
}

// read EXT4 inode
int 
ext4fs_read_inode(struct ext2_data *data, int ino, struct ext2_inode *inode) {
	struct ext2_block_group *blkgrp;
	struct ext2_sblock *sblock = &data->sblock;
	struct ext_filesystem *fs = &ext_fs;
	int log2blksz =  9; // ext_fs->dev_desc->log2blksz;   CHECK
	int inodes_per_block, err;
	long int blkno;
	unsigned int blkoff;

	// Allocate blkgrp based on gdsize (for 64-bit support).
	blkgrp = malloc(fs->gdsize);
	if (!blkgrp)
		return -ENOMEM;

	// It is easier to calculate if the first inode is 0. 
	ino--;
	if ( sblock->inodes_per_group == 0 || fs->inodesz == 0) {
		mfree(blkgrp);
		return -EINVAL;
	}
	err = ext4fs_blockgroup(data, ino / sblock->inodes_per_group, blkgrp);
	if (err) {
		mfree(blkgrp);
		return err;
	}
//    debug("[EXT_FS] %s read blockgroup block id %d\n", __func__, blkgrp->block_id);
//    debug("[EXT_FS] blockgroup checksum 0x%X flags 0x%X\n", blkgrp->bg_checksum, blkgrp->bg_flags);
//    debug("[EXT_FS] free blocks %d free inodes %d\n", blkgrp->free_blocks, blkgrp->free_inodes);
	inodes_per_block = EXT2_BLOCK_SIZE(data) / fs->inodesz;
//    debug("[EXT_FS] %s inode per block %d\n", __func__, inodes_per_block);
	if ( inodes_per_block == 0 ) {
		mfree(blkgrp);
		return -EINVAL;
	}
    
	blkno = ext4fs_bg_get_inode_table_id(blkgrp, fs) +
	    (ino % sblock->inodes_per_group) / inodes_per_block;
	blkoff = (ino % inodes_per_block) * fs->inodesz;
    
	// Free blkgrp as it is no longer required.
	mfree(blkgrp);

	// Read the inode.
	err = ext4fs_devread((uint64_t)blkno << (LOG2_BLOCK_SIZE(data) -
				log2blksz), blkoff,
				sizeof(struct ext2_inode), (char *)inode);
	if (err)
		return err;

	return 0;
}

void 
ext_cache_init(struct ext_block_cache *cache) {
	memset(cache, 0, sizeof(*cache));
}

void 
ext_cache_fini(struct ext_block_cache *cache) {
	mfree(cache->buf);
	ext_cache_init(cache);
}

long int 
read_allocated_block(struct ext2_inode *inode, int fileblock,
			      struct ext_block_cache *cache) {
	long int blknr;
	int blksz;
	int log2_blksz;
	int err;
	long int rblock;
	long int perblock_parent;
	long int perblock_child;
	unsigned long long start;
	// get the blocksize of the filesystem
	blksz = EXT2_BLOCK_SIZE(ext4fs_root);
	log2_blksz = LOG2_BLOCK_SIZE(ext4fs_root) - 9 /*get_fs()->dev_desc->log2blksz*/;

//    debug("[EXT_FS] %s blksz %d log2_blksz %d inode->flags 0x%X\n", __func__, blksz, log2_blksz, inode->flags);
	if (inode->flags & EXT4_EXTENTS_FL) {
		long int startblock, endblock;
		struct ext_block_cache *c, cd;
		struct ext4_extent_header *ext_block;
		struct ext4_extent *extent;
		int i;

		if (cache) {
			c = cache;
		} else {
			c = &cd;
			ext_cache_init(c);
		}
		ext_block = ext4fs_get_extent_block(ext4fs_root, c, (struct ext4_extent_header *)
						inode->b.blocks.dir_blocks, fileblock, log2_blksz);
		if (!ext_block) {
			printf("invalid extent block\n");
			if (!cache)
				ext_cache_fini(c);
			return -EINVAL;
		}

		extent = (struct ext4_extent *)(ext_block + 1);
//debug("[EXT_FS] %s ext_block->eh_entries %d \n", __func__, ext_block->eh_entries);
		for (i = 0; i < ext_block->eh_entries; i++) {
			startblock = extent[i].ee_block;
			endblock = startblock + extent[i].ee_len;
//debug("[EXT_FS] i=%d extent[i].ee_block %d extent[i].ee_len %d fileblock %d\n", i, extent[i].ee_block, extent[i].ee_len, fileblock);
			if (startblock > fileblock) {
				/* Sparse file */
				if (!cache)
					ext_cache_fini(c);
				return 0;

			} else if (fileblock < endblock) {
				start = extent[i].ee_start_hi;
				start = (start << 32) +
					extent[i].ee_start_lo;
				if (!cache)
					ext_cache_fini(c);
				return (fileblock - startblock) + start;
			}
		}

		if (!cache)
			ext_cache_fini(c);
		return 0;
	}

	/* Direct blocks. */
	if (fileblock < INDIRECT_BLOCKS)
		blknr = inode->b.blocks.dir_blocks[fileblock];

	/* Indirect. */
	else if (fileblock < (INDIRECT_BLOCKS + (blksz / 4))) {
		if (ext4fs_indir1_block == NULL) {
			ext4fs_indir1_block = malloc(blksz);
			if (ext4fs_indir1_block == NULL) {
				printf("** SI ext2fs read block (indir 1)"
					"malloc failed. **\n");
				return -1;
			}
			ext4fs_indir1_size = blksz;
			ext4fs_indir1_blkno = -1;
		}
		if (blksz != ext4fs_indir1_size) {
			mfree(ext4fs_indir1_block);
			ext4fs_indir1_block = NULL;
			ext4fs_indir1_size = 0;
			ext4fs_indir1_blkno = -1;
			ext4fs_indir1_block = malloc(blksz);
			if (ext4fs_indir1_block == NULL) {
				printf("** SI ext2fs read block (indir 1):"
					"malloc failed. **\n");
				return -1;
			}
			ext4fs_indir1_size = blksz;
		}
		if ((inode->b.blocks.indir_block <<
		     log2_blksz) != ext4fs_indir1_blkno) {
			err = ext4fs_devread((uint64_t)inode->b.blocks.indir_block << log2_blksz, 0,
					   blksz, (char *)ext4fs_indir1_block);
			if (err) {
				printf("** SI ext2fs read block (indir 1)"
					"failed. **\n");
				return -1;
			}
			ext4fs_indir1_blkno = inode->b.blocks.indir_block << log2_blksz;
		}
		blknr = ext4fs_indir1_block[fileblock - INDIRECT_BLOCKS];
	}
	/* Double indirect. */
	else if (fileblock < (INDIRECT_BLOCKS + (blksz / 4 *
					(blksz / 4 + 1)))) {

		long int perblock = blksz / 4;
		long int rblock = fileblock - (INDIRECT_BLOCKS + blksz / 4);

		if (ext4fs_indir1_block == NULL) {
			ext4fs_indir1_block = malloc(blksz);
			if (ext4fs_indir1_block == NULL) {
				printf("** DI ext2fs read block (indir 2 1)"
					"malloc failed. **\n");
				return -1;
			}
			ext4fs_indir1_size = blksz;
			ext4fs_indir1_blkno = -1;
		}
		if (blksz != ext4fs_indir1_size) {
			mfree(ext4fs_indir1_block);
			ext4fs_indir1_block = NULL;
			ext4fs_indir1_size = 0;
			ext4fs_indir1_blkno = -1;
			ext4fs_indir1_block = malloc(blksz);
			if (ext4fs_indir1_block == NULL) {
				printf("** DI ext2fs read block (indir 2 1)"
					"malloc failed. **\n");
				return -1;
			}
			ext4fs_indir1_size = blksz;
		}
		if ((inode->b.blocks.double_indir_block <<
		     log2_blksz) != ext4fs_indir1_blkno) {
			err = ext4fs_devread((uint64_t)inode->b.blocks.double_indir_block << log2_blksz,
					   0, blksz, (char *)ext4fs_indir1_block);
			if (err) {
				printf("** DI ext2fs read block (indir 2 1)"
					"failed. **\n");
				return -1;
			}
			ext4fs_indir1_blkno = inode->b.blocks.double_indir_block << log2_blksz;
		}

		if (ext4fs_indir2_block == NULL) {
			ext4fs_indir2_block = malloc(blksz);
			if (ext4fs_indir2_block == NULL) {
				printf("** DI ext2fs read block (indir 2 2)"
					"malloc failed. **\n");
				return -1;
			}
			ext4fs_indir2_size = blksz;
			ext4fs_indir2_blkno = -1;
		}
		if (blksz != ext4fs_indir2_size) {
			mfree(ext4fs_indir2_block);
			ext4fs_indir2_block = NULL;
			ext4fs_indir2_size = 0;
			ext4fs_indir2_blkno = -1;
			ext4fs_indir2_block = malloc(blksz);
			if (ext4fs_indir2_block == NULL) {
				printf("** DI ext2fs read block (indir 2 2)"
					"malloc failed. **\n");
				return -1;
			}
			ext4fs_indir2_size = blksz;
		}
		if ((ext4fs_indir1_block[rblock / perblock] <<
		     log2_blksz) != ext4fs_indir2_blkno) {
			err = ext4fs_devread((uint64_t)ext4fs_indir1_block[rblock / perblock] << log2_blksz, 0,
						blksz, (char *)ext4fs_indir2_block);
			if (err) {
				printf("** DI ext2fs read block (indir 2 2)"
					"failed. **\n");
				return -1;
			}
			ext4fs_indir2_blkno = ext4fs_indir1_block[rblock / perblock] << log2_blksz;
		}
		blknr = ext4fs_indir2_block[rblock % perblock];
	}
	/* Tripple indirect. */
	else {
		rblock = fileblock - (INDIRECT_BLOCKS + blksz / 4 +
				      (blksz / 4 * blksz / 4));
		perblock_child = blksz / 4;
		perblock_parent = ((blksz / 4) * (blksz / 4));

		if (ext4fs_indir1_block == NULL) {
			ext4fs_indir1_block = malloc(blksz);
			if (ext4fs_indir1_block == NULL) {
				printf("** TI ext2fs read block (indir 2 1)"
					"malloc failed. **\n");
				return -1;
			}
			ext4fs_indir1_size = blksz;
			ext4fs_indir1_blkno = -1;
		}
		if (blksz != ext4fs_indir1_size) {
			mfree(ext4fs_indir1_block);
			ext4fs_indir1_block = NULL;
			ext4fs_indir1_size = 0;
			ext4fs_indir1_blkno = -1;
			ext4fs_indir1_block = malloc(blksz);
			if (ext4fs_indir1_block == NULL) {
				printf("** TI ext2fs read block (indir 2 1)"
					"malloc failed. **\n");
				return -1;
			}
			ext4fs_indir1_size = blksz;
		}
		if ((inode->b.blocks.triple_indir_block <<
		     log2_blksz) != ext4fs_indir1_blkno) {
			err = ext4fs_devread((uint64_t)inode->b.blocks.triple_indir_block << log2_blksz,
                    0, blksz, (char *)ext4fs_indir1_block);
			if (err) {
				printf("** TI ext2fs read block (indir 2 1)"
					"failed. **\n");
				return -1;
			}
			ext4fs_indir1_blkno = inode->b.blocks.triple_indir_block << log2_blksz;
		}

		if (ext4fs_indir2_block == NULL) {
			ext4fs_indir2_block = malloc(blksz);
			if (ext4fs_indir2_block == NULL) {
				printf("** TI ext2fs read block (indir 2 2)"
					"malloc failed. **\n");
				return -1;
			}
			ext4fs_indir2_size = blksz;
			ext4fs_indir2_blkno = -1;
		}
		if (blksz != ext4fs_indir2_size) {
			mfree(ext4fs_indir2_block);
			ext4fs_indir2_block = NULL;
			ext4fs_indir2_size = 0;
			ext4fs_indir2_blkno = -1;
			ext4fs_indir2_block = malloc(blksz);
			if (ext4fs_indir2_block == NULL) {
				printf("** TI ext2fs read block (indir 2 2)"
					"malloc failed. **\n");
				return -1;
			}
			ext4fs_indir2_size = blksz;
		}
		if ((ext4fs_indir1_block[rblock / perblock_parent] << log2_blksz)
		    != ext4fs_indir2_blkno) {
			err = ext4fs_devread((uint64_t)ext4fs_indir1_block[rblock / perblock_parent] <<
						log2_blksz, 0, blksz, (char *)ext4fs_indir2_block);
			if (err) {
				printf("** TI ext2fs read block (indir 2 2)"
					"failed. **\n");
				return -1;
			}
			ext4fs_indir2_blkno = ext4fs_indir1_block[rblock / perblock_parent] << log2_blksz;
		}

		if (ext4fs_indir3_block == NULL) {
			ext4fs_indir3_block = malloc(blksz);
			if (ext4fs_indir3_block == NULL) {
				printf("** TI ext2fs read block (indir 2 2)"
					"malloc failed. **\n");
				return -1;
			}
			ext4fs_indir3_size = blksz;
			ext4fs_indir3_blkno = -1;
		}
		if (blksz != ext4fs_indir3_size) {
			mfree(ext4fs_indir3_block);
			ext4fs_indir3_block = NULL;
			ext4fs_indir3_size = 0;
			ext4fs_indir3_blkno = -1;
			ext4fs_indir3_block = malloc(blksz);
			if (ext4fs_indir3_block == NULL) {
				printf("** TI ext2fs read block (indir 2 2)"
					"malloc failed. **\n");
				return -1;
			}
			ext4fs_indir3_size = blksz;
		}
		if ((ext4fs_indir2_block[rblock / perblock_child] <<
		     log2_blksz) != ext4fs_indir3_blkno) {
			err = ext4fs_devread((uint64_t)ext4fs_indir2_block[(rblock / perblock_child) % (blksz / 4)]
                     << log2_blksz, 0, blksz, (char *)ext4fs_indir3_block);
			if (err) {
				printf("** TI ext2fs read block (indir 2 2)"
				       "failed. **\n");
				return -1;
			}
			ext4fs_indir3_blkno = ext4fs_indir2_block[(rblock / perblock_child) %
							      (blksz / 4)] << log2_blksz;
		}

		blknr = ext4fs_indir3_block[rblock % perblock_child];
	}
	debug("read_allocated_block %ld\n", blknr);

	return blknr;
}

/**
 * ext4fs_reinit_global() - Reinitialize values of ext4 write implementation's
 *			    global pointers
 *
 * This function assures that for a file with the same name but different size
 * the sequential store on the ext4 filesystem will be correct.
 *
 * In this function the global data, responsible for internal representation
 * of the ext4 data are initialized to the reset state. Without this, during
 * replacement of the smaller file with the bigger truncation of new file was
 * performed.
 */
void ext4fs_reinit_global(void)
{
	if (ext4fs_indir1_block != NULL) {
		mfree(ext4fs_indir1_block);
		ext4fs_indir1_block = NULL;
		ext4fs_indir1_size = 0;
		ext4fs_indir1_blkno = -1;
	}
	if (ext4fs_indir2_block != NULL) {
		mfree(ext4fs_indir2_block);
		ext4fs_indir2_block = NULL;
		ext4fs_indir2_size = 0;
		ext4fs_indir2_blkno = -1;
	}
	if (ext4fs_indir3_block != NULL) {
		mfree(ext4fs_indir3_block);
		ext4fs_indir3_block = NULL;
		ext4fs_indir3_size = 0;
		ext4fs_indir3_blkno = -1;
	}
}

int 
ext4fs_iterate_dir(struct ext2fs_node *dir, char *name,
				struct ext2fs_node **fnode, int *ftype)
{
	unsigned int fpos = 0;
	int err;
    int nlen;
	uint64_t actread;
	struct ext2fs_node *diro = (struct ext2fs_node *) dir;

//#ifdef DEBUG
//	if (name != NULL)
//		printf("Iterate dir %s\n", name);
//#endif /* of DEBUG */

	if (!diro->inode_read) {
		err = ext4fs_read_inode(diro->data, diro->ino, &diro->inode);
		if (err) {
            debug("[EXT_FS] read inode error %d in %s\n", err, __func__);
			return 0;
        }
	}
//    debug("[EXT_FS] Inode size %d\n", diro->inode.size);
	/* Search the file.  */
	while (fpos < diro->inode.size) {
		struct ext2_dirent dirent;

		err = ext4fs_read_file(diro, fpos, sizeof(struct ext2_dirent),
					   (char *)&dirent, &actread);

		if (err < 0) {
            debug("[EXT_FS] read file error %d in %s\n", err, __func__);
			return -1;
        }

		if (dirent.direntlen == 0) {
			printf("Failed to iterate over directory %s\n", name);
			return -1;
		}

		if (dirent.namelen != 0) {
			char filename[dirent.namelen + 1];
			struct ext2fs_node *fdiro;
			int type = FILETYPE_UNKNOWN;
            nlen = dirent.namelen;
			err = ext4fs_read_file(diro,
						  fpos +
						  sizeof(struct ext2_dirent),
						  dirent.namelen, filename,
						  &actread);
			if (err < 0) {
                debug("[EXT_FS] read file error %d in %s\n", err, __func__);
				return -1;
            }    

			fdiro = malloc(sizeof(struct ext2fs_node));
			if (!fdiro)
				return -1;

			fdiro->data = diro->data;
			fdiro->ino = dirent.inode;

			filename[dirent.namelen] = '\0';

			if (dirent.filetype != FILETYPE_UNKNOWN) {
				fdiro->inode_read = 0;

				if (dirent.filetype == FILETYPE_DIRECTORY)
					type = FILETYPE_DIRECTORY;
				else if (dirent.filetype == FILETYPE_SYMLINK)
					type = FILETYPE_SYMLINK;
				else if (dirent.filetype == FILETYPE_REG)
					type = FILETYPE_REG;
			} else {
				err = ext4fs_read_inode(diro->data, dirent.inode, &fdiro->inode);
				if (err) {
                    debug("[EXT_FS] read file error %d in %s\n", err, __func__);
					mfree(fdiro);
					return -1;
				}
				fdiro->inode_read = 1;

				if ((fdiro->inode.mode & FILETYPE_INO_MASK) == FILETYPE_INO_DIRECTORY) {
					type = FILETYPE_DIRECTORY;
				} else if ((fdiro->inode.mode & FILETYPE_INO_MASK) == FILETYPE_INO_SYMLINK) {
					type = FILETYPE_SYMLINK;
				} else if ((fdiro->inode.mode & FILETYPE_INO_MASK) == FILETYPE_INO_REG) {
					type = FILETYPE_REG;
				}
			}
//#ifdef DEBUG
//			printf("iterate >%s<\n", filename);
//#endif /* of DEBUG */
 //           debug("[EXT_FS] %s name 0x%X fnode 0x%X ftype 0x%X\n", __func__, name, fnode, ftype);
			if ((name != NULL) && (fnode != NULL)
			    && (ftype != NULL)) {
                    
 //           debug("[EXT_FS] name %s - %d filename %s - %d\n", name, nlen, filename, sizeof(filename));
                if(!sbi_memcmp(filename, name, nlen)){
					*ftype = type;
					*fnode = fdiro;
					return 1;
				}
			} else {
				if (fdiro->inode_read == 0) {
					err = ext4fs_read_inode(diro->data, dirent.inode, &fdiro->inode);
					if (err) {
						mfree(fdiro);
						return -1;
					}
					fdiro->inode_read = 1;
				}
				switch (type) {
				case FILETYPE_DIRECTORY:
					printf("<DIR> ");
					break;
				case FILETYPE_SYMLINK:
					printf("<SYM> ");
					break;
				case FILETYPE_REG:
					printf("      ");
					break;
				default:
					printf("< ? > ");
					break;
				}
				printf("%10u %s\n", fdiro->inode.size, filename);
			}
			mfree(fdiro);
		}
		fpos += dirent.direntlen;
	}
	return -1;
}


static char *
ext4fs_read_symlink(struct ext2fs_node *node) {
	char *symlink;
	struct ext2fs_node *diro = node;
	int err;
	uint64_t actread;

	if (!diro->inode_read) {
		err = ext4fs_read_inode(diro->data, diro->ino, &diro->inode);
		if (err)
			return NULL;
	}
	symlink = malloc(diro->inode.size + 1);
	if (!symlink)
		return NULL;

	if (diro->inode.size < sizeof(diro->inode.b.symlink)) {
		sbi_strncpy(symlink, diro->inode.b.symlink, diro->inode.size);
	} else {
		err = ext4fs_read_file(diro, 0, diro->inode.size, symlink, &actread);
		if ((err) || (actread == 0)) {
			mfree(symlink);
			return NULL;
		}
	}
	symlink[diro->inode.size] = '\0';
	return symlink;
}


int 
ext4fs_find_file1(const char *currpath, struct ext2fs_node *currroot,
		      struct ext2fs_node **currfound, int *foundtype) {
	char fpath[sbi_strlen(currpath) + 1];
	char *name = fpath;
	char *next;
	int err;
	int type = FILETYPE_DIRECTORY;
	struct ext2fs_node *currnode = currroot;
	struct ext2fs_node *oldnode = currroot;

	sbi_strncpy(fpath, currpath, sbi_strlen(currpath) + 1);

	// Remove all leading slashes.
	while (*name == '/')
		name++;

	if (!*name) {
		*currfound = currnode;
		return 1;
	}

	for (;;) {
		int found;

		// Extract the actual part from the pathname.
		next = sbi_strchr(name, '/');
		if (next) {
			// Remove all leading slashes.
			while (*next == '/')
				*(next++) = '\0';
		}

		if (type != FILETYPE_DIRECTORY) {
			ext4fs_free_node(currnode, currroot);
			return -1;
		}

		oldnode = currnode;

		// Iterate over the directory.
		found = ext4fs_iterate_dir(currnode, name, &currnode, &type);
//        debug("[EXT_FS] %s err %d\n",__func__,found);

        if (found != 1)
            return -1;

        if (found == -1)
			break;    

		// Read in the symlink and follow it.
		if (type == FILETYPE_SYMLINK) {
			char *symlink;

			// Test if the symlink does not loop.
			if (++symlinknest == 8) {
				ext4fs_free_node(currnode, currroot);
				ext4fs_free_node(oldnode, currroot);
				return -1;
			}

			symlink = ext4fs_read_symlink(currnode);
			ext4fs_free_node(currnode, currroot);

			if (!symlink) {
				ext4fs_free_node(oldnode, currroot);
				return 0;
			}

			debug("Got symlink >%s<\n", symlink);

			if (symlink[0] == '/') {
				ext4fs_free_node(oldnode, currroot);
				oldnode = &ext4fs_root->diropen;
			}

			// Lookup the node the symlink points to.
			err = ext4fs_find_file1(symlink, oldnode,
						    &currnode, &type);

			mfree(symlink);

			if (err) {
				ext4fs_free_node(oldnode, currroot);
				return -1;
			}
		}

		ext4fs_free_node(oldnode, currroot);

		// Found the node! 
		if (!next || *next == '\0') {
			*currfound = currnode;
			*foundtype = type;
			return 1;
		}
		name = next;
	}
	return -1;
}


int 
ext4fs_find_file(const char *path, struct ext2fs_node *rootnode,
	struct ext2fs_node **foundnode, int expecttype) {
	int err;
	int foundtype = FILETYPE_DIRECTORY;

	symlinknest = 0;
	if (!path)
		return -1;

	err = ext4fs_find_file1(path, rootnode, foundnode, &foundtype);
    
	if (err != 1)
		return -1;

	// Check if the node that was found was of the expected type. 
	if ((expecttype == FILETYPE_REG) && (foundtype != expecttype))
		return -1;
	else if ((expecttype == FILETYPE_DIRECTORY)
		   && (foundtype != expecttype))
		return -1;

	return 1;
}

int 
ext4fs_open(const char *filename, uintptr_t *len) {
	struct ext2fs_node *fdiro = NULL;
	int err;

	if (ext4fs_root == NULL)
		return -1;

	ext4fs_file = NULL;
	err = ext4fs_find_file(filename, &ext4fs_root->diropen, &fdiro,
				  FILETYPE_REG);
//    debug("[EXT_FS] %s error %d\n", __func__, err);
	if (err < 0)
		goto fail;

	if (!fdiro->inode_read) {
		err = ext4fs_read_inode(fdiro->data, fdiro->ino,
				&fdiro->inode);
		if (err)
			goto fail;
	}
	*len = fdiro->inode.size;
	ext4fs_file = fdiro;

	return 0;
fail:
	ext4fs_free_node(fdiro, &ext4fs_root->diropen);

	return -1;
}

int 
ext4fs_size(const char *filename, uintptr_t *size)
{
	return ext4fs_open(filename, size);
}

int ext4fs_mount(void) {
    struct ext2_data *data;
    int err;

    struct ext_filesystem *fs = &ext_fs;

    if(!boot_disk.dev_inited) {
        panic("NO BOOT DEVICE");
    }
    
//    debug("[EXT_FS] size of data structure %d\n", sizeof(struct ext2_data));
    data = (struct ext2_data *)malloc(SUPERBLOCK_SIZE);

	// Read the superblock.
	err = ext4_read_superblock((char *)&data->sblock);

 //   err = boot_disk.mmc->bread(data, boot_disk.fat_lba+2, 2); 

    if (err) {
        printf("[DISK] read BOOTFS return %d\n", err);
        goto fail; 
    }

    if(data->sblock.magic != EXT2_MAGIC){
        printf("[EXT4_FS] NOT EXT2/3/4 FS\n");
        return ENODEV;
    }

    printf("[EXT4_FS] Revision %d\n", data->sblock.revision_level);
    if (data->sblock.revision_level == 0) {
		fs->inodesz = 128;
		fs->gdsize = 32;
	} else {
		debug("EXT4 features COMPAT: %08x INCOMPAT: %08x RO_COMPAT: %08x\n",
		      data->sblock.feature_compatibility,
		      data->sblock.feature_incompat,
		      data->sblock.feature_ro_compat);

		fs->inodesz = data->sblock.inode_size;
		fs->gdsize = data->sblock.feature_incompat &
			EXT4_FEATURE_INCOMPAT_64BIT ?
			data->sblock.descriptor_size : 32;
	}

	debug("EXT2 rev %d, inode_size %d, descriptor size %d\n",
	      data->sblock.revision_level,
	      fs->inodesz, fs->gdsize);

    printf("[EXT4_FS] Total inodes %d\n", data->sblock.total_inodes);
    printf("[EXT4_FS] Total blocks %d\n", data->sblock.total_blocks);
    printf("[EXT4_FS] Free inodes %d\n", data->sblock.free_inodes);
    printf("[EXT4_FS] Free blocks %d\n", data->sblock.free_blocks);
    printf("[EXT4_FS] FS state 0x%X\n", data->sblock.fs_state);
    printf("[EXT4_FS] Inode size %d\n", data->sblock.inode_size);
    printf("[EXT4_FS] Block size %d\n", data->sblock.log2_block_size);
    printf("[EXT4_FS] Fragment size %d\n", data->sblock.log2_fragment_size);
    printf("[EXT4_FS] inodes per group %d\n", data->sblock.inodes_per_group);
    printf("[EXT4_FS] first data block %d\n", data->sblock.first_data_block);
    printf("[EXT4_FS] first inode %d\n", data->sblock.first_inode);

    data->diropen.data = data;
	data->diropen.ino = 2;
	data->diropen.inode_read = 1;
	data->inode = &data->diropen.inode;

	err = ext4fs_read_inode(data, 2, data->inode);
	if (err)
		goto fail;

	ext4fs_root = data;

    debug("[EXT_FS] inode virsion %d mode %d\n", data->inode->version, data->inode->mode);
    debug("[EXT_FS] inode size %d mode %d\n", data->inode->size, data->inode->blockcnt);

    return 0;

fail:
	debug("[EXT_FS] Failed to mount ext2 filesystem...\n");
//fail_noerr:
	mfree(data);
	ext4fs_root = NULL;
    return -1;
}

int 
ext4fs_read(char *buf, uint64_t offset, uint64_t len, uint64_t *actread) {
	if (ext4fs_root == NULL || ext4fs_file == NULL)
		return -1;

	return ext4fs_read_file(ext4fs_file, offset, len, buf, actread);
}

int 
ext4fs_ls(const char *dirname) {
	struct ext2fs_node *dirnode = NULL;
	int err;

	if (dirname == NULL)
		return -1;

	err = ext4fs_find_file(dirname, &ext4fs_root->diropen, &dirnode,
				  FILETYPE_DIRECTORY);
	if (err != 1) {
		printf("** Can not find directory. **\n");
		if (dirnode)
			ext4fs_free_node(dirnode, &ext4fs_root->diropen);
		return -1;
	}

	ext4fs_iterate_dir(dirnode, NULL, NULL, NULL);
	ext4fs_free_node(dirnode, &ext4fs_root->diropen);

	return 0;
}

int 
ext4_read_file(const char *filename, void *buf, uint64_t offset, 
                uint64_t len, uint64_t *len_read) {
	uintptr_t file_len;
	int ret;

	ret = ext4fs_open(filename, &file_len);
	if (ret < 0) {
		printf("** File not found %s **\n", filename);
		return -1;
	}

	if (len == 0)
		len = file_len;

	return ext4fs_read(buf, offset, len, len_read);
}

void 
ext4fs_close(void) {
	if ((ext4fs_file != NULL) && (ext4fs_root != NULL)) {
		ext4fs_free_node(ext4fs_file, &ext4fs_root->diropen);
		ext4fs_file = NULL;
	}
	if (ext4fs_root != NULL) {
		mfree(ext4fs_root);
		ext4fs_root = NULL;
	}

	ext4fs_reinit_global();
}

/*
 * Taken from openmoko-kernel mailing list: By Andy green
 * Optimized read file API : collects and defers contiguous sector
 * reads into one potentially more efficient larger sequential read action
 */
int 
ext4fs_read_file(struct ext2fs_node *node, uint64_t pos,
		    uint64_t len, char *buf, uint64_t *actread) {
//	struct ext_filesystem *fs = &ext_fs;
	int i;
	uint64_t blockcnt;
	int log2blksz = 9;
	int log2_fs_blocksize = LOG2_BLOCK_SIZE(node->data) - log2blksz;
	int blocksize = (1 << (log2_fs_blocksize + log2blksz));
	unsigned int filesize = node->inode.size;
	uint64_t previous_block_number = -1;
	uint64_t delayed_start = 0;
	uint64_t delayed_extent = 0;
	uint64_t delayed_skipfirst = 0;
	uint64_t delayed_next = 0;
	char *delayed_buf = NULL;
	char *start_buf = buf;
	short err;
	struct ext_block_cache cache;

 //   debug("[EXT_FS] %s pos %d len %d filesize %d\n", __func__, pos, len, filesize);
    
	ext_cache_init(&cache);

	// Adjust len so it we can't read past the end of the file.
	if (len + pos > filesize)
		len = (filesize - pos);

	if (blocksize <= 0 || len <= 0) {
		ext_cache_fini(&cache);
		return -1;
	}

	blockcnt = lldiv(((len + pos) + blocksize - 1), blocksize);
//    debug("[EXT_FS] log2_fs_blocksize %d blocksize %d blockcnt %d\n", log2_fs_blocksize, blocksize, blockcnt);
	for (i = lldiv(pos, blocksize); i < blockcnt; i++) {
		long int blknr;
		int blockoff = pos - (blocksize * i);
		int blockend = blocksize;
		int skipfirst = 0;

		blknr = read_allocated_block(&node->inode, i, &cache);
		if (blknr < 0) {
			ext_cache_fini(&cache);
			return -1;
		}

		blknr = blknr << log2_fs_blocksize;

		/* Last block.  */
		if (i == blockcnt - 1) {
			blockend = (len + pos) - (blocksize * i);

			/* The last portion is exactly blocksize. */
			if (!blockend)
				blockend = blocksize;
		}

		/* First block. */
		if (i == lldiv(pos, blocksize)) {
			skipfirst = blockoff;
			blockend -= skipfirst;
		}
//        debug("[EXT_FS] blknr %d blockend %d\n", blknr, blockend);
		if (blknr) {
			int err;

			if (previous_block_number != -1) {
				if (delayed_next == blknr) {
					delayed_extent += blockend;
					delayed_next += blockend >> log2blksz;
				} else {	/* spill */
					err = ext4fs_devread(delayed_start,
							delayed_skipfirst,
							delayed_extent,
							delayed_buf);
					if (err) {
                        printf("[EXT_FS] %s read error %d\n", __func__, err);
						ext_cache_fini(&cache);
						return -1;
					}
       
					previous_block_number = blknr;
					delayed_start = blknr;
					delayed_extent = blockend;
					delayed_skipfirst = skipfirst;
					delayed_buf = buf;
					delayed_next = blknr +
						(blockend >> log2blksz);
				}
			} else {
				previous_block_number = blknr;
				delayed_start = blknr;
				delayed_extent = blockend;
				delayed_skipfirst = skipfirst;
				delayed_buf = buf;
				delayed_next = blknr +
					(blockend >> log2blksz);
			}
		} else {
			int n;
			int n_left;
			if (previous_block_number != -1) {
				/* spill */
				err = ext4fs_devread(delayed_start,
							delayed_skipfirst,
							delayed_extent,
							delayed_buf);
				if (err) {
                    printf("[EXT_FS] %s read error %d\n", __func__, err);
					ext_cache_fini(&cache);
					return -1;
				}
				previous_block_number = -1;
			}
           
			/* Zero no more than `len' bytes. */
			n = blocksize - skipfirst;
			n_left = len - ( buf - start_buf );
			if (n > n_left)
				n = n_left;
			memset(buf, 0, n);
		}
		buf += blocksize - skipfirst;
	}
      
	if (previous_block_number != -1) {
		/* spill */
		err = ext4fs_devread(delayed_start,
					delayed_skipfirst, delayed_extent,
					delayed_buf);
		if (err) {
            printf("[EXT_FS] %s read error %d\n", __func__, err);
			ext_cache_fini(&cache);
			return -1;
		}
		previous_block_number = -1;
	}

	*actread  = len;
//   debug("[EXT_FS] %s &cache 0x%X cache.buf 0x%X\n", __func__, &cache, cache.buf);
	ext_cache_fini(&cache);
   
	return 0;
}


int 
ext_cache_read(struct ext_block_cache *cache, uint64_t block, int size) {   
	// This could be more lenient, but this is simple and enough for now
	if (cache->buf && cache->block == block && cache->size == size)
		return 1;
 
	ext_cache_fini(cache);
	//cache->buf = memalign(ARCH_DMA_MINALIGN, size);
    cache->buf = (char *)malloc(size);
    debug("[EXT_FS] %s cache->buf 0x%X\n", __func__, cache->buf);
	if (!cache->buf)
		return 0;
	if (!ext4fs_devread(block, 0, size, cache->buf)) {
		ext_cache_fini(cache);
		return 0;
	}
	cache->block = block;
	cache->size = size;
	return 1;
}

int bootfs_init(void) {
    char *splash_bmp;
    uintptr_t fsize = 0;
    uint64_t len_read;
    ext4fs_mount();
    ext4fs_ls("/spacemit");
#if (defined(__JH7110__) || defined(__SPACEMIT_K1__))	
    ext4fs_size("bianbu.bmp", &fsize);
  
    printf("Size of file is %d\n",fsize);
    splash_bmp = (char *)malloc(fsize);
    ext4_read_file("bianbu.bmp", splash_bmp, 0, fsize, &len_read);
    printf("Read of %d\n",len_read);
    video_bmp_display((uint8_t *)FB, splash_bmp, 900, 450);

	mfree(splash_bmp);
#endif
	return 0;
}