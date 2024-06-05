#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sstream>
#include <iostream>
#include <map>
#include <string>
#include <algorithm>
#include <sys/stat.h>
#include <dirent.h>

#include "DistributedFileSystemService.h"
#include "ClientError.h"
#include "ufs.h"
#include "WwwFormEncodedDict.h"
#include "Disk.h"
#include <cstring>

using namespace std;

DistributedFileSystemService::DistributedFileSystemService(string diskFile) : HttpService("/ds3/") {
  this->fileSystem = new LocalFileSystem(new Disk(diskFile, UFS_BLOCK_SIZE));
}  

void DistributedFileSystemService::get(HTTPRequest *request, HTTPResponse *response){
  string myPath = request->getPath();
  string result;
  struct stat myPath_stat;

  stat(myPath.c_str(), &myPath_stat);
  if (S_ISDIR(myPath_stat.st_mode)){
    DIR *dir;
    struct dirent *entry;
    vector<string> entries;

    if ((dir = opendir(myPath.c_str())) != NULL){
      while ((entry = readdir(dir)) != NULL){
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0){
          string entryName = entry->d_name;
          if (entry->d_type == DT_DIR){
            entryName += "/";
          }
          entries.push_back(entryName);
        }
      }
      closedir(dir);
      sort(entries.begin(), entries.end());

      // For each loop has a chance to shine!
      for (vector<string>::const_iterator it = entries.begin(); it != entries.end(); ++it){
        result += *it + "\n";
      }
    }
    else{
      // directory not found
      throw ClientError::notFound();
    }
  }
  else{
    // Handle file reading
    int fd = open(myPath.c_str(), O_RDONLY);
    if (fd < 0){
      throw ClientError::notFound();
    }
    else{
      int ret;
      char buffer[4096];
      while ((ret = read(fd, buffer, sizeof(buffer))) > 0){
        result.append(buffer, ret);
      }
      close(fd);
    }
  }

  response->setBody(result);
}

void DistributedFileSystemService::put(HTTPRequest *request, HTTPResponse *response) {
  string path = request->getPath();
  string data = request->getBody();
  struct stat path_stat;

  // Trim relative path
  if (path.find("/ds3/") == 0){
    path = path.substr(5);
  }

  // Check if the path actually points somewhere
  if (path.back() == '/'){
    throw ClientError::badRequest();
    return;
  }

  string dirPath = path.substr(0, path.rfind('/'));

  // Create directories as needed
  size_t pos = 0;
  string token;
  while ((pos = dirPath.find('/', pos)) != string::npos){
    string currentDir = dirPath.substr(0, pos);
    stat(currentDir.c_str(), &path_stat);
    if (stat(currentDir.c_str(), &path_stat) == 0){
      if (!S_ISDIR(path_stat.st_mode)){
        throw ClientError::conflict();
      }
    }
    else
    {
      // Directory does not exist, create it
      if (mkdir(currentDir.c_str(), 0777) != 0){
        throw ClientError::insufficientStorage();
      }
    }
    pos++;
  }

  // Start transaction for file creation or update
  fileSystem->disk->beginTransaction();

  try{
    int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0){
      fileSystem->disk->rollback();
      throw ClientError::insufficientStorage();
    }

    if (write(fd, data.c_str(), data.size()) != (ssize_t)data.size()){
      close(fd);
      fileSystem->disk->rollback();
      throw ClientError::insufficientStorage();
    }
    close(fd);

    fileSystem->disk->commit();
    response->setStatus(200);
    response->setBody("File created/updated successfully");
  }
  catch (...){
    fileSystem->disk->rollback();
    throw;
  }
}

void DistributedFileSystemService::del(HTTPRequest *request, HTTPResponse *response) {
  string myPath = request->getPath(); //remove the myPath
  response->setBody("");
}
