#include "kernel/fcntl.h"
#include "kernel/fs.h"
#include "kernel/stat.h"
#include "kernel/types.h"
#include "user/user.h"

/**
 * find the file in the directory.
 * 
 * @param fname file name(exact)
 * @param wd working directory
 */
void find_in_dir(char *fname, char *wd) {
  // memory buffer, will only work if path length is smaller than 512.
  char buf[512];
  strcpy(buf, wd);
  char *it = buf + strlen(buf);
  // file descriptor
  int fd = open(wd, O_RDONLY);
  struct stat st;

  if (fstat(fd, &st) < 0) {
    fprintf(2, "file stat abnormal, abort\n");
    close(fd);
    return;
  } 

  *it = '/'; ++it;
  switch (st.type) {
    case T_DIR: {
      struct dirent de;
      while (read(fd, &de, sizeof(struct dirent)) == sizeof(struct dirent)) {
        if (de.inum == 0) { continue; }
        strcpy(it, de.name);

        if (stat(buf, &st) < 0) {
          printf("cannot stat %s\n", buf);
          continue;
        }
        if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) { continue; }
        if (strcmp(fname, de.name) == 0) {
          printf("%s\n", buf); 
        }
        find_in_dir(fname, buf);
      }

      close(fd);
      break;
    }

    default: {
      close(fd);
      break;
    }
  }
}

int main(int argc, char **argv) {

  if (argc < 3) {
    printf("usage <directory> <file-name>\n");
    exit(0);
  }

  find_in_dir(argv[2], argv[1]);
  exit(0);
}