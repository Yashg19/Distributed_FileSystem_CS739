/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  Copyright (C) 2011       Sebastian Pipping <sebastian@pipping.org>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall fusexmp.c `pkg-config fuse --cflags --libs` -o fusexmp
*/

#define FUSE_USE_VERSION 26

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "client.hh"
#include "wrap.hh"
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

using namespace std;

int fail_flag = 0;

//static AfsClient client(grpc::CreateChannel("localhost:50051", grpc::InsecureCredentials()));

static AfsClient client(grpc::CreateChannel("128.110.152.95:50051", grpc::InsecureCredentials()));

static string tmpDir;
static string clientCacheDirectory;

static int open_file(string &clientPath, int flags) {
        int fd;
	fd = open(clientPath.c_str(), flags, 0666); 
	return fd; 
}

static int write_newfile(int fd, string &buf, size_t size) {
        int res = pwrite(fd, buf.c_str(), size, 0);
	return res;
}

static int file_copy(string sourcePath, string destPath) {
  
        size_t bufSize = 1;
	size_t fileSize;    
	char buf[bufSize];
	
	FILE* source = fopen(sourcePath.c_str(), "rb");
	FILE* dest = fopen(destPath.c_str(), "wb");
	
	while ((fileSize = fread(buf, bufSize, 1, source))) {
	  client.SendText("Copying data to the temp folder");
	  fwrite(buf, bufSize, fileSize, dest);
	}
	
	fclose(source);
	fclose(dest);

	return 0;
}


int xmp_getattr(const char *path, struct stat *stbuf)
{
	int res;
	string stringpath(path);
	GetAttrResponse response = client.GetAttr(path);

	stbuf->st_dev = response.dev();
	stbuf->st_ino = response.ino();
	stbuf->st_mode = response.mode();
	stbuf->st_nlink = response.nlink();
	stbuf->st_uid = response.uid();
	stbuf->st_gid = response.gid();
	stbuf->st_rdev = response.rdev();
	stbuf->st_size = response.size();
	stbuf->st_atime = response.atime();
	stbuf->st_mtime = response.mtime();
	stbuf->st_ctime = response.ctime();
	stbuf->st_blksize = response.blksize();
	stbuf->st_blocks = response.blocks();

	res = response.res();
	if (res == -1) 
	  return -ENOENT;

	return 0;
}

int xmp_access(const char *path, int mask)
{
	int res;

	string stringpath(path);
	AccessResponse response = client.AccessFile(stringpath, mask);
	res = response.res();
	if (res == -1) return -errno;

	return 0;
}

int xmp_utime(const char *path, utimbuf *time) {
       
        string stringpath(path);
	UTimeResponse response = client.UTime(stringpath);
	
	int res = response.res();
	if (res == -1) return -errno;
	
	// also utime on the client
	string clientPath = clientPath + stringpath;
	res = utime(clientPath.c_str(), NULL);
	if (res == -1) return -errno;
	
	return 0;
}

int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{
        string stringPath(path);
	ReadDirResponse reply = client.ReadDir(stringPath);
	(void) offset;
	(void) fi;
	
	
	for (int i = 0; i < reply.inodenumber().size(); i++) {
	  struct stat st;
	  memset(&st, 0, sizeof(st));
	  st.st_ino = reply.inodenumber().Get(i);
	  st.st_mode = reply.type().Get(i) << 12;
	  string name = reply.name().Get(i);
	  if (filler(buf, name.c_str(), &st, 0)) break;
	}
	
	return 0;
}

int xmp_mknod(const char *path, mode_t mode, dev_t rdev)
{
	int res;

	/* On Linux this could just be 'mknod(path, mode, rdev)' but this
	   is more portable */
	string stringpath(path);
	MkNodResponse response = client.MkNod(stringpath, mode, rdev);

	res = response.res();
	if (res == -1) return -errno;


	return 0;
}

int xmp_mkdir(const char *path, mode_t mode)
{
	int res;

	string stringpath(path);
	MkDirResponse response = client.MkDir(stringpath, mode);

	res = response.res();
	if (res == -1) 
	         return -errno;

	//Create directory in local cache if server works fine

	string cacheMkdirPath = clientCacheDirectory + path;

	client.SendText("Local mkdir creation: " + cacheMkdirPath);

	res = mkdir(cacheMkdirPath.c_str(), mode);
	if (res == -1) 
	  {
	    client.SendText("CACHE MKDIR failed! ErrNo =  " + errno);
	    return -errno;
	  }

	string tmpPath = tmpDir + string(path);
	
	client.SendText("Temp mkdir creation: " + tmpPath);

	res = mkdir(tmpPath.c_str(), mode);
	if (res == -1) 
	  {
	    client.SendText("Temp folder MKDIR failed! ErrNo =  " + errno);
	    return -errno;
	  }

	return 0;
}

int xmp_unlink(const char *path)
{
	int res;

	string stringpath(path);
	UnlinkResponse response = client.Unlink(stringpath);
  
	res = response.res();
	if (res == -1) return -errno;

	// also unlink it on the client
	string clientPath = clientCacheDirectory + stringpath;
	res = unlink(clientPath.c_str());
	if (res == -1) return -errno;

	return 0;
}

