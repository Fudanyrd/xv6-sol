# Lab 7: locks

## Memory Allocator
First let's code a simple allocator for each cpu(in <a href="../kernel/kalloc.c">kernel/kalloc.c</a>)

### allocator struct
```c
/** allocator belongs to a single cpu */
struct kallocator {
  /** head of free list */
  struct run *freelist_;
  /** number of avaiable blocks, for debugging */
  uint32 avail_blk_;
  /** lock needed to avoid racing */
  struct spinlock lock_;
} kallocators[NCPU];
```
### allocator init
```c
/** initialize an allocator */
void kalloc_init(struct kallocator *alloc) {
  alloc->freelist_ = 0;
  alloc->avail_blk_ = 0;
  initlock(&alloc->lock_, "allocator lock");
}
```

### allocator free
```c
/** free a block to an allocator */
void kalloc_free(struct kallocator *alloc, void *pa) {
  // validate alloc
  if (alloc == 0) {
    panic("kalloc_free: alloc is null");
  }
  // validate physical address
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end 
    || (uint64)pa >= PHYSTOP) {
    printf("NOTE: pa = %p\n", pa);
    panic("kalloc_free");
  }

  // add to the head of free list of the kernel 
  acquire(&(alloc->lock_));
  struct run *r = (struct run *)pa;
  r->next = alloc->freelist_;
  alloc->freelist_ = r;
  ++alloc->avail_blk_;
  release(&(alloc->lock_));
}
```

### allocator alloc
This function name is a bit weired...
```c
void *kalloc_alloc(struct kallocator *alloc) {
  // look into the free list first
  acquire(&(alloc->lock_));
  if (alloc->freelist_) {
    if (!alloc->avail_blk_) {
      panic("incosistent allocator!");
    }

    // ok, not empty, can be allocated
    struct run *r = alloc->freelist_;
    alloc->freelist_ = r->next;
    alloc->avail_blk_ -= 1; 
    release(&(alloc->lock_));

    return r;
  } 

  release(&(alloc->lock_));
  return 0;  // mark as failure, need to steal
}
```

### result
Then rewrite `kalloc` and `kfree` such that they call the `kalloc_alloc` and `kalloc_free` to 
do the stuff. When `kalloc_alloc` cannot allocate pages, ask for the pages held by other cpus,
the logic is similar to the following:
```c
void *kalloc(void){
  const int cpu = kcpuid();
  void *ret = kalloc_alloc(&(kallocators[cpu]));

  if (ret != 0) {
    memset(ret, 5, PGSIZE);
    return ret;
  }

  // else have to borrow pages!
  for (int i = NCPU - 1; i >= 0; i--) {
    if (i == cpu) { continue; }
    ret = kalloc_alloc(&(kallocators[i]));
    
    if (ret != 0) {
      memset(ret, 5, PGSIZE);
      return ret;
    } 
  }

  // no page can be borrowed, abort
  return 0;
}
```

Here we should follow the tradition of filling garbage using `memset` in xv6.

Now we should pass `kalloctest`.
```
init: starting sh
$ kalloctest
start test1
test1 results:
--- lock kmem/bcache stats
lock: bcache: #test-and-set 0 #acquire() 346
--- top 5 contended locks:
lock: proc: #test-and-set 20344 #acquire() 317585
lock: proc: #test-and-set 19666 #acquire() 317586
lock: proc: #test-and-set 16798 #acquire() 317546
lock: proc: #test-and-set 16387 #acquire() 317546
lock: proc: #test-and-set 16312 #acquire() 317546
tot= 0
test1 OK
start test2
total free number of pages: 32498 (out of 32768)
.....
test2 OK
```

## Buffer cache
<b style="color: red">TODO</b>

Here's a hash table package for you to start with:
```c
// number of bio htable buckets
#define BIO_HTBUCKETS 31U

/** 
 * hashtable bucket
 * We can have fixed size of entry, for the maximum
 * number of buffer is NBUF. Operations on a bucket
 * includes lookup, put, remove, they are all atomic.
 */
struct bio_bucket {
  // pointer to buffers
  struct buf *bufs_[NBUF];
  // number of valid bufs
  uint valid_;
  // lock of the bucket
  struct spinlock lock_;
};
/** initialize a bucket */
void 
init_bio_bucket(struct bio_bucket *bkt) {
  for (int i = 0; i < NBUF; ++i) {
    bkt->bufs_[i] = 0;
  }
  bkt->valid_ = 0;
  initlock(&bkt->lock_, "");
}

/** lookup a buffer in the bucket, return 0 if not found */
struct buf *
lookup_bio_bucket(struct bio_bucket *bkt, uint dev, uint blockno) {
  uint it = 0;
  for (; it < bkt->valid_; ++it) {
    if(bkt->bufs_[it]->blockno == blockno
    && bkt->bufs_[it]->dev == dev) {
      return bkt->bufs_[it];
    }
  }

  return 0;
}
/** put a buffer into the bucket */
void
put_bio_bucket(struct bio_bucket *bkt, struct buf *bf) {
  bkt->bufs_[bkt->valid_] = bf;
  bkt->valid_ += 1;
  if (bkt->valid_ > NBUF) {
    panic("ht overflow");
  }
}
/** remove a buffer in the bucket */
void
remove_bio_bucket(struct bio_bucket *bkt, uint dev, uint blockno) {
  // lookup in the bucket
  uint idx = NBUF;
  for (uint i = 0; i < bkt->valid_; ++i) {
    struct buf *b = bkt->bufs_[i];
    if (b->blockno == blockno && b->dev == dev) {
      idx = i;
      break;
    }  
  }

  // not found, release the lock, return
  if (idx == NBUF) {
    panic("remove_bio_bucket: not found");
    return;
  }

  // move bucket entries in bulks
  for (uint k = idx + 1; k < bkt->valid_; ++k) {
    bkt->bufs_[k - 1] = bkt->bufs_[k];
  }
  bkt->valid_ -= 1;

}

/** hash table */
struct bio_htable {
  struct bio_bucket buckets_[BIO_HTBUCKETS];
} bcache_ht;

static inline uint
hash_to_bucket_idx(uint blockno) {
  return blockno % BIO_HTBUCKETS;
}
static inline void
lock_bio_bucket(uint bucketno) {
  acquire(&(bcache_ht.buckets_[bucketno].lock_));
}
static inline void
unlock_bio_bucket(uint bucketno) {
  release(&(bcache_ht.buckets_[bucketno].lock_));
}
```

> I feel that there is no safe way to concurrently execute bget, brelse,
> bpin, bunpin. So I'll just use a global lock that serializes all buffer
> operations. 

## run all
```
== Test running kalloctest == (52.7s) 
== Test   kalloctest: test1 ==
  kalloctest: test1: OK
== Test   kalloctest: test2 ==
  kalloctest: test2: OK
== Test kalloctest: sbrkmuch == kalloctest: sbrkmuch: OK (7.9s) 
== Test running bcachetest == (11.0s) 
== Test   bcachetest: test0 ==
  bcachetest: test0: FAIL
    ...
         tot= 35998
         test0: FAIL
         start test1
         test1 OK
         $ qemu-system-riscv64: terminating on signal 15 from pid 33095 (make)
    MISSING '^test0: OK$'
== Test   bcachetest: test1 ==
  bcachetest: test1: OK
== Test usertests == usertests: OK (124.0s) 
== Test time == 
time: OK 
Score: 60/70
```
<b style="color: red">FAIL TO COMPLETE THIS LAB!!!</b>
