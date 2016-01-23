#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <dirent.h>

#include <iostream>
#include <memory>
#include <time.h>
#include <stdint.h>
#include <stdlib.h>
#include <algorithm>
#include <signal.h>

#include <fstream>

#include <grpc++/grpc++.h>
#include "afsworld.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using grpc::ClientReader;
using grpc::ClientWriter;

using afsworld::AfsWorldService;
using afsworld::StringMessage;
using afsworld::ReadDirMessage;
using afsworld::ReadDirResponse;
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

// Defines a stub which you call into to call server functions
class AfsClient {
public:
  AfsClient(shared_ptr<Channel> channel)
    : stub_(AfsWorldService::NewStub(channel)) {}

  string SendText(const string &message) {
    StringMessage request;
    request.set_stringmessage(message);

    StringMessage reply;
    ClientContext context;
    Status status = stub_->SendText(&context, request, &reply);

    if (status.ok()) {
      return reply.stringmessage();
    } else {
      return "RPC returned FAILURE";
    }  
  }

  ReadDirResponse ReadDir(const string &path) {
    ReadDirMessage request;
    request.set_path(path);
      
    ReadDirResponse reply;
    ClientContext context;
    Status status = stub_->ReadDir(&context, request, &reply);
    if (status.ok()) {
      return reply;
    }
    //return null;
    // TODO: Do something on failure here
    return reply;
  }

  GetAttrResponse GetAttr(const string &path) {
    GetAttrRequest request;
    request.set_path(path);

    GetAttrResponse response;
    ClientContext context;
    Status status = stub_->GetAttr(&context, request, &response);
    if (status.ok()) {
      return response;
    }
    // TODO: Do something on failure here
    return response;
  }

  MkDirResponse MkDir(const string &path, mode_t mode) {
    MkDirRequest request;
    request.set_path(path);
    request.set_mode(mode);

    MkDirResponse response;
    ClientContext context;
    Status status = stub_->MkDir(&context, request, &response);
    if (status.ok()) {
      return response;
    }
    // TODO: Do something on failure here
    return response;
  }

  RmDirResponse RmDir(const string &path) {
    RmDirRequest request;
    request.set_path(path);

    RmDirResponse response;
    ClientContext context;
    Status status = stub_->RmDir(&context, request, &response);
    if (status.ok()) {
      return response;
    }
    // TODO: Do something on failure here
    return response;
  }

  MkNodResponse MkNod(const string &path, mode_t mode, dev_t rdev) {
    MkNodRequest request;
    request.set_path(path);
    request.set_mode(mode);
    request.set_rdev(rdev);

    MkNodResponse response;
    ClientContext context;
    Status status = stub_->MkNod(&context, request, &response);
    if (status.ok()) {
      return response;
    }
    // TODO: Do something on failure here
    return response;
  }

  ReadResponse ReadFile(const string &path, size_t size, off_t offset) {
    ReadRequest request;
    request.set_path(path);
    request.set_size(size);
    request.set_offset(offset);

    ReadResponse response;
    ClientContext context;
    Status status = stub_->ReadFile(&context, request, &response);
    if (status.ok()) {
      return response;
    }
    // TODO: Do someting on failure here
    return response;
  }

  GetFileResponse GetFile(const string &path) {
    GetFileRequest request;
    request.set_path(path);

    GetFileResponse response;
    ClientContext context;
    Status status = stub_->GetFile(&context, request, &response);
    if (status.ok()) {
      return response;
    }
    // TODO: Do Something on failure here
    return response;
  }

  WriteFileResponse WriteFullFile(const string &path, string &buf, size_t size) {
    WriteFileRequest request;
    request.set_path(path);
    request.set_buf(buf);
    request.set_size(size);
 
    WriteFileResponse response;
    ClientContext context;
    Status status = stub_->WriteFullFile(&context, request, &response);
    if (status.ok()) {
      return response;
    }
    // TODO: Do something on failure here
    return response;
  }

  OpenResponse OpenFile(const string &path, int flags) {
    OpenRequest request;
    request.set_path(path);
    request.set_flags(flags);

    OpenResponse response;
    ClientContext context;
    Status status = stub_->OpenFile(&context, request, &response);
    if (status.ok()) {
      return response;
    }
    // TODO: Do something on failure here
    return response;
  }

  WriteResponse WriteFile(const string &path, string &buf, size_t size, off_t offset) {
    WriteRequest request;
    request.set_path(path);
    request.set_buf(buf);
    request.set_size(size);
    request.set_offset(offset);

    WriteResponse response;
    ClientContext context;
    Status status = stub_->WriteFile(&context, request, &response);
    if (status.ok()) {
      return response;
    }
    // TODO: Do something on failure here
    return response;
  }

  AccessResponse AccessFile(const string &path, int mask) {
    AccessRequest request;
    request.set_path(path);
    request.set_mask(mask);

    AccessResponse response;
    ClientContext context;
    Status status = stub_->AccessFile(&context, request, &response);
    if (status.ok()) {
      return response;
    }
    // TODO: Do something on failure here
    return response;
  }

  UTimeResponse UTime(const string &path) {
    UTimeRequest request;
    request.set_path(path);

    UTimeResponse response;
    ClientContext context;
    Status status = stub_->UTime(&context, request, &response);
    if (status.ok()) {
      return response;
    }
    // TODO: Do something on failure here
    return response;
  }

  UnlinkResponse Unlink(const string &path) {
    UnlinkRequest request;
    request.set_path(path);

    UnlinkResponse response;
    ClientContext context;
    Status status = stub_->Unlink(&context, request, &response);
    if (status.ok()) {
      return response;
    }
    // TODO: Do something on failure here
    return response;
  }

  RenameResponse Rename(const string &from, const string &to) {
    RenameRequest request;
    request.set_toname(to);
    request.set_fromname(from);

    RenameResponse response;
    ClientContext context;
    Status status = stub_->RenameFile(&context, request, &response);
    if (status.ok()) {
      return response;
    }
    // TODO: Do something on failure here
    return response;
  } 

private:
  unique_ptr<AfsWorldService::Stub> stub_;

};
