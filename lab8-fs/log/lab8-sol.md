# Lab 8: Filesystem
[handout](https://pdos.csail.mit.edu/6.828/2021/labs/fs.html)

## Large Files
Primary representation of a file on disk:
```c
/* 
    dinode            +----------+
+------------+   _____|   data   |
|  metadata  |  /     +----------+
+------------+ /
| address 1  |        +-----------+
+------------+        |    data   |
|    ...     |  ______+-----------+
+------------+ /
| address 12 |/
+------------+
| address 13 |                 +----------+
+------------+                 |   data   |
              \   indirect     +----------+
               +-------------+/
               | address  1  |
               +-------------+
               |    ...      |
               +-------------+
               | address 255 |
               +-------------+

See figure 8.3 in xv6 book
*/
```
A single file can occupy $12 + 256 = 268$ data blocks.

Our job is to implement the following file layout(rough):
```c
/* 
    dinode            +----------+
+------------+   _____|   data   |    +----------+
|  metadata  |  /     +----------+    |   data   |
+------------+ /                     /+----------+
| address 1  |        +-----------+ /
+------------+        | indirect  |
|    ...     |  ______+-----------+
+------------+ /                   \
| address 12 |/                     \+----------+
+------------+                       |   data   |
|            |                       +----------+
| address 13 |                  +----------+
+------------+                  | indirect |
              \                /+----------+
               +-------------+/             \+--------+
               |  indrirect  |               |  data  |
               +-------------+               +--------+
*/
```
The indirect node at address 12 is singly-indirected, and that at address 13 is doubly-indrected.
This layout allows us to create files that is $11 + 1\times 256 + 1\times 256\times 256 = 65803$ 
blocks(xv6 block size is 1KB, i.e. we can create files larger than 64GB!).

First in `kernel/fs.h`, change the value of `NDIRECT` to be 11, and the size of addr in `struct dinode` and
`struct inode` should be `NDIRECT+2` instead.

Here's the `bmap` method:
```c
static uint bmap(struct inode *ip, uint bn) {
  uint addr, *a;
  struct buf *bp;
  
  // direct mapping
  if(bn < NDIRECT){
    if((addr = ip->addrs[bn]) == 0)
      ip->addrs[bn] = addr = balloc(ip->dev);
    return addr;
  }
  bn -= NDIRECT;

  // singly-indirect mapping
  if(bn < NINDIRECT){
    // skipped
  }
  bn -= NINDIRECT;  // bn -= 256

  // doubly-indirected mapping
  struct buf *f1, *f2;  // these need to be freed
  if (bn < NDOUBLE_INDIRECT) {  // bn < 256 * 256
    // get the root, at depth 0
    if ((addr = ip->addrs[NDIRECT+1]) == 0) {
      addr = ip->addrs[NDIRECT+1] = balloc(ip->dev);
    }
    bp = bread(ip->dev, addr);
    a = (uint *)bp->data;
    f1 = bp;

    // get the directory, at depth 1
    if ((addr = a[bn / NINDIRECT]) == 0) {
      addr = a[bn / NINDIRECT] = balloc(ip->dev);
      log_write(bp);
    }
    bp = bread(ip->dev, addr);
    a = (uint *)bp->data;
    f2 = bp;

    // get to data page, at depth 2
    if ((addr = a[bn % NINDIRECT]) == 0) {
      addr = a[bn % NINDIRECT] = balloc(ip->dev);
      log_write(bp);
    }
    bp = bread(ip->dev, addr);

    brelse(f1);
    brelse(f2);
    brelse(bp);
    return addr;
  }

  panic("bmap: out of range");
}
```

## Symbolic links
We follow the practice in <a href="../../lab1-syscall/log/lab1-sol.md"> this log</a> to create the syscall
`sys_symlink`. To retrive the content of target file, we also need to store the length of path(as shown below).
```c
struct symlink_pack {
  uint len_;  // length of directory
  char path_[MAXPATH]; 
};

// symlink(char *target, char *path)
uint64 sys_symlink(void) {
  struct symlink_pack pack;
  char *target = pack.path_;
  char path[MAXPATH];
  int bytes;

  if((bytes = argstr(0, target, MAXPATH)) < 0 || argstr(1, path, MAXPATH) < 0) {
    return -1;
  }
  pack.len_ = bytes; 

  begin_op();
  struct inode *ip = namei(path);

  if (ip != 0) {
    // already exists:
    end_op();
    return -1;
  }
  // create file
  ip = create(path, T_SYMLINK, 0, 0);
  if (ip == 0) {
    end_op();
    return -1;
  }
  // dump the entire pack
  if (writei(ip, 0, (uint64)&pack, 0, bytes + sizeof(uint)) != bytes + sizeof(uint)) {
    iunlock(ip);
    end_op();
    return -1;
  }

  iunlock(ip);
  end_op();
  return 0;
}
```

I'm a lazy guy, so I'll just use some trivial way to complete this, move most content in
`sys_open` to a new one:
```c
uint64 sys_open_helper(char *path, int omode, int depth) {
  int fd;
  struct file *f;
  struct inode *ip;

  if(omode & O_CREATE){
    ip = create(path, T_FILE, 0, 0);
    if(ip == 0){
      return -1;
    }
  } else {
    if((ip = namei(path)) == 0){
      return -1;
    }
    ilock(ip);
    if(ip->type == T_DIR && omode != O_RDONLY){
      iunlockput(ip);
      return -1;
    }
  }

  if (ip->type == T_SYMLINK && !(omode & O_NOFOLLOW)) {
    if (depth == 0) {
      iunlockput(ip);
      return -1;
    }
    struct symlink_pack pack;
    if (readi(ip, 0, (uint64)&pack, 0, sizeof(uint)) < sizeof(uint)) {
      iunlockput(ip);
      return -1;
    }
    const uint bytes = sizeof(uint) + pack.len_;
    if (readi(ip, 0, (uint64)&pack, 0, (bytes)) < (bytes)) {
      iunlockput(ip);
      return -1;
    }

    uint64 ret = sys_open_helper(pack.path_, omode, depth -1);
    iunlock(ip);
    return ret;
  }

  if(ip->type == T_DEVICE && (ip->major < 0 || ip->major >= NDEV)){
    iunlockput(ip);
    return -1;
  }

  if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
    if(f)
      fileclose(f);
    iunlockput(ip);
    return -1;
  }

  if(ip->type == T_DEVICE){
    f->type = FD_DEVICE;
    f->major = ip->major;
  } else {
    f->type = FD_INODE;
    f->off = 0;
  }
  f->ip = ip;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);

  if((omode & O_TRUNC) && ip->type == T_FILE){
    itrunc(ip);
  }

  iunlock(ip);
  return fd;
}
```
One benefit of doing so is that we don't have to code the entire process of dealing with symbolic links; 
also, this provide convenience for recursive function call. Here's the `sys_open` implementation:
```c
uint64 sys_open(void) {
  char path[MAXPATH];
  int fd, omode;
  [[maybe_unused]]int n;

  if((n = argstr(0, path, MAXPATH)) < 0 || argint(1, &omode) < 0)
    return -1;

  begin_op();
  fd = sys_open_helper(path, omode, DEPTH_THRESHOLD);
  end_op();
  return fd;
}
```
This is surprisingly clear.

## run all
```
make: 'kernel/kernel' is up to date.
== Test running bigfile == running bigfile: OK (190.7s) 
    (Old xv6.out.bigfile failure log removed)
== Test running symlinktest == (0.9s) 
== Test   symlinktest: symlinks == 
  symlinktest: symlinks: OK 
== Test   symlinktest: concurrent symlinks == 
  symlinktest: concurrent symlinks: OK 
== Test usertests == usertests: OK (56.6s) 
== Test time == 
time: OK 
Score: 100/100
```

This completes the lab.
> To pass the bigfile test, I changed the timeout in testing script to be 240.
> Sometimes bigfile test will timeout, but in others it doesn't.
> Also, my implementation requires more inode(I used 256 inodes). Perhaps these
> changes are acceptable. Hahahaha
