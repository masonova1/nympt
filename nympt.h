#ifndef NYMPT_H_INCLUDED
#define NYMPT_H_INCLUDED

//#define PROTOTHREAD_NO_SIGNALS

struct protothread_pcntl_s;
struct protothread_s;

// Protothread signal interrupt handler. These are passed a pointer to the calling protothread.
typedef void(*protothread_sigint_handler_t)(struct protothread_s*);

// Protothread process control structure.
typedef struct protothread_pcntl_s {
  int16_t line;           // Execution control
  int16_t exc_ret;        // Exception return
#ifndef PROTOTHREAD_NO_SIGNALS
  uint8_t sig;            // Priority signal register
  protothread_sigint_handler_t* sigint_vectors; // Priority signal interrupt vectors
#endif
} protothread_pcntl_t;

// Protothread structure.
typedef struct protothread_s {
  protothread_pcntl_t pcntl;
} protothread_t;

#define PTEXEC_RUNNING    (0) // This is returned by thread bodies when the thread is not in a reserved finality, exception, or 'terminate' state.
#define PTEXEC_COMPLETE   (1) // This is returned by thread bodies when the thread has concluded via protothread_exit(), and also serves as its own case label.
#define PTEXEC_FAIL       (2) // This is returned by thread bodies when the thread has concluded via protothread_exit_fail(), and also serves as its own case label.
#define PTEXEC_TERMINATE  (3) // This is returned by thread bodies when the thread is running its terminate handler, and also serves as its own case label.
#define PTEXEC_TERMINATED (4) // This is returned by thread bodies when the thread has concluded via protothread_exit_terminated(), and also serves as its own case label.
#define PTEXEC_KILLED     (5) // This is returned by thread bodies when the thread has concluded via system kill, and also serves as its own case label.
#define PTEXEC_EXCEPTION  (127) // This is returned by threads when an unhandled software exception has occurred.
#define PTEXEC_RESERVED_CODES (5) // Successful exit, failure exit, terminate, terminated, killed (exceptions are discounted)

// This declares a protothread code body.
#define protothread_entry(NAME)         static char NAME(protothread_t* this_pt)

// This sets a thread's state to its entrypoint. Warning: Does not reset static variables in thread function bodies. These _must_ be explicitly re-initialized in software.
#ifndef PROTOTHREAD_NO_SIGNALS
#define protothread_init(T)             do { (T)->pcntl.line = 0; (T)->pcntl.sigint_vectors = NULL; } while(0)
#else
#define protothread_init(T)             do { (T)->pcntl.line = 0; } while(0)
#endif

// Momentary execution yield to scheduling.
#define protothread_yield()             do { this_pt->pcntl.line = __LINE__; return PTEXEC_RUNNING; case __LINE__: } while(0) \


// Conditional execution yield to scheduling.
#define protothread_yield_if(COND)      if(COND) { this_pt->pcntl.line = __LINE__; return PTEXEC_RUNNING; case __LINE__: } \


// Yield execution while a condition is false.
#define protothread_yield_until(COND)   do { this_pt->pcntl.line = __LINE__; case __LINE__: \
                                        if(!(COND)) return PTEXEC_RUNNING; } while(0)

// Yield execution while a condition is true.
#define protothread_yield_while(COND)   do { this_pt->pcntl.line = __LINE__; case __LINE__: \
                                        if(COND) return PTEXEC_RUNNING; } while(0)

// This sets a thread's state to a reserved successful exit finality. After this statement, thread control enters an indefinite yield until re-initialization.
#define protothread_exit()              do { this_pt->pcntl.line = -PTEXEC_COMPLETE; return PTEXEC_COMPLETE; } while(0)

// This sets a thread's state to a reserved failure exit finality. After this statement, thread control enters an indefinite yield until re-initialization.
#define protothread_exit_fail()         do { this_pt->pcntl.line = -PTEXEC_FAIL; return PTEXEC_FAIL; } while(0)

// This sets a thread's state to a reserved terminated exit finality. After this statement, thread control enters an indefinite yield until re-initialization.
#define protothread_exit_terminated()   do { this_pt->pcntl.line = -PTEXEC_TERMINATED; return PTEXEC_TERMINATED; } while(0)

// This can be called on one thread by another to immediately set control to the terminate handler, if it exists.
#define protothread_terminate(T)        do { (T)->pcntl.line = -PTEXEC_TERMINATE; if((T) == this_pt) return PTEXEC_TERMINATE; } while(0)