int xmp_rmdir(const char *path)
{
	int res;

	string stringpath(path);
	RmDirResponse response = client.RmDir(stringpath);
	res = response.res();
	if (res == -1) 
	  {
	    client.SendText("SERVER RMDIR failed! ErrNo =  " + errno);
	    return -errno;
	  }

	// If server rmdir works correctly, also need to remove this directory in the client cache
	string cacheRmdirPath = clientCacheDirectory + path;
	client.SendText("CACHE RMDIR: " + cacheRmdirPath);
	res = rmdir(cacheRmdirPath.c_str());
	if (res == -1) 
	  {
	    client.SendText("CACHE RMDIR failed! ErrNo =  " + errno);
	    return -errno;
	  }

	string tmpPath = tmpDir + string(path);
	client.SendText("Temp folder RMDIR: " + tmpPath);
	res = rmdir(tmpPath.c_str());
	if (res == -1) 
	  {
	    client.SendText("Temp Folder RMDIR failed! ErrNo =  " + errno);
	    return -errno;
	  }
	

	return 0;
}

int xmp_rename(const char *from, const char *to)
{
	int res;
	string frompath(from);
	string topath(to);
	RenameResponse response = client.Rename(frompath, topath);
	res = response.res();
	if (res == -1) return -errno;

	return 0;
}

int xmp_truncate(const char *path, off_t size)
{
	int res;

	string clientPath = clientCacheDirectory + string(path);
	res = truncate(clientPath.c_str(), size);
	if (res == -1) return -errno;

	return 0;
}

int xmp_open(const char *path, struct fuse_file_info *fi)
{
        int res;
	string stringpath = string(path);
	string tmpPath = tmpDir + stringpath;
	string clientPath = clientCacheDirectory + string(path);

	//Delete temp file if any in the temp folder path
	ifstream tmpfile(tmpPath);
	client.SendText("xmp_open: temp Path = " + tmpPath);

	if (tmpfile.good()) {
	       client.SendText("xmp_open: Erasing file on temp Path = " + tmpPath);
	       res = remove(tmpPath.c_str());
	       if (res == -1) 
		        return -errno;
	}

	//Check - if file is cached on local
	client.SendText("xmp_open: infile cache path = " + clientPath);
	ifstream infile(clientPath);
	
	if (infile.good()) {
	       struct stat statbuf;
	       if (stat(clientPath.c_str(), &statbuf) == -1) 
		        return -errno;
	       
	       GetAttrResponse attrResponse = client.GetAttr(stringpath);
	       
	       if (attrResponse.res() == -1) 
		        return -errno;
	       
	       //Fetch new copy from server if it has new else use local/cached copy
	       if (attrResponse.mtime() > statbuf.st_mtime) {
		 
		        client.SendText("File: " + string(path) + " Fetching new copy - updated");
			GetFileResponse response = client.GetFile(string(path));
			
			int res = response.res();
			if (res == -1) 
			        return -errno;
			
			int fd = open_file(clientPath, O_CREAT | O_RDWR | O_TRUNC);
			if (fd == -1) 
			        return -errno;
			
			string buf = response.buf();
			size_t size = response.size();
			int clientRes = write_newfile(fd, buf, size);
			
			if (clientRes == -1) 
			        return -errno;

			close(fd);
			return 0;
	       } else {
		        int fd = open_file(clientPath, O_RDWR);
			if (fd == -1) 
			        return -errno;

			close(fd);

			client.SendText("File: " + string(path) + " Using cached copy! PATH: " + clientPath);
			return 0;
	       }
	} else {
	         GetFileResponse response = client.GetFile(string(path));
		 int res = response.res();
		 if (res == -1) 
		          return -errno; 

		 //Writing to local file
		 int fd = open_file(clientPath, O_CREAT | O_RDWR | O_TRUNC);
		 if (fd == -1) 
		         return -errno;

		 string buf = response.buf();
		 size_t size = response.size();
		 int clientRes = write_newfile(fd, buf, size);

		 if (clientRes == -1) 
		         clientRes = -errno;

		 close(fd);
		 client.SendText("File: " + string(path) + " Fetching File from Server first time!!");
		 return 0;
	}
	
	return -1;
}

int xmp_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{
	int fd;
	int res;

	(void) fi;

	string clientPath = clientCacheDirectory + string(path);
	
	fd = open(clientPath.c_str(), O_RDONLY);
	if (fd == -1) 
	        return -errno;

	res = pread(fd, buf, size, offset);
	if (res == -1) 
	         res = -errno;

	close(fd);
	return res;
}

