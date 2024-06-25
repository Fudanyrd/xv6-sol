	.text

	/*
         * save the old thread's registers,
         * restore the new thread's registers.
         */

	.globl thread_switch
thread_switch:
	/* YOUR CODE HERE */
	/* store registers of prev thread */
  sd ra, 0(a0)
  sd sp, 8(a0)
  sd gp, 16(a0)
  sd tp, 24(a0)
  sd t0, 32(a0)
  sd t1, 40(a0)
  sd t2, 48(a0)
  sd s0, 56(a0)
  sd s1, 64(a0)
  sd a1, 72(a0)
  sd a2, 80(a0)
  sd a3, 88(a0)
  sd a4, 96(a0)
  sd a5, 104(a0)
  sd a6, 112(a0)
  sd a7, 120(a0)
  sd s2, 128(a0)
  sd s3, 136(a0)
  sd s4, 144(a0)
  sd s5, 152(a0)
  sd s6, 160(a0)
  sd s7, 168(a0)
  sd s8, 176(a0)
  sd s9, 184(a0)
  sd s10, 192(a0)
  sd s11, 200(a0)
  sd t3, 208(a0)
  sd t4, 216(a0)
  sd t5, 224(a0)
  sd t6, 232(a0)

	/* load registers of thread to execute */
	add t0, a1, x0
  ld ra, 0(t0)
  ld sp, 8(t0)
  ld gp, 16(t0)
  ld tp, 24(t0)
  ld t1, 40(t0)
  ld t2, 48(t0)
  ld s0, 56(t0)
  ld s1, 64(t0)
  ld a1, 72(t0)
  ld a2, 80(t0)
  ld a3, 88(t0)
  ld a4, 96(t0)
  ld a5, 104(t0)
  ld a6, 112(t0)
  ld a7, 120(t0)
  ld s2, 128(t0)
  ld s3, 136(t0)
  ld s4, 144(t0)
  ld s5, 152(t0)
  ld s6, 160(t0)
  ld s7, 168(t0)
  ld s8, 176(t0)
  ld s9, 184(t0)
  ld s10, 192(t0)
  ld s11, 200(t0)
  ld t3, 208(t0)
  ld t4, 216(t0)
  ld t5, 224(t0)
  ld t6, 232(t0)
  ld t0, 32(t0)

	ret    /* return to ra */
