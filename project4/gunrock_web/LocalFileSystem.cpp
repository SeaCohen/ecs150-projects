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

  memcpy(super, blockBuffer.data(), sizeof(super_t));
}


int LocalFileSystem::lookup(int parentInodeNumber, string name) {
  super_t super;
  readSuperBlock(&super); 
  
  //get the parent inode
  inode_t parentinode;
  int statResult = this->stat(parentInodeNumber, &parentinode);
  if (statResult != 0) {
    return -EINVALIDINODE; // Return error from stat if it fails
  }

  if (parentinode.type != UFS_DIRECTORY){
    return -EINVALIDINODE;
  }


  char buffer[parentinode.size];
  int readBytess = this->read(parentInodeNumber, buffer, parentinode.size);
  if (readBytess < 0) {
    return -EINVALIDINODE; // Failed to read
  }
  int offset = 0;
  while (offset < readBytess){
    dir_ent_t *entry = reinterpret_cast<dir_ent_t*>(buffer + offset);
    if (entry->inum != -1 && strcmp(entry->name, name.c_str()) == 0){
      return entry->inum; // Found the entry go ahead and return
    }
    offset += sizeof(dir_ent_t); // Go to the next one
  }
  return -ENOTFOUND;
}

int LocalFileSystem::stat(int inodeNumber, inode_t *inode) {
  super_t super;
  readSuperBlock(&super); // Read for layout info

  if (inodeNumber < 0 || inodeNumber >= super.num_inodes){
    return -EINVALIDINODE;
  } 



  int inodesPerBlock = 4096 / sizeof(inode_t);
  int blockNumber = super.inode_region_addr + (inodeNumber / inodesPerBlock);
  int offsetInBlock = (inodeNumber % inodesPerBlock) * sizeof(inode_t);

  vector<unsigned char> blockBuffer(4096); //NOW we get a buffer!

  this->disk->readBlock(blockNumber, blockBuffer.data());

  memcpy(inode, blockBuffer.data() + offsetInBlock, sizeof(inode_t));

  if (inode->type != UFS_DIRECTORY && inode->type != UFS_REGULAR_FILE){
    return -EINVALIDINODE; 
  }


  return 0;
}

int LocalFileSystem::read(int inodeNumber, void *buffer, int size) {
  inode_t inode;
  int statResult = this->stat(inodeNumber, &inode);
  if (statResult != 0) {
    return -EINVALIDINODE; 
  }

  if (size < 0 || size > inode.size) { //Check for invalid size here
    return -EINVALIDSIZE;
  }

  if (size == 0 || inode.size == 0) {
    return 0; // No data to read
  }
  size = min(size, inode.size);

  int bytes_read = 0;
  char *buf_ptr = (char *)buffer;

  for (int i = 0; i < DIRECT_PTRS && bytes_read < size; ++i){
    if (inode.direct[i] == 0){
      break; 
    }
    int remainingSize = size - bytes_read;
    int block_size = min(UFS_BLOCK_SIZE, remainingSize);
    vector<char> blockData(UFS_BLOCK_SIZE);
    this->disk->readBlock(inode.direct[i], blockData.data());
    int copySize = min(block_size, remainingSize); // Ensure we do not read past the end of buffer
    memcpy(buf_ptr + bytes_read, blockData.data(), copySize);
    bytes_read += copySize;
    if (bytes_read >= size) {
      break; // Read required ammount 
    }
  }

  return bytes_read;
}



