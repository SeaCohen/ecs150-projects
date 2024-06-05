#include <iostream>
#include <string>
#include <algorithm>
#include <cstring>
#include <cmath>

#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"

using namespace std;

void ds3cat(LocalFileSystem &fs, int inodeNumber) {
  inode_t myInode;
  if (fs.stat(inodeNumber, &myInode) != 0) { //Invalid
    //cerr << "ERROR IN DS3CAT INODE: " << inodeNumber << endl;
    return;
  }

  // Print file blocks
  cout << "File blocks" << endl;
  int numBlocks = static_cast<int>(std::ceil(static_cast<double>(myInode.size) / UFS_BLOCK_SIZE)); // Calculate num of blocks using ceiling 
    for (int i = 0; i < numBlocks && i < DIRECT_PTRS; ++i) {
      if (myInode.direct[i] != 0) {
        cout << myInode.direct[i] << endl;
      }
    }
  cout << endl;

  // Print file data
  cout << "File data" << endl;
  int fileSize = myInode.size;
  char buffer [MAX_FILE_SIZE];
  fs.read(inodeNumber, &buffer, fileSize);
  cout << buffer;

}


int main(int argc, char *argv[]) {
  if (argc != 3) {
    cout << argv[0] << ": diskImageFile inodeNumber" << endl;
    return 1;
  }

  string diskimage = argv[1];
  int inodeNumber = atoi(argv[2]);
  Disk disk(diskimage, 4096); //disk instance. 
  LocalFileSystem filesystem(&disk); //filesystem instance.

  ds3cat(filesystem, inodeNumber);

}
