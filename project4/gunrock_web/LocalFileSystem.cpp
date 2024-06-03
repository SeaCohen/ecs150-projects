#include <iostream>
#include <string>
#include <vector>
#include <assert.h>

#include "LocalFileSystem.h"
#include "ufs.h"
#include <cstring>

using namespace std;


LocalFileSystem::LocalFileSystem(Disk *disk){
  this->disk = disk;
}

void LocalFileSystem::readSuperBlock(super_t *super){
  const int blockSize = 4096;
  vector<unsigned char> blockBuffer(blockSize); // Buffer to hold the block data
  this->disk->readBlock(0, blockBuffer.data()); // Read into buffer

  // blockBuffer.data() is a pointer to the beginning of the buffer
  memcpy(super, blockBuffer.data(), sizeof(super_t));
}
int LocalFileSystem::lookup(int parentInodeNumber, string name) {
  return 0;
}

int LocalFileSystem::stat(int inodeNumber, inode_t *inode) {
  super_t super;
  readSuperBlock(&super); // First, read the superblock to get layout information

  if (inodeNumber >= super.num_inodes){
    return -EINVALIDINODE; // Check if inode number is valid
  }

  if (inodeNumber < 0 || inodeNumber >= super.num_inodes){
    return -EINVALIDINODE; // Check if inode number is valid
  } 



  int inodesPerBlock = 4096 / sizeof(inode_t);
  int blockNumber = super.inode_region_addr + (inodeNumber / inodesPerBlock);
  int offsetInBlock = (inodeNumber % inodesPerBlock) * sizeof(inode_t);

  vector<unsigned char> blockBuffer(4096);
  this->disk->readBlock(blockNumber, blockBuffer.data());

  memcpy(inode, blockBuffer.data() + offsetInBlock, sizeof(inode_t));

  if (inode->type != UFS_DIRECTORY && inode->type != UFS_REGULAR_FILE){
    return -EINVALIDINODE; // Verify the inode type
  }

  return 0;
}

int LocalFileSystem::read(int inodeNumber, void *buffer, int size) {
  inode_t inode;
  int statResult = this->stat(inodeNumber, &inode);
  if (statResult != 0) {
    return -EINVALIDINODE; // Return error from stat if it fails
  }

  if (size < 0 || size > inode.size) {
    return -EINVALIDSIZE; // Error if the requested size is invalid
  }

  if (size == 0 || inode.size == 0) {
    return 0; // No data to read
  }
  size = min(size, inode.size); // Adjust size to not exceed the file size

  int bytes_read = 0;
  char *buf_ptr = (char *)buffer;

  for (int i = 0; i < DIRECT_PTRS && bytes_read < size; ++i){
    if (inode.direct[i] == 0){
      break; // End of data blocks
    }

    int remainingSize = size - bytes_read;
    int block_size = min(UFS_BLOCK_SIZE, remainingSize);
    vector<char> blockData(UFS_BLOCK_SIZE);
    this->disk->readBlock(inode.direct[i], blockData.data());

    int copySize = min(block_size, remainingSize); // Ensure we do not read past the end of buffer
    memcpy(buf_ptr + bytes_read, blockData.data(), copySize);
    bytes_read += copySize;

    if (bytes_read >= size) {
      break; // Stop if we have read the required amount
    }
  }

  return bytes_read;
}

int LocalFileSystem::create(int parentInodeNumber, int type, string name) {
  return 0;
}

int LocalFileSystem::write(int inodeNumber, const void *buffer, int size) {
  return 0;
}

int LocalFileSystem::unlink(int parentInodeNumber, string name) {
  return 0;
}


void LocalFileSystem::readDataBitmap(super_t *super, unsigned char *dataBitmap) {
  // Read each block of the data bitmap from the disk
  for (int i = 0; i < super->data_bitmap_len; ++i) {
    // Calculate the block number where the current block of data bitmap is located
    int dataBitmapBlockNumber = super->data_bitmap_addr + i;

    // Calculate the offset within the dataBitmap buffer
    int offset = i * UFS_BLOCK_SIZE;

    // Read the current block of data bitmap from the disk
    this->disk->readBlock(dataBitmapBlockNumber, dataBitmap + offset);
  }
}



void LocalFileSystem::readInodeRegion(super_t *super, inode_t *inodes) {
  // Calculate the total number of bytes to read
  //int totalBytes = super->inode_region_len * UFS_BLOCK_SIZE;

  // Read the inode region block by block
  for (int i = 0; i < super->inode_region_len; ++i) {
      this->disk->readBlock(super->inode_region_addr + i, reinterpret_cast<unsigned char*>(inodes) + (i * UFS_BLOCK_SIZE));
  }
}



void LocalFileSystem::readInodeBitmap(super_t *super, unsigned char *inodeBitmap) {
    // Read each block of the inode bitmap from the disk
    for (int i = 0; i < super->inode_bitmap_len; ++i) {
        // Calculate the block number where the current block of inode bitmap is located
        int inodeBitmapBlockNumber = super->inode_bitmap_addr + i;

        // Calculate the offset within the inodeBitmap buffer
        int offset = i * UFS_BLOCK_SIZE;

        // Read the current block of inode bitmap from the disk
        this->disk->readBlock(inodeBitmapBlockNumber, inodeBitmap + offset);
    }
}
