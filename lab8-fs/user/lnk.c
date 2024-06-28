#include "user/user.h"

int main(int argc, char **argv) {
  int ret = symlink("README", "README.lnk");

  if (ret == 0) {
    printf("sym succeeded\n");
  } else {
    printf("oops, fail\n");
  }

  int fd = open("README.lnk", 0);
  if (fd < 0) {
    printf("open README.lnk failed\n");
    exit(0);
  }

  char buf[10];
  ret = read(fd, buf, 10);

  printf("read, fd = %d, ret = %d\n", fd, ret);
  for (int i = 0; i < 10; ++i) {
    printf("%c", buf[i]);
  }
  printf("\n");
  for (int i = 0; i < 10; ++i) {
    printf("%d ", (int)buf[i]);
  }
  printf("\n");

  exit(0);
}