int xmp_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
	int res;
	int tmp_fd;

	string tmpPath = tmpDir + string(path);
	string clientCachePath = clientCacheDirectory + string(path);

	(void) fi;


	client.SendText("xmp_write File Copy - Source = " + clientCachePath + " tmpPath =  " + tmpPath);
	//Copying cached file to tmp folder first
	file_copy(clientCachePath, tmpPath);
  
	// Write to temporary file 

	tmp_fd = open(tmpPath.c_str(), O_CREAT | O_WRONLY, 0666);  
	
	if (tmp_fd == -1) 
	           return -errno;

	res = pwrite(tmp_fd, buf, size, offset);
	fsync(tmp_fd);

	if (res == -1) 
	         res = -errno;

	close(tmp_fd);
  
	//Failure 1 on client
	if (fail_flag == 1) {
	  kill(getpid(), SIGKILL);
	}

	return res;
}

int xmp_release(const char *path, struct fuse_file_info *fi)
{
	/* Just a stub.	 This method is optional and can safely be left
	   unimplemented */

        int res;
        string stringpath(path);

	string tmpPath = tmpDir + stringpath;
	string cacheFilePath = clientCacheDirectory + stringpath;


	// Local File Stat:
	struct stat cacheFileStat;

	client.SendText("xmp_release tmpPath : " + tmpPath);
	if (stat(tmpPath.c_str(), &cacheFileStat) == -1) 
	  {
	    	client.SendText("xmp_release error 1 : " + tmpPath);
	        return -errno;
	  }

	size_t cacheFileSize = cacheFileStat.st_size;
	
	// Server File Stat:
	GetAttrResponse serverFileAttrResponse = client.GetAttr(stringpath);
	res = serverFileAttrResponse.res();
	if (res == -1) 
	  {
	    client.SendText("xmp_release Error: not able to read it from server");
	    return -errno;
	  }

	if (cacheFileStat.st_mtime >= serverFileAttrResponse.mtime()) {
	       int fd = open_file(tmpPath, O_RDONLY);
	       if (fd == -1) 
		       return -errno;
  
	       char buf[cacheFileSize];
	       res = pread(fd, &buf, cacheFileSize, 0); 
	       if (res == -1) 
		        return -errno;
	       close(fd);

	       string cacheFileData = string(buf);

	       client.SendText("xmp_release File: " + stringpath + " Pushing File to server Data: " + cacheFileData + " Size: " + to_string(cacheFileSize));

	       WriteFileResponse response = client.WriteFullFile(stringpath, cacheFileData, cacheFileSize);
	       res = response.res();
	       client.SendText("Response: " + to_string(res));

	       if (res == -1) 
		        return -errno;

	       client.SendText("Local Renaming: " + cacheFilePath);
	       
	       rename(tmpPath.c_str(), cacheFilePath.c_str());
	} else {
	       client.SendText("xmp_release File: " + stringpath + " Do Nothing if timestamp not changed even on fsync call.");
	}
	
	return 0;
}

struct fuse_operations xmp_oper;

int main(int argc, char *argv[])
{
        int fuse_stat;
        umask(0);
	xmp_oper.getattr	= xmp_getattr,
	xmp_oper.access		= xmp_access,
	xmp_oper.readlink	= NULL,
	xmp_oper.readdir	= xmp_readdir,
	xmp_oper.mknod		= xmp_mknod,
	xmp_oper.mkdir		= xmp_mkdir,
	xmp_oper.symlink	= NULL,
	xmp_oper.unlink		= xmp_unlink,
	xmp_oper.rmdir		= xmp_rmdir,
	xmp_oper.rename		= xmp_rename,
	xmp_oper.link		= NULL,
	xmp_oper.chmod		= NULL,
	xmp_oper.chown		= NULL,
	xmp_oper.truncate	= xmp_truncate,
	xmp_oper.utime          = xmp_utime,
	xmp_oper.open		= xmp_open,
	xmp_oper.read		= xmp_read,
	xmp_oper.write		= xmp_write,
	xmp_oper.statfs		= NULL,
	xmp_oper.release	= xmp_release,
	xmp_oper.fsync		= NULL,
	xmp_oper.setxattr	= NULL,
	xmp_oper.getxattr	= NULL,
	xmp_oper.listxattr	= NULL,
	xmp_oper.removexattr	= NULL;


	if (argc < 3 || argc > 5) {
	  cout << "Invalid number of arguments. Quitting..." << endl;
	  cout << "Third argument must be an absolute path." << endl;
	  exit(0);
	}

	int numArgs = 1;
	if (argc == 5) {
	  numArgs = 2; 
	  string fail = string(argv[argc - 1]);
	  if (fail.compare("fail1") == 0) {
	    fail_flag = 1;
	  }
	}
	char *args[argc - numArgs];
	for (int i = 0; i < argc - numArgs; i++) {
	  int length = strlen(argv[i]);
	  args[i] = new char[length + 1];
	  strncpy(args[i], argv[i], length);
	  args[i][length] = '\0';
	}

	tmpDir = "/users/ygovind/fuse-2.9.4/example/cs739_p2/tmp2/";
	clientCacheDirectory = argv[argc - numArgs];

	cout << "clientCacheDir = " << clientCacheDirectory << endl;
	//clientTmpDirectory = "/tmp";
	fuse_stat = fuse_main(argc - numArgs, args, &xmp_oper, NULL);

        printf("fuse_main returned %d\n", fuse_stat);

	return fuse_stat;
}
