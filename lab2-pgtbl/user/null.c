#include "user/user.h"

/** try dereferencing a nullptr. */
int main(int argc, char **argv) {
  printf("pid = %d\n", ugetpid());

  int *nptr = 0;
  printf("deref = %d\n", *nptr);

  exit(0);
}