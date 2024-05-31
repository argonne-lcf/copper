/*
    FUSE: Filesystem in Userspace

    This program can be distributed under the terms of the GNU GPL.
    Copyright (C) 2001-2005  Miklos Szeredi <miklos@szeredi.hu>
    See the file COPYING.
*/

// gcc hello.c `pkg-config fuse3 --cflags --libs`  -o hello

#define FUSE_USE_VERSION 30


#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <margo.h>
#include <stdlib.h>
#include <thallium.hpp>
#include <thallium/serialization/stl/string.hpp>
const char *hello_str = "Hello World!\n";
const char *hello_path = "/hello";

namespace tl = thallium;

tl::engine serverEngine;
tl::remote_procedure get_file_data;

int hello_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
    int res = 0;

    memset(stbuf, 0, sizeof(struct stat));

    if(strcmp(path, "/") == 0) 
    {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    }
    else if(strcmp(path, hello_path) == 0) 
    {
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = strlen(hello_str);
    }
    else
        res = -ENOENT;

    return res;
}

int hello_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags)
{
    (void) offset;
    (void) fi;
	enum fuse_fill_dir_flags fill_flags ;
    fill_flags = FUSE_FILL_DIR_PLUS;


    if(strcmp(path, "/") != 0)
        return -ENOENT;

    filler(buf, ".", NULL, 0, fill_flags);
    filler(buf, "..", NULL, 0, fill_flags);
    filler(buf, hello_path + 1, NULL, 0, fill_flags);

    return 0;
}

int hello_open(const char *path, struct fuse_file_info *fi)
{
    if(strcmp(path, hello_path) != 0)
        return -ENOENT;

    if((fi->flags & 3) != O_RDONLY)
        return -EACCES;

    return 0;
}

int hello_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    size_t len;
    (void) fi;
    
    if(strcmp(path, hello_path) != 0)
        return -ENOENT;

    len = strlen(hello_str);
    
    if (offset < len) 
    {
        if (offset + size > len)
            size = len - offset;
        memcpy(buf, hello_str + offset, size);
    } 
    else
        size = 0;

    {
        tl::engine *serverEngine = static_cast<tl::engine*>(fuse_get_context()->private_data);
        pthread_t tid;
        tid = pthread_self();
        printf("Thread ID: %ld\n", tid);
        printf("Hello from real fuse program with pid  %ld\n", tid);
        std::string client_file_content = get_file_data.on(serverEngine->self())(std::string("/path/to/file"), 4);  
        printf("I'm the main requester from fuse program\n");

        std::cout << client_file_content << std::endl;
    }


    return size;
}

struct fuse_operations hello_oper = {
    .getattr	= hello_getattr,
    .open	    = hello_open,
    .read	    = hello_read,
    .readdir	= hello_readdir,
};


int cu_hello_main(int argc, char *argv[], void* userdata)
{
  printf("hello from orginal fuse program\n");

  for (int i = 0; i < argc; i++) 
  {
    printf("%s\n", argv[i]);
  }

  return fuse_main(argc, argv, &hello_oper, userdata);
}
