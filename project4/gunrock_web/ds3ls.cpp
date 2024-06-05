#include <iostream>
#include <string>
#include <algorithm>
#include <cstring>
#include <vector>

#include "LocalFileSystem.h"
#include "Disk.h"
#include "ufs.h"

using namespace std;
bool compareDirEnt(const dir_ent_t &a, const dir_ent_t &b){
  return strcmp(a.name, b.name) < 0;
}

void printdirectory(LocalFileSystem &fs, int inodeNum, const string &path){

  inode_t myInode;
  if (fs.stat(inodeNum, &myInode) != 0    ||    myInode.type != UFS_DIRECTORY){
    return;
  }


  // Read directory 
  vector<dir_ent_t> entries;  //file or directory
  char buffer[UFS_BLOCK_SIZE * DIRECT_PTRS]; 
  int bytesRead = fs.read(inodeNum, buffer, myInode.size);
  int offset = 0;

  //Now to actually do stuff


  while (offset < bytesRead){
    dir_ent_t *entry = reinterpret_cast<dir_ent_t *>(buffer + offset);
    if (entry->inum != -1){ 
      // Valid directory 
      entries.push_back(*entry);
    }
    offset += sizeof(dir_ent_t);
  }

  // Sorting directory entries by name
  sort(entries.begin(), entries.end(), compareDirEnt);

  cout << "Directory " << path << endl;

  //print directory contents
  for (vector<dir_ent_t>::const_iterator it = entries.begin(); it != entries.end(); ++it){
    cout << it->inum << "\t" << it->name << endl;
  }
  cout << endl;

  // Traverse them all!
  for (vector<dir_ent_t>::const_iterator it = entries.begin(); it != entries.end(); ++it){

    if (strcmp(it->name, ".") == 0 || strcmp(it->name, "..") == 0){
      continue; 
    }

    string newPath = path + (path.back() == '/' ? "" : "/") + it->name + "/";
    printdirectory(fs, it->inum, newPath);
  }
}



int main(int argc, char *argv[]){
  if (argc != 2)
  {
    cout << argv[0] << ": diskImageFile" << endl;
    return 1;
  }
  
  string diskimage = argv[1];
  Disk disk(diskimage, 4096); //disk instance. 
  LocalFileSystem filesystem(&disk); //filesystem instance.

  printdirectory(filesystem, UFS_ROOT_DIRECTORY_INODE_NUMBER, "/");

  return 0;
}
