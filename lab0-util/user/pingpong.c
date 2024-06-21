#include "user/user.h"

struct pipe_port {
  /** read file descriptor */
  int read_;
  /** write file descriptor */
  int write_;
};

int main(int argc, char **argv) { 
  /** create a pipe */
  struct pipe_port pip;
  pipe((int *)(&pip));
  
  /** id of parent */
  [[maybe_unused]]const int parent_id = getpid();  
  /** id of child */
  [[maybe_unused]]const int child_id = fork();
  
  /** byte to ping-pong */
  char byte = 'b';

  /** now consider this: */
  if (child_id != 0) {
    // ok, is parent, 
    write(pip.write_, &byte, 1);
    read(pip.read_, &byte, 1);
    printf("%d: received pong\n", getpid());
  } else {
    // is child, write byte to the pipe.
    read(pip.read_, &byte, 1);
    printf("%d: received ping\n", getpid());
    write(pip.write_, &byte, 1);
  }

  close(pip.read_);
  close(pip.write_);
  exit(0);
}
