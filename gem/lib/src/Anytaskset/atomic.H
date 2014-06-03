#ifndef ATOMIC_H
#define ATOMIC_H
/* for GCC internals see
   http://gcc.gnu.org/onlinedocs/gcc-4.4.1/gcc/Atomic-Builtins.html */

/* We use only 0-1 values with the lock */

typedef char sfp_lock_t;

/* nop, backoff, and slowpath locking code based on RSTM 
http://www.cs.rochester.edu/~sandhya/csc258/assignments/ass2/atomic_ops.h*/

static inline void nop( ) {
  asm volatile("nop");
}

static inline void backoff( int *b ) {
  int i;
    for (i = *b; i; i--)
        nop();

    if (*b < 4096)
        *b <<= 1;
}

static inline void lock_acquire_slowpath(char *lock)
{
    int b = 64;

    do
    {
        backoff(&b);
    }
    while (__sync_bool_compare_and_swap( (lock), 0, 1 ) == 0);
}

static inline void lock_acquire(sfp_lock_t *lock) {
  if(__sync_bool_compare_and_swap((lock), 0, 1) == 0) {
    lock_acquire_slowpath(lock);
  }
}

static inline void sfp_lock_acquire( sfp_lock_t *lock, unsigned long long pos ) {
  int b = 64;
  unsigned long long index = pos>>3;
  int mask = 1<<(pos&7);
  if( (mask&(__sync_fetch_and_or(&lock[index], mask))) != 0) {
    do
    {
      backoff(&b);
    }
    while( (mask&(__sync_fetch_and_or(&lock[index], mask))) != 0);
  }
}
#define lock_release( lock ) *(lock) = 0

static inline void sfp_lock_release( sfp_lock_t *lock, unsigned long long pos) {
  unsigned long long index = pos>>3;
  int mask = 1<<(pos&7);
  __sync_fetch_and_xor(&lock[index], mask);
}

static inline void sfp_wait_flag( sfp_lock_t *flag ) {
  int b = 64;
  while ( ! *flag ) 
    backoff( &b );
}

//#define sfp_lock_clear( lock ) sfp_lock_release( (lock) )

#endif
