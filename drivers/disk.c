#include <common.h>
#include <part.h>

disk_t boot_disk;

int boot_disk_init(void){
    int err;
    gpt_header *gpt;
    gpt_entry *gpt_en;
    uint64_t next_lba;

    if(!boot_disk.dev_inited) {
        panic("NO BOOT DEVICE");
    }
    

    gpt = (gpt_header *)malloc(512);

    err = boot_disk.bread(gpt, 1, 1); 

    if (err) {
        printf("[DISK] read GPT header return %d\n", err);
        return -1;  
    }
    


    if (gpt->signature != GPT_HEADER_SIGNATURE_UBOOT) {
        return -1;
    }

    printf("[DISK] GPT: revision 0x%X\n", gpt->revision);
    printf("[DISK] GPT: GUID ");
    for (int i = 0; i < 16; i++) {
        printf("%X ", gpt->disk_guid.b[i]);
    }
    printf("\n");
    printf("[DISK] GPT: partition entry lba 0x%lX\n", gpt->partition_entry_lba);
    printf("[DISK] GPT: num partition entries %d\n", gpt->num_partition_entries);
    printf("[DISK] GPT: size of partition entry %d\n", gpt->sizeof_partition_entry);
    
    next_lba = gpt->partition_entry_lba;
    
    printf("[DISK] Allocating %d bytes for gpt entries\n",gpt->sizeof_partition_entry*gpt->num_partition_entries);
    gpt_en = (gpt_entry *)malloc(gpt->sizeof_partition_entry*gpt->num_partition_entries);
    
    err =  boot_disk.bread(gpt_en, next_lba, gpt->sizeof_partition_entry*gpt->num_partition_entries/0x200);
    if (err) {
        printf("[DISK] read GPT entry return %d\n", err);
        return -1;  
    }

    for (int i=0; i< 4/*gpt->num_partition_entries*/; i++) {
 //       if(gpt_en[i].partition_type_guid.b[0]) { 
            printf("[DISK] partition %d type GUID ", i);
            for (int i = 0; i < 16; i++) {
                printf("%X ", gpt_en[i].partition_type_guid.b[i]);
            }
            printf("\n");
            printf("[DISK] unique GUID ");
            for (int i = 0; i < 16; i++) {
                printf("%X ", gpt_en[i].unique_partition_guid.b[i]);
            }
            printf("\n");            
            printf("[DISK] GPT: partition %i lbas 0x%lX -> 0x%lX\n", i, gpt_en[i].starting_lba, gpt_en[i].ending_lba);

            if(i == 2) 
                boot_disk.fat_lba = gpt_en[i].starting_lba;
//        }
    }

    mfree(gpt_en);
    mfree(gpt);

    printf("[DISK] boot fat partition starts at 0x%X\n", boot_disk.fat_lba);

    return 0;
}