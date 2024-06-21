#include "user/user.h"

struct pipe_port {
  /** read file descriptor */
  int read_;
  /** write file descriptor */
  int write_;
};
/** get a file descriptor */
void init_pipe(struct pipe_port *pip) {
  pipe((int *)pip);
}
/** close both file descriptors of a pipe */
void close_pipe(struct pipe_port *pip) {
  close(pip->read_);
  close(pip->write_);
}

/** 
 * @return 1 if num is prime number; 0 if not; -1 if non-positive. 
 */
int is_prime(int num) {
  if (num <= 0) {
    return -1;
  }
  
  if (num == 2) { return 1; }
  for (int i = 2; i * i <= num; ++i) {
    if (num % i == 0) {
      return 0;
    }
  }

  return 1;
}

/**
 * print prime number to the screen.
 */
void print_prime(int prime) {
  printf("prime %d\n", prime);
}

int main(int argc, char **argv) {
  const int end = 35;  // end of prime search
  int start = 2;
  struct pipe_port pip;

// start of prime pipeline.
pip_start:  
  // find next prime number.
  while (!is_prime(start)) { ++start; }
  if (start >= end) { 
    exit(0); 
  }
  // print current prime number. 
  print_prime(start++);

  // start pipeline.
  init_pipe(&pip);
  int child = fork();
  if (child == 0) {
    // child process should close the pipe.
    read(pip.read_, (void *)(&start), sizeof(int));
    close_pipe(&pip);
    goto pip_start;
  } else {
    // parent process
    write(pip.write_, (void *)(&start), sizeof(int));
    [[maybe_unused]] int tmp;
    wait(&tmp);
  }

  exit(0);
}