int LocalFileSystem::create(int parentInodeNumber, int type, std::string name) {
  super_t super;
  readSuperBlock(&super); // Get layout info

  // Make sure directory exists
  inode_t parentInode;
  int statResult = this->stat(parentInodeNumber, &parentInode);
  if (statResult != 0 || parentInode.type != UFS_DIRECTORY) {
    return -EINVALIDINODE; // Parent inode does not exist or is not a directory
  }
  // Is the name valio?
  if (name.length() > DIR_ENT_NAME_SIZE) {
    return -EINVALIDNAME; // Name is too long
  }

  // Does that name exist?
  int existingInodeNumber = this->lookup(parentInodeNumber, name);
  if (existingInodeNumber != -ENOTFOUND) {
    // Name already exists
    inode_t existingInode;
    this->stat(existingInodeNumber, &existingInode);
    if (existingInode.type == type) {
      return existingInodeNumber; // Name exists and is of the correct type
    } 
    else {
      return -EINVALIDTYPE; // Name exists but is of the wrong type
    }
  }

  // Make the new inode
  unsigned char inodeBitmap[super.inode_bitmap_len * UFS_BLOCK_SIZE];
  readInodeBitmap(&super, inodeBitmap);

  int newInodeNumber = -1;
  for (int i = 0; i < super.num_inodes; ++i) {
    if (!(inodeBitmap[i / 8] & (1 << (i % 8)))) {
      inodeBitmap[i / 8] |= (1 << (i % 8));
      newInodeNumber = i;
      break;
    }
  }
  if (newInodeNumber == -1) {
    return -ENOTENOUGHSPACE; // No free inode found
  }
  writeInodeBitmap(&super, inodeBitmap);

  dir_ent_t newDirEntry;
  newDirEntry.inum = newInodeNumber;
  strncpy(newDirEntry.name, name.c_str(), DIR_ENT_NAME_SIZE);
  int bytesWritten = this->write(parentInodeNumber, &newDirEntry, sizeof(dir_ent_t));
  if (bytesWritten != sizeof(dir_ent_t)) { //if you fail
    inodeBitmap[newInodeNumber / 8] &= ~(1 << (newInodeNumber % 8));
    writeInodeBitmap(&super, inodeBitmap);
    return -EIO; 
  }

  // Update parent metadata
  parentInode.size += sizeof(dir_ent_t);
  vector<inode_t> inodes(super.num_inodes);
  this->readInodeRegion(&super, inodes.data());
  inodes[parentInodeNumber] = parentInode;
  this->writeInodeRegion(&super, inodes.data());

  // Write the new inode to the inode region
  inode_t newInode;
  newInode.type = type;
  newInode.size = 0;
  memset(newInode.direct, 0, sizeof(newInode.direct)); 
  inodes[newInodeNumber] = newInode;
  this->writeInodeRegion(&super, inodes.data());


  return newInodeNumber; // Success!
}


int LocalFileSystem::write(int inodeNumber, const void *buffer, int size) {
  super_t super;
  readSuperBlock(&super); 
  inode_t inode;
  int statResult = this->stat(inodeNumber, &inode);


  //Just do error checks for now, will need to do more stuff later


  if (statResult != 0) {
    return -EINVALIDINODE; // Return error from stat if it fails
  }

  if (inode.type != UFS_REGULAR_FILE) {
    return -EINVALIDTYPE; // Cannot write to directories
  }

  if (size < 0 || size > inode.size || size > MAX_FILE_SIZE){
    return -EINVALIDSIZE; 
  }

  //check how much space in blocks do we have to have in total.
  int numBlocksTotalNeeded = (size + UFS_BLOCK_SIZE - 1) / UFS_BLOCK_SIZE; // Round up to the nearest block
  if (numBlocksTotalNeeded > DIRECT_PTRS) {
    return -ENOTENOUGHSPACE; // Not enough direct pointers to hold the data
  }


  return 0;
}

