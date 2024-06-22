#define FUSE_USE_VERSION 30

#include <asm-generic/errno-base.h>
#include <errno.h>
#include <fuse.h>
#include <glib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "file.h"

GHashTable *dir_list;
GHashTable *files;

void add_dir(const char *dir_name) {
  char *new_dir_name = strdup(dir_name);
  g_hash_table_add(dir_list, new_dir_name);
}

int is_dir(const char *path) {
  if (g_hash_table_contains(dir_list, path)) {
    return true;
  }
  return false;
}

void add_file(const char *filename) {
  struct file_handle *new_handle = file_handle_init(filename);
  g_hash_table_insert(files, new_handle->path_to_file, new_handle);
}

int is_file(const char *path) {
  if (g_hash_table_contains(files, path)) {
    return true;
  }
  return false;
}

void write_to_file(const char *path, const char *new_content, size_t size,
                   off_t offset) {
  struct file_handle *handle = g_hash_table_lookup(files, path);
  if (handle == NULL) {
    // printf(stderr, "Null Handle, creating files...\n");
    return;
  }
  file_handle_add_content(handle, new_content, size, offset);
}

int unlink_file(const char *path) {
  if (g_hash_table_remove(dir_list, path)) {
    return 0;
  }
  if (g_hash_table_remove(files, path)) {
    return 0;
  }
  return -ENOENT;
}

static int do_getattr(const char *path, struct stat *st) {
  // printf("GetAttr: %s\n", path);
  st->st_uid = getuid(); // The owner of the file/directory is the user who
                         // mounted the filesystem
  st->st_gid = getgid(); // The group of the file/directory is the same as the
                         // group of the user who mounted the filesystem
  st->st_atime =
      time(NULL); // The last "a"ccess of the file/directory is right now
  st->st_mtime =
      time(NULL); // The last "m"odification of the file/directory is right now

  if (strcmp(path, "/") == 0 || is_dir(path + 1) == 1) {
    st->st_mode = S_IFDIR | 0755;
    st->st_nlink = 2; // Why "two" hardlinks instead of "one"? The answer is
                      // here: http://unix.stackexchange.com/a/101536
    return 0;
  } else if (is_file(path + 1) == 1) {
    st->st_mode = S_IFREG | 0644;
    st->st_nlink = 1;
    st->st_size = 1024;
    return 0;
  }
  return -ENOENT;
}

static int do_open(const char *path, struct fuse_file_info *fi) {
  // printf("Opening: %s\n", path);
  return 0;
}

struct files_data {
  void *buffer;
  fuse_fill_dir_t filler;
};

void files_foreach(gpointer key, gpointer value, gpointer userdata) {
  char *path_to_file = (char *)key;
  struct files_data *data = (struct files_data *)userdata;

  data->filler(data->buffer, path_to_file, NULL, 0);
}

static int do_readdir(const char *path, void *buffer, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi) {
  filler(buffer, ".", NULL, 0);  // Current Directory
  filler(buffer, "..", NULL, 0); // Parent Directory

  if (strcmp(path, "/") != 0) {
    return 0;
  }

  // If the user is trying to show the files/directories of the root
  // directory show the following
  struct files_data *data = (struct files_data *)malloc(sizeof(*data));
  *data = (struct files_data){
      .buffer = buffer,
      .filler = filler,
  };

  g_hash_table_foreach(dir_list, files_foreach, data);
  g_hash_table_foreach(files, files_foreach, data);

  free(data);

  return 0;
}

static int do_read(const char *path, char *buffer, size_t size, off_t offset,
                   struct fuse_file_info *fi) {
  // printf("Reading: %s, size: %zu, offset: %ld\n", path, size, offset);
  struct file_handle *handle = g_hash_table_lookup(files, path + 1);
  if (handle == NULL) {
    // printf(stderr, "File not found");
    return -ENOENT;
  }

  size_t read_size = read_offset_content(buffer, handle, size, offset);
  // printf("Read size: %lu\n", read_size);
  return read_size;
}

static int do_mkdir(const char *path, mode_t mode) {
  // printf("Make dir: %s\n", path);
  add_dir(path + 1);
  return 0;
}

static int do_mknod(const char *path, mode_t mode, dev_t rdev) {
  // fprintf("Creating file: %s\n", path);
  add_file(path + 1);
  return 0;
}

static int do_write(const char *path, const char *buffer, size_t size,
                    off_t offset, struct fuse_file_info *info) {
  // printf("Writing File: %s, Size: %zu bytes, offset: %ld bytes\n", path, size, offset);
  write_to_file(path + 1, buffer, size, offset);
  return size;
}

static int do_unlink(const char *path) {
  // printf("Deleting File: %s\n", path);
  return unlink_file(path + 1);
}

static struct fuse_operations operations = {
    .getattr = do_getattr,
    .readdir = do_readdir,
    .open = do_open,
    .read = do_read,
    .mkdir = do_mkdir,
    .mknod = do_mknod,
    .write = do_write,
    .unlink = do_unlink,
};

int main(int argc, char *argv[]) {
  dir_list = g_hash_table_new_full(g_str_hash, g_str_equal, free, free);
  files =
      g_hash_table_new_full(g_str_hash, g_str_equal, NULL, file_handle_free);

  return fuse_main(argc, argv, &operations, NULL);
}