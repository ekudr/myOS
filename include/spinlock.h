
#ifndef __SPINLOCK_H__
#define __SPINLOCK_H__

// Mutual exclusion lock.
struct spinlock {
  unsigned int locked;       // Is the lock held?

  // For debugging:
  char *name;        // Name of lock.
  struct cpu *cpu;   // The cpu holding the lock.
};

typedef struct spinlock spinlock_t;

void initlock(struct spinlock *lk, char *name);
void acquire(struct spinlock *lk);
void release(struct spinlock *lk);
int holding(struct spinlock *lk);
void push_off(void);
void pop_off(void);


#endif  /* _SPINLOCK_H */