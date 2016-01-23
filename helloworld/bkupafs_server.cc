/*
 *
 * Copyright 2015, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <iostream>
#include <memory>
#include <string>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>

#include <grpc++/grpc++.h>

#include "afsworld.grpc.pb.h"

//using packagename::request/response (packagename from protos - afsworld.proto)

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using grpc::ServerWriter;
using grpc::ServerReader;

using afsworld::StringMessage;
using afsworld::ReadDirMessage;
using afsworld::ReadDirResponse;
using afsworld::AfsWorldService;
using afsworld::GetAttrRequest;
using afsworld::GetAttrResponse;
using afsworld::MkDirRequest;
using afsworld::MkDirResponse;
using afsworld::RmDirRequest;
using afsworld::RmDirResponse;
using afsworld::MkNodRequest;
using afsworld::MkNodResponse;
using afsworld::ReadRequest;
using afsworld::ReadResponse;
using afsworld::OpenRequest;
using afsworld::OpenResponse;
using afsworld::WriteRequest;
using afsworld::WriteResponse;
using afsworld::AccessRequest;
using afsworld::AccessResponse;
using afsworld::UTimeRequest;
using afsworld::UTimeResponse;
using afsworld::UnlinkRequest;
using afsworld::UnlinkResponse;
using afsworld::GetFileRequest;
using afsworld::GetFileResponse;
using afsworld::WriteFileRequest;
using afsworld::WriteFileResponse;
using afsworld::RenameRequest;
using afsworld::RenameResponse;

using namespace std;

//Define Class and definition of what is in the protos file

class AfsWorldServiceImpl final : public AfsWorldService::Service {
  
  Status SendText(ServerContext *context, const StringMessage *request,
                    StringMessage *reply) override {
    cout << request->stringmessage() << endl;
    string stringToReturn("Message: " + request->stringmessage());
    reply->set_stringmessage(stringToReturn);
    return Status::OK;
  }

  Status ReadDir(ServerContext *context, const ReadDirMessage *request,
                 ReadDirResponse *reply) override {
    string path = serverpath + request->path();

    DIR *dp;
    struct dirent *de;
    
    dp = opendir(path.c_str());
    if (dp == NULL) return Status::CANCELLED;

    while ((de = readdir(dp)) != NULL) {
      reply->add_inodenumber(de->d_ino);  
      reply->add_type(de->d_type);
      reply->add_name(de->d_name);
    }
    closedir(dp);
    return Status::OK;
  }

  // These functions need to put the stuff into stbuf
  // Then we need to ship stuff back to the client
  Status GetAttr(ServerContext *context, const GetAttrRequest *request,
                 GetAttrResponse *response) override {
    string path = serverpath + request->path();
    struct stat stbuf;
    int res = lstat(path.c_str(), &stbuf);
    
    cout<<"GetAttr Result = "<< res << endl;

    response->set_dev(stbuf.st_dev);
    response->set_ino(stbuf.st_ino);
    response->set_mode(stbuf.st_mode);
    response->set_nlink(stbuf.st_nlink);
    response->set_uid(stbuf.st_uid);
    response->set_gid(stbuf.st_gid);
    response->set_rdev(stbuf.st_rdev);
    response->set_size(stbuf.st_size);
    response->set_atime(stbuf.st_atime);
    response->set_mtime(stbuf.st_mtime);
    response->set_ctime(stbuf.st_ctime);
    response->set_blksize(stbuf.st_blksize);
    response->set_blocks(stbuf.st_blocks);
    response->set_res(res);

    return Status::OK;
  }

  Status MkDir(ServerContext *context, const MkDirRequest *request,
               MkDirResponse *response) override {
    string path = serverpath + request->path();
    int res = mkdir(path.c_str(), request->mode());
    response->set_res(res);
    cout << "MKDIR: " << path << endl;
    return Status::OK;
  }

  Status RmDir(ServerContext *context, const RmDirRequest *request,
               RmDirResponse *response) override {
    string path = serverpath + request->path();
    int res = rmdir(path.c_str());
    response->set_res(res);
    cout << "RMDIR: " << path << endl;
    return Status::OK;
  }

  Status MkNod(ServerContext *context, const MkNodRequest *request,
               MkNodResponse *response) override {
    string path = serverpath + request->path();
    int res;
    mode_t mode = request->mode();
    dev_t rdev = request->rdev();

    if (S_ISREG(mode)) {
      res = open(path.c_str(), O_CREAT | O_EXCL | O_WRONLY, mode);
      if (res >= 0) res = close(res);
    } else if (S_ISFIFO(mode)) {
      res = mkfifo(path.c_str(), mode);
    } else {
      res = mknod(path.c_str(), mode, rdev);
    }

    response->set_res(res);
    cout << "MKNOD: " << path << endl;
    return Status::OK;
  }

  // shouldnt be called anymore
  Status ReadFile(ServerContext *context, const ReadRequest *request,
                  ReadResponse *response) override {
    int fd;
    int res;
    string path = serverpath + request->path();

    fd = open(path.c_str(), O_RDONLY);
    if (fd == -1) return Status::CANCELLED;
 
    size_t size = request->size();
    off_t offset = request->offset();
    char buf[size];

    res = pread(fd, &buf, size, offset);
    if (res == -1) return Status::CANCELLED;
    close(fd);

    string returnBuf(buf);
    response->set_buf(returnBuf);
    response->set_res(res);

    return Status::OK;
  }

  Status GetFile(ServerContext *context, const GetFileRequest *request,
                 GetFileResponse *response) override {
    int fd;
    int res;

    string path = serverpath + request->path();
    fd = open(path.c_str(), O_RDONLY);
    if (fd == -1) return Status::CANCELLED;

    // get the size of the file
    struct stat st;
    stat(path.c_str(), &st);
    size_t size = st.st_size;

    char buf[size];
    // read the entire file
    res = pread(fd, &buf, size, 0); 
    if (res == -1) return Status::CANCELLED;
    close(fd);

    string returnBuf(buf);
    response->set_buf(returnBuf);
    response->set_res(res);
    response->set_size(size);

    return Status::OK;
  }

  // shouldnt be called anymore
  Status OpenFile(ServerContext *context, const OpenRequest *request,
                  OpenResponse *response) override {
    int res;
    string path = serverpath + request->path();
    res = open(path.c_str(), request->flags());
    response->set_res(res);
    close(res);
    return Status::OK;
  }

  // shouldnt be called anymore
  Status WriteFile(ServerContext *context, const WriteRequest *request,
                   WriteResponse *response) override {
    int fd;
    int res;
    string path = serverpath + request->path();
    fd = open(path.c_str(), O_WRONLY);
    if (fd == -1) return Status::CANCELLED;
 
    string buf = request->buf();
    size_t size = request->size();
    off_t offset = request->offset();
    res = pwrite(fd, buf.c_str(), size, offset);
    close(fd);
    
    response->set_res(res);
    return Status::OK;
  }

  Status WriteFullFile(ServerContext *context, const WriteFileRequest *request,
                        WriteFileResponse *response) override {
    int fd;
    int res;
    string path = serverpath + request->path();
    cout << "File: " + path + " Writing the WHOLE file back to the server. Data: " << request->buf() << endl;
    fd = open(path.c_str(), O_CREAT | O_RDWR | O_TRUNC);
    if (fd == -1) return Status::CANCELLED;

    string buf = request->buf();
    size_t size = request->size();
    res = pwrite(fd, buf.c_str(), size, 0);
    close(fd);

    response->set_res(res);
    return Status::OK;
  }

  // not being called
  Status AccessFile(ServerContext *context, const AccessRequest *request,
                    AccessResponse *response) override {
    int res;
    string path = serverpath + request->path();
    int mask = request->mask();
    res = access(path.c_str(), mask);
    response->set_res(res);
    return Status::OK;
  }

  Status UTime(ServerContext *context, const UTimeRequest *request,
               UTimeResponse *response) override {
    string path = serverpath + request->path();
    int res = utime(path.c_str(), NULL);
    response->set_res(res);
    cout << "UTIME: " << path << endl;
    return Status::OK;
  }

  Status Unlink(ServerContext *context, const UnlinkRequest *request,
		UnlinkResponse *response) override {
    string path = serverpath + request->path();
    int res = unlink(path.c_str());
    response->set_res(res);
    cout << "UNLINK: " << path << endl;
    return Status::OK;
  }

  Status RenameFile(ServerContext *context, const RenameRequest *request,
                    RenameResponse *response) override {
    string toString = serverpath + request->toname();
    string fromString = serverpath + request->fromname();
    int res = rename(fromString.c_str(), toString.c_str());
    response->set_res(res);
    cout << "RENAME: " << fromString << endl;
    return Status::OK;
  }

 public:
  AfsWorldServiceImpl(string path) {
    serverpath = path; 
  }

 private:
  string serverpath;
  
};

int main(int argc, char** argv) {
  // Instantiate the client. It requires a channel, out of which the actual RPCs
  // are created. This channel models a connection to an endpoint (in this case,
  // localhost at port 50051). We indicate that the channel isn't authenticated
  // (use of InsecureCredentials()).

  if (argc < 2 || argc > 2) {
    cout << "Usage: ./afs_server <directory>\n";
    exit(0);
  }

  string dir(argv[1]);
  cout << "Directory Name: " << dir << endl;
  cout << "Server listening ..." << endl;

  string server_address("0.0.0.0:50051");
  // This is where the files live on the server.
  AfsWorldServiceImpl service(dir);

  ServerBuilder builder;

  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());

  builder.RegisterService(&service);

  unique_ptr<Server> server(builder.BuildAndStart());
  cout << "Server listening on " << server_address << endl;
  server->Wait();

  return 0;
}  
