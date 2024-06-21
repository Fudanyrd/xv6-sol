#include "user/user.h"

int main(int argc, char **argv) {
  printf("%l\n", fremem());

  [[maybe_unused]]char buf[512];
  printf("%l\n", fremem());

  exit(0);
}