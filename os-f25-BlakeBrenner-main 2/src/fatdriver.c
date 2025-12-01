#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include "fat.h"

int fd = 0; 
char boot_sector[512];
char root_directory_region[512];
int root_dir_region_start = 0;
//ide.s
void driver_init(char *disk_img_path){
  fd = open(disk_img_path, O_RDONLY); 
}
//ide.s
void sector_read(unsigned int sector_num, char *buf){
  lseek(fd, sector_num * 512, SEEK_SET);
  size_t nbytes = read(fd, buf, 512);
}

//fatinit()
void fatinit(){
//read boot sector and RDE region
  sector_read(2048, boot_sector);

  printf("Number of bytes per sector = %d\n", ((struct boot_sector*)boot_sector)->bytes_per_sector);
  printf("Number of sectors per cluster = %d\n",((struct boot_sector*)boot_sector)->num_sectors_per_cluster);
  printf("Number of reserved sectors = %d\n", ((struct boot_sector*)boot_sector)->num_reserved_sectors);
  printf("Number of FAT tables = %d\n", ((struct boot_sector*)boot_sector)->num_fat_tables);
  printf("Number of RDEs = %d\n",((struct boot_sector*)boot_sector)->num_root_dir_entries);

  int root_dir_region_start = 2048
                              + ((struct boot_sector*)boot_sector)->num_reserved_sectors
                              + ((struct boot_sector*)boot_sector)->num_fat_tables +((struct boot_sector*)boot_sector)->num_sectors_per_fat;
  printf("Root directory region start (sectors) = %d\n", root_dir_region_start);

  sector_read(root_dir_region_start, root_directory_region);


}

//fatopen
//find the RDE for a file given a path
struct rde * fatOpen(char *path){
//iterate through the RDE region searching for a files RDE
  for(int k = 0; k < 10; k++){

    printf("File Name: \"%s.%s\"\n", rde[k].file_name, rde[k].file_extension);
    printf("Data Cluster: %d\n", rde[k].cluster);
    printf("File Size: %d\n", rde[k].file_size);
  }

}

int fatRead(struct rde *rde, char * buf, int n){
// read file data into buf from file described by rde
}

int main(){
  struct root_directory_entry *rde = root_directory_region;
  char dataBuf = [100];
  driver_init("disk.img");

  fatinit();
  struct rde *rde = fatOpen("file.txt");
  fatRead(rde, dataBuf, sizeof(dataBuf);

  printf("data read from file = %s\n", dataBuf);
  return 0;
}
