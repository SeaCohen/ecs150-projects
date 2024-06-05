#include <iostream>
#include <string>
#include <algorithm>
#include <cstring>

#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"

using namespace std;

void printBitmaps(LocalFileSystem &fs, Disk &disk){

  super_t super;
  fs.readSuperBlock(&super);

  cout<<"Super"<<endl;
  cout<<"inode_region_addr "<<super.inode_region_addr<<endl;
  cout<<"data_region_addr "<<super.data_region_addr<<endl;
  cout<<endl;

  cout<<"Inode bitmap"<<endl;
  unsigned char inodebuffer[super.inode_bitmap_len * UFS_BLOCK_SIZE]; //len is in blocks

  //read
  for (int i=0; i<super.inode_bitmap_len;i++){
    disk.readBlock(super.inode_bitmap_addr + i, &inodebuffer[i * UFS_BLOCK_SIZE]);
  }

  //then print
  for (int i=0; i<super.inode_bitmap_len * UFS_BLOCK_SIZE;i++){
    cout << (unsigned int)inodebuffer[i] << " ";
  }
  cout << endl;
  cout << endl; // Have a blank line as a break


  //data bitmap!

  cout << "Data bitmap" << endl;

  //need a buffer for data bitmap
  unsigned char buffer[super.data_bitmap_len * UFS_BLOCK_SIZE];
  // read the data_bitmap into a buffer
  for (int i = 0; i < super.data_bitmap_len; i++){
    disk.readBlock(super.data_bitmap_addr + i, &buffer[i * UFS_BLOCK_SIZE]);
  }

  //print the data bitmap
  for (int i = 0; i < super.data_bitmap_len * UFS_BLOCK_SIZE; i++){
    cout << (unsigned int)buffer[i] << " ";
  }
  cout << endl;

}


int main(int argc, char *argv[]) {
  if (argc != 2) {
    cout << argv[0] << ": diskImageFile" << endl;
    return 1;
  }

  string diskimage = argv[1];
  Disk disk(diskimage, 4096); //disk instance. 
  LocalFileSystem filesystem(&disk); //filesystem instance.

  printBitmaps(filesystem, disk);

  return 0;
}
