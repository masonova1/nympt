#include <stdint.h>
#include <stdio.h>

#include "nympt.h"

#include <signal.h>
#include <unistd.h>
#include <setjmp.h>

static int counter = 0;

static protothread_spinlock_t example_lock;
static protothread_rw_lock_t example_rw_lock;

static void example_sigint_handler(protothread_t* calling_thread) {
  printf("Signal handler called from %p\n\r", calling_thread);
  return;
}

static protothread_sigint_handler_t example_sigint_vectors[] = {
  NULL,
  example_sigint_handler,
};

protothread_t main_pt;
protothread_t example_pt;

// Example thread entrypoint
protothread_entry(example_thread) {
  protothread_begin();

  // This is what a terminate handler looks like. An exit statement is necessary, or fall-through behavior may occur.
  protothread_terminate_handler {
    printf("Example thread terminated\n\r");
    // A process may try to bring down the entire ship with it.
    // while(1) { /* printf("LOL\n\r"); */ }  // Use this to test pre-emptive schedulers.
    protothread_exit_terminated();
  }

  // // This is an example of a general exception handler.
  protothread_exception_handler_any {
    if(protothread_exception() == 0) {
      printf("First exception fired\n\r");
      // This exception might signify a recoverable state.
      protothread_exception_return();
    } else {
      printf("Some other exception fired\n\r");
      protothread_exit_fail(); // Anything else might signify a failure on exit.
    }
  }

  // This thread's job is to keep tabs on the counter.
  do {
    // if the counter is above a threshold, reset it
    printf("While body executing\n\r");
    protothread_yield_until(counter >= 20);
    printf("While return-from-yield\n\r");
    if(counter >= 20) {
      counter = 0;
      protothread_exception_raise(0);
      printf("Protothread reset counter!\n\r");
    }
  } while(1);
  protothread_end();
}

// Main thread entrypoint
protothread_entry(main_thread) {
  static int i;
  protothread_begin();

  protothread_detach(&example_pt);

  // Increment the counter 100 times
  pt_for(i = 0; i < 100; i++) {
    printf("For body executing\n\r");
    counter++;
  }

  protothread_terminate(&example_pt);

  //protothread_join(&example_pt);

  // End of main thread body
  protothread_end();
}

int main(void) {
  // Initialize example and main threads
  protothread_init(&main_pt);
  protothread_init(&example_pt);

  // Execute protothreads
  while(protothread_running(&main_pt)) {
    // printf("num_proc is %i\n\r", scheduler.num_procs);
    // pt_scheduler_run_all(&scheduler);
    // pt_scheduler_clear_joinable(&scheduler);
    // printf("num_proc is %i\n\r", scheduler.num_procs);
    main_thread(&main_pt);
    example_thread(&example_pt);
  }

  printf("returning\n\r");
  return 0;
}
