#include "user.h"

int 
main(int argc, char **argv) {
  
  if (argc == 1) {
    printf("<usage> sleep <seconds>\n");
    exit(0);
  }

  const int sec = atoi(argv[1]);
  sleep(sec);

  exit(0);
}