// This can
#define protothread_kill(T)             do { (T)->pcntl.line = -PTEXEC_KILLED; } while(0)

#define protothread_terminate_handler   case -PTEXEC_TERMINATE: if(this_pt->pcntl.line == -PTEXEC_TERMINATE)

#ifndef PROTOTHREAD_NO_SIGNALS

#define protothread_sigint_handle()     if(this_pt->pcntl.sigint_vectors == NULL) {} else { \
                                          int i = 0; \
                                          while((this_pt->pcntl.sig != 0) && (i < sizeof(this_pt->pcntl.sig)*8)) { \
                                            if(this_pt->pcntl.sig & (1u << i)) { \
                                              this_pt->pcntl.sig &= ~(1u << i); /* Reset signal bit */ \
                                              if(this_pt->pcntl.sigint_vectors[i] != NULL) { \
                                                this_pt->pcntl.sigint_vectors[i](this_pt); \
                                              } \
                                            } i++; \
                                          } \
                                        }
#else

#define protothread_sigint_handle()     do {} while(0)

#endif

// Ugly, necessary wrappers for protothread control. These should come after any STATIC in-scope member declarations for the concurrent code.
#define protothread_begin()             /* printf("this_pt is %i\n\r", this_pt->pcntl.line); */ \
                                        /* _pt_preempt(0); */ \
                                        protothread_sigint_handle(); \
                                        protothread_case_entry: \
                                        switch(this_pt->pcntl.line) { \
                                        /* This is where all of our reserved routines go. */ \
                                        /* In theory, arbitrarily many reserved execution cases can be added with negligible overhead. */ \
                                        case -PTEXEC_COMPLETE: return PTEXEC_COMPLETE; \
                                        case -PTEXEC_FAIL: return PTEXEC_FAIL; \
                                        case -PTEXEC_TERMINATED: return PTEXEC_TERMINATED; \
                                        case -PTEXEC_KILLED: return PTEXEC_KILLED; \
                                        case 0:

#define protothread_end()               } \
                                        do { this_pt->pcntl.exc_ret = 0; if(this_pt->pcntl.line < -PTEXEC_RESERVED_CODES) { \
                                        /* Unhandled exception panic goes here */ \
                                          return PTEXEC_EXCEPTION; \
                                        } else { \
                                          switch(this_pt->pcntl.line) { \
                                            default: \
                                              protothread_exit(); \
                                            case -PTEXEC_TERMINATE: \
                                              protothread_exit_terminated(); \
                                          } \
                                        } } while(0)

#define PTPRIORITY_DEFAULT (8)

const static inline int _pt_preempt(const int p) {
  //printf("Scheduling timer invoked.\n\r");
  static uint8_t __protothread_scheduling_timer = -1; // Allows iteration priority scheduling without rote loop unrolling
  // printf("Scheduler level is %i\n\r", __protothread_scheduling_timer);
  register int ret = 0;
  ++__protothread_scheduling_timer;
  if(ret = ((__protothread_scheduling_timer) >= p)) {
    __protothread_scheduling_timer = 0;
  }

  return ret;
}

// Yield execution once every S calls for iteration (Larger S means higher priority)
#define protothread_tight_loop(S)        do { if(_pt_preempt(S)) protothread_yield(); } while(0)

#define pt_while(COND)     while(COND) if(_pt_preempt(PTPRIORITY_DEFAULT)) { this_pt->pcntl.line = __LINE__; return PTEXEC_RUNNING; } else case __LINE__: if(0) {} else \


#define pt_do              do if(_pt_preempt(PTPRIORITY_DEFAULT)) { this_pt->pcntl.line = __LINE__; return PTEXEC_RUNNING; } else case __LINE__: if(0) {} else \


#define pt_for(FORARGS)    for(FORARGS) if(_pt_preempt(PTPRIORITY_DEFAULT)) { this_pt->pcntl.line = __LINE__; return PTEXEC_RUNNING; } else case __LINE__: if(0) {} else \


// === Protothread exceptions ===

// Get the current exception state / number of a thread T.
#define protothread_exception_get(T)         (-((T)->pcntl.line+PTEXEC_RESERVED_CODES+1))

// Get the current exception state / number of this thread.
#define protothread_exception()              protothread_exception_get(this_pt)

// Case handle for exception number X. (WARNING: Do not yield in exception handlers.)
#define protothread_exception_handler(X)     case -(X+PTEXEC_RESERVED_CODES+1): if(this_pt->pcntl.line != -(X+PTEXEC_RESERVED_CODES+1)) {} else

#define protothread_exception_handler_any    default: if(this_pt->pcntl.line >= -(PTEXEC_RESERVED_CODES)) {} else

// Return to pre-exception case and continue regular execution.
#define protothread_exception_return()       do { this_pt->pcntl.line = this_pt->pcntl.exc_ret; this_pt->pcntl.exc_ret = 0; goto protothread_case_entry; } while(0)

// Raise exception number X and branch to handler.
#define protothread_exception_raise(X)       do { this_pt->pcntl.exc_ret = __LINE__; this_pt->pcntl.line = -(X+PTEXEC_RESERVED_CODES+1); goto protothread_case_entry; case __LINE__: } while(0) \


// === Protothread signals ===
#ifndef PROTOTHREAD_NO_SIGNALS
// Raise signal S to thread T.
#define protothread_signal_raise(T, S)       do { (T)->pcntl.sig |= (1u << ((S) & 0x3)); } while(0)

// This sets a protothread T's sigint vectors to a set of signal handlers, ordered by priority, up to a maximum of 8.
#define protothread_set_sigint_vectors(T, V) do { (T)->pcntl.sigint_vectors = (V); } while(0)

// Attach handle H to signal S for the current thread.
//#define protothread_sighandler_set(S, H)     do { if(S == 0) this_pt->pcntl.sigint_vectors[0] = (H); else this_pt->pcntl.sigint_vectors[(1u << ((S) & 0x3))] = (H); } while(0)
#define protothread_sighandler_set(S, H)     do { this_pt->pcntl.sigint_vectors[(S == 0) ? 0 : (1u << ((S) & 0x3))] = (H); } while(0)

#endif
// === Protothread synchronization ===

// If the thread is not terminating, running a positive case, or an exception, it is joinable.
#define protothread_joinable(T)        (((T)->pcntl.line < 0) && ((T)->pcntl.line >= -PTEXEC_RESERVED_CODES) && ((T)->pcntl.line != -PTEXEC_TERMINATE))

// If the thread is in some zero or positive case value, in an exception state, or in its terminate handler, it is running.
#define protothread_running(T)         (((T)->pcntl.line >= 0) || ((T)->pcntl.line < -PTEXEC_RESERVED_CODES) || ((T)->pcntl.line == -PTEXEC_TERMINATE))

// While thread T is in a reserved finality state, yield.
#define protothread_detach(T)          protothread_yield_while(protothread_joinable(T))

// Yield until thread T enters a reserved finality state.
#define protothread_join(T)            protothread_yield_until(protothread_joinable(T))

// === Protothread spinlocks and read-write locks ===

// This is a VERY primitive, unsafe spinlock implementation which does not consider thread ownership. Use with care.

typedef struct protothread_spinlock_s {
  unsigned char m;
} protothread_spinlock_t;

// typedef unsigned char protothread_spinlock_t;

// Initialize spinlock to 'unlocked' state.
#define protothread_lock_init(L)       (L)->m = 0

// If lockable, lock and return 1. Else, return 0.
#define protothread_trylock(L)         (((L)->m == 0) ? ((L)->m = 1) : 0)

// Yield until this thread can take the spinlock.
#define protothread_lock(L)            protothread_yield_until(protothread_trylock(L))

// Free the spinlock.
// Warning: Can be called by ANY thread on ANY spinlock.
#define protothread_unlock(L)          (L)->m = 0

// Read-write lock implementation due to Raynal.
typedef struct protothread_rw_lock_s {
  protothread_spinlock_t r;
  protothread_spinlock_t g;
  unsigned short int b;
} protothread_rw_lock_t;

// This initializes a read-write lock in the "unlocked read and write" state.
#define protothread_rw_lock_init(L)       do { (L)->b = 0; protothread_lock_init(&((L)->r)); protothread_lock_init(&((L)->g)); } while(0) \


#define protothread_read_lock(L)          do { protothread_lock(&((L)->r)); (L)->b++; \
                                          if((L)->b == 1) protothread_lock(&((L)->g)); protothread_unlock(&((L)->r)); } while(0) \


#define protothread_read_unlock(L)        do { protothread_lock(&((L)->r)); (L)->b--; \
                                          if((L)->b == 0) protothread_unlock(&((L)->g)); protothread_unlock(&((L)->r)); } while(0) \


#define protothread_write_lock(L)         protothread_lock(&((L)->g)) \


#define protothread_write_unlock(L)       protothread_unlock(&((L)->g)) \


#endif // NYMPT_H_INCLUDED