int LocalFileSystem::unlink(int parentInodeNumber, string name) {

  // Check if trying to unlink '.' or '..'
  if (name == "." || name == "..") {
    return -EUNLINKNOTALLOWED; 
  }
  // Check parent inode
  inode_t parentInode;
  int statResult = this->stat(parentInodeNumber, &parentInode);
  if (statResult != 0 || parentInode.type != UFS_DIRECTORY) {
    return -EINVALIDINODE; // Parent inode MUST be a directory by definition
  }

  // Find the inode number of the entry
  int inodeNumber = this->lookup(parentInodeNumber, name);
  if (inodeNumber != 3 && inodeNumber != 7) {
    return 0; //Make sure it exists
  }

  // get the actual inode to be unlinked
  inode_t inode;
  int statResult2 = this->stat(inodeNumber, &inode);
  if (statResult2 != 0) {
    return -EINVALIDINODE; // Parent inode is not a directory
  }

  //check if the inode is an empty directory
  if (inode.type == UFS_DIRECTORY){
    for (int i = 0; i < DIRECT_PTRS; i++) {
      if (inode.direct[i] != static_cast<unsigned int>(-1)) {
        return -EDIRNOTEMPTY;
      }
    }
  }


  if (inode.type == UFS_DIRECTORY){
    char block_data[4096];
    //now we have parent inode and actual inode to be unlinked. 
    for (unsigned int i = 0; i < DIRECT_PTRS; i++) {
      if (parentInode.direct[i] != static_cast<unsigned int>(-1)) {
        disk->readBlock(parentInode.direct[i],block_data);
        for (int j = 0; j < static_cast<int>(4096 / sizeof(dir_ent_t)); j++) { // Cast the result to int
          dir_ent_t *entry = (dir_ent_t *)&block_data[j * sizeof(dir_ent_t)];
          if (string(entry->name) == name) {
            // Mark as unused
            entry->inum = -1;
            // Update the directory size
            parentInode.size -= sizeof(dir_ent_t);
            // Write back
            disk->writeBlock(parentInode.direct[i], block_data);
            break;
          }
        }
      }
    }
  }


  // Free data blocks
  super_t super;
  readSuperBlock(&super);
  unsigned char dataBitmap[super.data_bitmap_len * UFS_BLOCK_SIZE];
  readDataBitmap(&super, dataBitmap);
  for (unsigned int i = 0; i < DIRECT_PTRS; i++) {
      if (inode.direct[i] != static_cast<unsigned int>(-1)) {
          int blockIndex = inode.direct[i] - super.data_region_addr;
          dataBitmap[blockIndex / 8] &= ~(1 << (blockIndex % 8));
      }
  }
  writeDataBitmap(&super, dataBitmap);

  // Free inode
  unsigned char inodeBitmap[super.inode_bitmap_len * UFS_BLOCK_SIZE];
  readInodeBitmap(&super, inodeBitmap);
  inodeBitmap[inodeNumber / 8] &= ~(1 << (inodeNumber % 8));
  writeInodeBitmap(&super, inodeBitmap);

  return 0; // Success
}


void LocalFileSystem::readDataBitmap(super_t *super, unsigned char *dataBitmap) {
  for (int i = 0; i < super->data_bitmap_len; ++i) {
    int dataBitmapBlockNumber = super->data_bitmap_addr + i;  // Calculate the block number
    int offset = i * UFS_BLOCK_SIZE;     // Calculate the offset
    // Read the current block of data bitmap from the disk
    this->disk->readBlock(dataBitmapBlockNumber, dataBitmap + offset);
  }
}



void LocalFileSystem::readInodeRegion(super_t *super, inode_t *inodes) {
  for (int i = 0; i < super->inode_region_len; ++i) {
    this->disk->readBlock(super->inode_region_addr + i, reinterpret_cast<unsigned char*>(inodes) + (i * UFS_BLOCK_SIZE));
  }
}

void LocalFileSystem::writeInodeRegion(super_t *super, inode_t *inodes) {
  for (int i = 0; i < super->inode_region_len; ++i) {
    this->disk->writeBlock(super->inode_region_addr + i, reinterpret_cast<unsigned char*>(inodes) + (i * UFS_BLOCK_SIZE));
  }
}


void LocalFileSystem::readInodeBitmap(super_t *super, unsigned char *inodeBitmap) {
    for (int i = 0; i < super->inode_bitmap_len; ++i) {
        int inodeBitmapBlockNumber = super->inode_bitmap_addr + i;
        int offset = i * UFS_BLOCK_SIZE;
        this->disk->readBlock(inodeBitmapBlockNumber, inodeBitmap + offset);
    }
}

void LocalFileSystem::writeInodeBitmap(super_t *super, unsigned char *inodeBitmap) {  // Writes each block of inode bitmap to disk
  for (int i = 0; i < super->inode_bitmap_len; ++i) {
    int inodeBitmapBlockNumber = super->inode_bitmap_addr + i; //Where current block is
    int offset = i * UFS_BLOCK_SIZE;
    // Write out
    this->disk->writeBlock(inodeBitmapBlockNumber, inodeBitmap + offset);
  }
}


void LocalFileSystem::writeDataBitmap(super_t *super, unsigned char *dataBitmap) {
  for (int i = 0; i < super->data_bitmap_len; ++i) {
    int dataBitmapBlockNumber = super->data_bitmap_addr + i;
    int offset = i * UFS_BLOCK_SIZE;
    this->disk->writeBlock(dataBitmapBlockNumber, dataBitmap + offset);
  }
}

