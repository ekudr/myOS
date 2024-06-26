#include <common.h>

#ifndef _SYS_RISCV_H
#define _SYS_RISCV_H

// Supervisor Status Register, sstatus

#define SSTATUS_SPP (1L << 8)  // Previous mode, 1=Supervisor, 0=User
#define SSTATUS_SPIE (1L << 5) // Supervisor Previous Interrupt Enable
#define SSTATUS_UPIE (1L << 4) // User Previous Interrupt Enable
#define SSTATUS_SIE (1L << 1)  // Supervisor Interrupt Enable
#define SSTATUS_UIE (1L << 0)  // User Interrupt Enable

static inline uint64 r_sstatus()
{
  uint64 x;
  asm volatile("csrr %0, sstatus" : "=r" (x) );
  return x;
}

static inline void w_sstatus(uint64 x)
{
  asm volatile("csrw sstatus, %0" : : "r" (x));
}

// Supervisor Interrupt Pending
static inline uint64 r_sip() {
  uint64 x;
  asm volatile("csrr %0, sip" : "=r" (x) );
  return x;
}

static inline void w_sip(uint64 x) {
  asm volatile("csrw sip, %0" : : "r" (x));
}

// Supervisor Interrupt Enable
#define SIE_SEIE (1L << 9) // external
#define SIE_STIE (1L << 5) // timer
#define SIE_SSIE (1L << 1) // software

static inline uint64 r_sie() {
  uint64 x;
  asm volatile("csrr %0, sie" : "=r" (x) );
  return x;
}

static inline void w_sie(uint64 x) {
  asm volatile("csrw sie, %0" : : "r" (x));
}


// Supervisor Trap-Vector Base Address
// low two bits are mode.
static inline void w_stvec(uint64 x) {
  asm volatile("csrw stvec, %0" : : "r" (x));
}

static inline uint64 r_stvec() {
  uint64 x;
  asm volatile("csrr %0, stvec" : "=r" (x) );
  return x;
}

// supervisor exception program counter, holds the
// instruction address to which a return from
// exception will go.
static inline void w_sepc(uint64 x) {
  asm volatile("csrw sepc, %0" : : "r" (x));
}

static inline uint64 r_sepc() {
  uint64 x;
  asm volatile("csrr %0, sepc" : "=r" (x) );
  return x;
}

// Supervisor Trap Cause
static inline uint64 r_scause() {
  uint64 x;
  asm volatile("csrr %0, scause" : "=r" (x) );
  return x;
}

// Supervisor Trap Value
static inline uint64 r_stval() {
  uint64 x;
  asm volatile("csrr %0, stval" : "=r" (x) );
  return x;
}

// read and write tp, the thread pointer, which xv6 uses to hold
// this core's hartid (core number), the index into cpus[].
static inline uint64 r_tp() {
  uint64 x;
  asm volatile("mv %0, tp" : "=r" (x) );
  return x;
}

static inline void w_tp(uint64 x) {
  asm volatile("mv tp, %0" : : "r" (x));
}

static inline uint64 r_sp() {
  uint64 x;
  asm volatile("mv %0, sp" : "=r" (x) );
  return x;
}

// use riscv's sv39 page table scheme.
#define SATP_SV39 (8L << 60)

#define MAKE_SATP(pagetable) (SATP_SV39 | (((uint64)pagetable) >> 12))

// supervisor address translation and protection;
// holds the address of the page table.
static inline void 
w_satp(uint64 x)
{
  asm volatile("csrw satp, %0" : : "r" (x));
}

static inline uint64
r_satp()
{
  uint64 x;
  asm volatile("csrr %0, satp" : "=r" (x) );
  return x;
}

// enable device interrupts
static inline void intr_on() {
  w_sstatus(r_sstatus() | SSTATUS_SIE);
}

// disable device interrupts
static inline void intr_off() {
  w_sstatus(r_sstatus() & ~SSTATUS_SIE);
}

// are device interrupts enabled?
static inline int intr_get() {
  uint64 x = r_sstatus();
  return (x & SSTATUS_SIE) != 0;
}

#endif /* _SYS_RISCV_H */