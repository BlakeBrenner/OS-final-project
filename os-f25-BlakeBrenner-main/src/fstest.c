/*

This is the activity we did in class on 11/3/2025. This file reads the
superblock from a FAT-formatted disk.img (aka bios information block in FAT
parlance) and prints out some parameters.


To create the disk image:

rambler@system ~ $ dd if=/dev/zero of=disk.img bs=1M count=8192
rambler@system ~ $ mkfs.vfat -F 16 disk.img

*/

#include "fat.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

char sector_buf[512];
char rde_reigon[16384];
int fd = 0;


/*
   read_sector_from_disk_image()

   Reads one sector from a disk image. Assumes global variable fd represents an
   open file descriptor for the disk image.

   This is a Linux implementation of ata_lba_read(), which is provided in ide.s,
   that reads from disk by directly accessing the hardware ATA controller. Since
   we're experimenting with disk images in Linux, we don't want to directly
   access the ATA controller.

*/
int read_sector_from_disk_image(unsigned int sector_num, char *buf, unsigned int nsectors) {
  // position the OS index 
  lseek(fd, sector_num * 512, SEEK_SET);

  // Read one sector from disk image
  int n = read(fd, buf, 512 * nsectors);
 }

 //strcpy()
 void array2array(char *dest[], char *src[]){
  // copy from arr1 to arr2
  int k = 0;
  while(dest[k] != 0){
    src[k] = dest[k];
    k++;
  }
 }

 //strcpy() for fat
 void extract_fname(struct root_directory_entry *rde, char *fname){
  // copy from arr1 to arr2
  int k = 0;
  while (rde->file_name[k] != ' ') && (k < 8){
    fname[k] = (rde->file_name)[k];
    k++;
  }
  fname[k] = '\0';
  if(rde->file_extension[0] == ' '){
    return;
  }
  fname[k++] = '.'; //add dot
  fname[k++] = '\0';//add null terminator
  int n = 0
  while (rde->file_extension[k] != ' ') && (n < 3){
    fname[k] = (rde->file_extension)[n];
    k++;
    n++;
  }
  fname[k] = '\0';
 }
int main() {

  struct boot_sector *bs = (struct boot_sector*)sector_buf;
  struct root_directory_entry *rde_tbl =(struct root_directory_entry*) rde_reigon;
  // Open disk image file. fd is a global to make it accessible to
  // read_sector_from_disk_image()
  fd = open("disk.img", O_RDONLY);

  // Call our function to read a sector from the disk image.
  read_sector_from_disk_image(0, sector_buf, 1);

  // not weird way to print sectors per cluster
  //printf("sectors per cluster = %d\n", sector_buf[13]);

  // Print info from superblock
  printf("sectors per cluster = %d\n", bs->num_sectors_per_cluster);
  printf("reserved sectors = %d\n", bs->num_reserved_sectors);
  printf("num fat tables = %d\n", bs->num_fat_tables);
  printf("num RDEs = %d\n", bs->num_root_dir_entries);

  //read RDE reigon replace 0 with 2048 for homework
  read_sector_from_disk_image( 0 + bs->num_reserved_sectors + bs->num_fat_tables * bs->num_sectors_per_fat, //sector num
  			       rde_reigon, // buffer to hold the RDE table
			       32);// Number of sectors in the RDE Table
  for(int k = 0; k < 512; k++){
    char temp_str[10];
    strcpy_fat(temp_str, rde_tbl[k].file_name);
    extract_fname(&rde_tbl[k], temp_str);
    printf("fname = %s\n", rde_tbl[k].file_name);
  }

  return 0;
}
