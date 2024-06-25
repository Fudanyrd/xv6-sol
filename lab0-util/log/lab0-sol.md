# Unix Utility Lab
[Handout](https://pdos.csail.mit.edu/6.828/2021/labs/util.html)

## Boot xv6
Simply follow instructions [here](https://pdos.csail.mit.edu/6.828/2021/tools.html), i.e.  
```sh
git clone git://g.csail.mit.edu/xv6-labs-2021 && \
cd xv6-labs-2021 && \
git checkout util && \
make qemu
```

## sleep
Don't forget to add sleep implementation in `Makefile`.  Only information needed to 
complete this task is the two functions in `user.h`:
```c
int sleep(int);
int atoi(const char*);
```

Results: perfect.
```
root@yrd:/mnt/d/handouts/xv6/code# ./grade-lab-util sleep
make: 'kernel/kernel' is up to date.
== Test sleep, no arguments == sleep, no arguments: OK (0.8s) 
    (Old xv6.out.sleep_no_args failure log removed)
== Test sleep, returns == sleep, returns: OK (0.8s) 
== Test sleep, makes syscall == sleep, makes syscall: OK (1.0s) 
    (Old xv6.out.sleep failure log removed)
```

## Pingpong
System calls needed:
```c
// Create a pipe, put read/write file descriptors in p[0] and p[1]
int pipe(int p[]);
// Write n bytes from buf to file descriptor fd; returns n.
int write(int fd, char *buf, int n);
// Read n bytes into buf; returns number read; or 0 if end of file
int read(int fd, char *buf, int n);
```
Then I encountered concurrency error, the output looks like
```
4: r3ec:e rievced epinigv
ed pong
```
Solution: will need to receive the byte before calling `printf`, i.e.
```c
else {
  // is child, write byte to the pipe.
  read(pip.read_, &byte, 1);
  printf("%d: received ping\n", getpid());
  write(pip.write_, &byte, 1);
}
```

is the correct order of the child process.

Results:
```
== Test pingpong == pingpong: OK (1.1s)
    (Old xv6.out.pingpong failure log removed)
```

A kind <a style="color:red">warning</a>: always use `close` to close file descriptors.

## Primes
Surprisingly, this can pass the test:
```c
  for (int i = start; i < 35; ++i) {
    if (is_prime(i)) {  // judge prime number
      print_prime(i);   // print prime number
    }
  }
```
But I'd like to try something fancier. First attempt(looks correct, but not):
```c
struct pipe_port { int read_, write_; };
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
    // parent process, write current number to the pipe.
    write(pip.write_, (void *)(&start), sizeof(int));
  }

  exit(0);
}
```
This is incorrect because parent will exit **before** its child process. Need some way to make parent 
live long enough. The solution is obvious:
```c
struct pipe_port { int read_, write_; };
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
```
Another solution that came to my mind is when the child is done, ping-pong a byte through the pipe. But in this case,
we cannot close the pipe that early. This will cause running time to take a hit.

## Find
First following the hints, read `ls.c` to see how to read a directory:
```c
/** 
 * @param fd file descriptor
 * @return <0 if cannot stat the file
 */
int fstat(int fd, struct stat*);
```

Also from chapter 1 of `xv6 book`, the struct of a file is:
```c
#define T_DIR  1  // directory
#define T_FILE 2  // file

struct stat {
  int dev;        // disk device
  uint ino;       // inode number
  short type;
  short nlink;    // # links to file
  uint64_t size;  // size of file in bytes
};
```

To get the file name, we need to read `dirent`:
```c
struct dirent {
  ushort inum;
  char name[14];
};
```

`dirent` resides in the file descriptor(really?? just pretend for now), hence we can do the following:
```c
  // in ls.c, line 57
    while(read(fd, &de, sizeof(de)) == sizeof(de)) {
      if (de.inum == 0) { continue; }
      ...
    }
```

Results:
```
== Test find, in current directory == find, in current directory: OK (0.9s) 
    (Old xv6.out.find_curdir failure log removed)
== Test find, recursive == find, recursive: OK (1.1s) 
    (Old xv6.out.find_recursive failure log removed)
```

## Xargs
Typical fork-exec combination. The only pitfall here is how to set `argv`(recall that `argv[0]` should be the executable 
name!).

Results:
```
== Test xargs == xargs: OK (1.3s) 
    (Old xv6.out.xargs failure log removed)
```

## Run all tests
```
make: 'kernel/kernel' is up to date.
== Test sleep, no arguments == sleep, no arguments: OK (0.7s) 
== Test sleep, returns == sleep, returns: OK (0.8s) 
== Test sleep, makes syscall == sleep, makes syscall: OK (1.0s) 
== Test pingpong == pingpong: OK (1.1s) 
== Test primes == primes: OK (1.0s)
== Test find, in current directory == find, in current directory: OK (1.2s)
== Test find, recursive == find, recursive: OK (1.0s)
== Test xargs == xargs: OK (1.0s)
== Test time ==
time: OK
Score: 100/100
```
That's it.
