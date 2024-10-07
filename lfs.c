/*______________________________________________
|     __      __    __   _     __   __ __    __ |
|    /@/     /@/   /@/  /@\   /@/  /@/ \@\  /@/ |
|   /x/     /X/   /X/  /X.X\ /X/  /X/   \X\/X/  |
|  /~/___  /~/___/~/  /~/ \~`~/  /~/    /~/\~\  |
| /_____/  \______/  /_/   \_/  /_/    /_/  \_\ |
|_______________________________________________|
|___F___I___L___E______S___Y___S___T___E___M___*/

#define FUSE_USE_VERSION 25
#include<fuse.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<time.h>
#include<errno.h>
#include"lfs.h"

int lfs_getattr(const char*, struct stat*);
int lfs_readdir(const char*, void*, fuse_fill_dir_t, off_t, struct fuse_file_info*);
int lfs_read(const char*, char*, size_t, off_t, struct fuse_file_info*);
int lfs_mkdir(const char*, mode_t);
int lfs_mknod(const char*, mode_t , dev_t);
int lfs_write(const char*, const char*, size_t, off_t, struct fuse_file_info*);
int lfs_unlink(const char*);
int lfs_utime(const char*, struct utimbuf*);
int lfs_rmdir(const char*);
void lfs_destroy(void*);

static FILE *disk;

// Fuse operations
static struct fuse_operations lfs_oper = {
.getattr  = lfs_getattr,
.readdir  = lfs_readdir,
.mknod    = lfs_mknod,
.mkdir    = lfs_mkdir,
.unlink   = lfs_unlink,
.rmdir    = lfs_rmdir,
.read     = lfs_read,
.write    = lfs_write,
.utime    = lfs_utime,
.destroy  = lfs_destroy,
};
// function is called everytime we do something in our filesystme
int lfs_getattr(const char *path, struct stat *st) {
    lfs_entry *entry = lfs_get_entry(path);
    if(strcmp(path, "/") == 0 || (entry && !(entry->isFile))) { // dir
        st->st_mode = S_IFDIR | 0755;
        st->st_nlink = 2;
    }
    else if(entry && entry->isFile) { // entry exists and is file
        st->st_mode = S_IFREG | 0644;
        st->st_nlink = 1;
        st->st_size = strlen(entry->data);
    }
    else
        return -ENOENT;

    // set owner ids and time
    st->st_uid = entry->user_id;
    st->st_gid = entry->user_id;
    st->st_atime = entry->atime;
    st->st_mtime = entry->mtime;

    return 0; // Signals success
}

//function is called when we list a directory through ls 
int lfs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    filler(buffer, ".", NULL, 0); // current dir
    filler(buffer, "..", NULL, 0); // parrent dir

    lfs_entry *current; // dir to read 
    lfs_entry *parent;  // keeps track of current entry being looked at 

    for(int i = 1; i < table->num_entries; i++) {
        current = table->all_entries[i];
        parent = lfs_get_parent_table(current->path);
        if(strcmp(parent->path, path) == 0)
            filler(buffer, current->file_name, NULL, 0);
    }
    return 0; // Signal success
}
// reads the data of a file 
int lfs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
    lfs_entry *entry = lfs_get_entry(path);
    if(!entry)
        return -ENOENT;

    strcpy(buffer, (entry->data)+offset);

    return strlen(buffer) - offset;
}
//writes to a file 
int lfs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *info) {
    lfs_entry *entry = lfs_get_entry(path);
    if(!entry)
        return -ENOENT;

    if(strcmp(path+strlen(path)-3, ".txt") == 0) // if .txt
        strcpy(entry->data+offset, buffer);
    else
        memcpy(entry->data+offset, buffer, size);

    entry->length = strlen(entry->data);

    return size; // Signal success
}
// creates a directory
int lfs_mkdir(const char *path, mode_t mode) {
    lfs_init_entry(path, 0, lfs_generate_unique_id(), NULL);
    return 0;
}
// creates a file
int lfs_mknod(const char *path, mode_t mode, dev_t rdev) {
    lfs_init_entry(path, 1, lfs_generate_unique_id(), "");
    return 0; // Signal success
}
// updates time 
int lfs_utime(const char *path, struct utimbuf *ut) {
    lfs_entry *entry = lfs_get_entry(path);
    if(!entry)
        return -ENOENT;

    entry->atime = ut->actime;
    entry->mtime = ut->modtime;
    return 0; // Signal success
}
// removes a file 
int lfs_unlink(const char *path) {
    lfs_entry *entry = lfs_get_entry(path);
    if(!entry)
        return -ENOENT;
    if(!(entry->isFile)) // is a dir
        return EINVAL;
    else
        return lfs_rm_entry(entry);
}
// removes a directory 
int lfs_rmdir(const char *path) {
    lfs_entry *entry = lfs_get_entry(path);
    if(!entry)
        return -ENOENT;
    if(entry->isFile) // is a file
        return EINVAL;
    else
        return lfs_rm_entry(entry);
}

void lfs_destroy(void *private) {
    lfs_write_data_to_disk(disk);
}

int main(int argc, char *argv[]) {
    // disk to open
    char *filename = "disk.lunix";
    if(access(filename, F_OK) == 0)
        disk = fopen(filename, "r+");
    else
        disk = fopen(filename, "w+");

    lfs_new_table();
    lfs_load_data_from_disk(disk);

    return fuse_main(argc, argv, &lfs_oper);
}
