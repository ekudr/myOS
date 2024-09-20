#include <common.h>
#include <mmc.h>
#include <part.h>
#include <ext4.h>
#include <linux/errno.h>

struct ext_filesystem ext_fs;
extern disk_t boot_disk;

// read FS from device
int 
fs_devread(uint64_t sector, int byte_offset, int byte_len, char *buf) {
	unsigned block_len;
	int log2blksz, err;
    int blksz = 512;
    
//	ALLOC_CACHE_ALIGN_BUFFER(char, sec_buf, (blk ? blk->blksz : 0));
    char sec_buf[blksz];

	log2blksz = 9; //log2(blksz)

//    debug("read %d bytes from sector %d, offset %d\n", byte_len, sector, byte_offset);
//    debug("partition start %d total sectors %d\n", ext_fs.start_sect, ext_fs.total_sect);
	// Check partition boundaries
	if ((sector + ((byte_offset + byte_len - 1) >> log2blksz))
	    >= ext_fs.total_sect * blksz) {
		printf("[EXT_FS] read outside partition %d\n", sector);
		return EINVAL;
	}

	/* Get the read to the beginning of a partition */
	sector += byte_offset >> log2blksz;
	byte_offset &= blksz - 1;

//	debug(" < %d, %d, %d>\n", sector, byte_offset, byte_len);

	if (byte_offset != 0) {
		int readlen;
		// read first part which isn't aligned with start of sector
        err = boot_disk.mmc->bread((void *)sec_buf, ext_fs.start_sect + sector, 1); 
        if (err) {
            printf("[DISK] ** %s read error %d **\n", __func__, err);
            return err;  
        }
		readlen = min((int)blksz - byte_offset,
			      byte_len);
		sbi_memcpy(buf, sec_buf + byte_offset, readlen);
		buf += readlen;
		byte_len -= readlen;
		sector++;
	}

	if (byte_len == 0)
		return 0;

	// read sector aligned part
	block_len = byte_len & ~(blksz - 1);

	if (block_len == 0) {
	//	ALLOC_CACHE_ALIGN_BUFFER(u8, p, blk->blksz);
        u8 p[blksz];

		block_len = blksz;

        err = boot_disk.mmc->bread((void *)p, ext_fs.start_sect + sector, 1); 
        if (err) {
            printf("[EXT_FS] read BOOTFS return %d\n", err);
            return err;  
        }
		sbi_memcpy(buf, p, byte_len);
		return 0; // success
	}

    err = boot_disk.mmc->bread((void *)buf, ext_fs.start_sect + sector, block_len >> log2blksz); 
    if (err) {
        printf("[DISK] ** %s block read error %d\n", __func__, err);
        return err;  
    }
	
	block_len = byte_len & ~(blksz - 1);
	buf += block_len;
	byte_len -= block_len;
	sector += block_len / blksz;

	if (byte_len != 0) {
		// read rest of data which are not in whole sector 
        err = boot_disk.mmc->bread((void *)sec_buf, ext_fs.start_sect + sector, 1); 
        if (err) {
            printf("[DISK] read BOOTFS return %d\n", err);
            return err;  
        }
		sbi_memcpy(buf, sec_buf, byte_len);
	}
	return 0;
}