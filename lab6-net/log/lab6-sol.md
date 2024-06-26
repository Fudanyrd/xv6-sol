# Lab 6: Network Driver
[handout](https://pdos.csail.mit.edu/6.828/2021/labs/net.html)

## Prerequisites
Read this developer [manual](https://pdos.csail.mit.edu/6.828/2021/readings/8254x_GBe_SDM.pdf)
Personally I feel that this manual is large enough to confuse people like me. 
So I'm going to just skip reading it and start coding.

## e1000 recv

## e1000 transmit 
Read the starter code, notice that a lock is needed because this method may be run simutaneously
by multiple cpus.
```c
int e1000_transmit(struct mbuf *m) {
  acquire(&e1000_lock);
  //
  // Your code here.
  //
  // the mbuf contains an ethernet frame; program it into
  // the TX descriptor ring so that the e1000 sends it. Stash
  // a pointer so that it can be freed after sending.
  //
  
  release(&e1000_lock);
  return 0;
}
```

Hmm, these descriptions are still a little blur to me. I suggest reading hints in the handout instead.

And here's my implementation:
```c
int e1000_transmit(struct mbuf *m) {
  acquire(&e1000_lock);

  // the tail of tx queue
  uint32 tail = regs[E1000_TDT];
  // the head of tx queue
  uint32 head = regs[E1000_TDH];
  if (head == tail && !(tx_ring[tail].status & E1000_TXD_STAT_DD)) {
    // cannot overwrite
    release(&e1000_lock);
    return -1;
  } else {
    // free previous descriptor
    if (tx_mbufs[tail] != 0) {
      mbuffree(tx_mbufs[tail]);
    }
  }
  // stash m for later freeing
  tx_mbufs[tail] = m;

  // build transmit descriptor 
  struct tx_desc *txd = &(tx_ring[tail]);
  txd->addr = (uint64)m->head;
  txd->length = (uint16)m->len;
  txd->cmd = E1000_TXD_CMD_RS | E1000_TXD_CMD_EOP;
  txd->status = 0x00;

  [[maybe_unused]]volatile uint8 finished = 0;
  // update the ring position
  regs[E1000_TDT] = (tail + 1) % TX_RING_SIZE;

  // release lock, over
  release(&e1000_lock);
  return 0;
}
```

```c
static void e1000_recv(void) {
  uint32 tail;

// start of loop
recv_start:
  tail = regs[E1000_RDT];
  tail = (tail + 1) % RX_RING_SIZE;

  // check if a new packet is available
  if (!(rx_ring[tail].status & E1000_RXD_STAT_DD)) {
    return;
  }
  struct mbuf *m = rx_mbufs[tail];
  m->len = rx_ring[tail].length;
  
  // deliver the mbuf to the network stack
  net_rx(m);
  
  // allocate a new mbuf to replace the one just given to net_rx
  rx_mbufs[tail] = mbufalloc(0);
  if (!rx_mbufs[tail]) {
    // failure
    panic("e1000_recv");
  }
  // clear the descriptor's status bits to zero. 
  rx_ring[tail].status = 0x00;
  rx_ring[tail].addr = (uint64) rx_mbufs[tail]->head;
  
  // update the E1000_RDT register
  regs[E1000_RDT] = tail;
  
  // I should have used a loop instead...
  goto recv_start;
}
```

> Only pitfall is that sometimes DNS test takes a long time. If so, consider 
> rebooting your wsl(or whatever virtual machines) and rerun test. In my case
> rebooting helped.

## run all
```
root@yrd:/mnt/d/handouts/xv6/code# ./grade-lab-net
make: 'kernel/kernel' is up to date.
== Test running nettests == (2.7s) 
== Test   nettest: ping == 
  nettest: ping: OK 
== Test   nettest: single process ==
  nettest: single process: OK
== Test   nettest: multi-process ==
  nettest: multi-process: OK
== Test   nettest: DNS ==
  nettest: DNS: OK
== Test time ==
time: OK
Score: 100/100
```

This completes netdriver lab.