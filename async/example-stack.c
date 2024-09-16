/**
 * This is an example for a basic future and for using the stack field
 *
 * A separate stack field is included for the async struct since traditional
 * stack-allocate variables cannot be safely used. Instead you need to define a
 * custom struct with all of the local variables you would use for the async
 * function in question. You would then need to create an instance of that
 * struct and pass it as a pointer to the stack field in the async struct for
 * your function call. You cannot make a stack-allocated instance of a stack
 * struct for one async function within another since like stated before,
 * stack-allocated variables cannot be used safely.
 *
 * The stack-field was added so that staticly-allocated global variables can be
 * avoided. Hopefully to make the code safer and reentrant.
 *
 * Although I did say that traditional stack-allocated variables cannot be
 * safely used in an async function, they can with the understanding that the
 * variable's lifetime is only valid before its first async operation. For
 * example:
 *
 * async foo(struct async *pt)
 * {
 *     async_begin(pt);
 *     var a = ...; <- a is created, a normal stack-allocated variable
 *     ...; <- Some other stuff, a can be used. No async operations
 *     bar(a); <- a is still valid since there has not been any async operations
 *     await(lock); <- First async operation, a is no longer valid
 *     var b = ...; <- b is created, also a normal stack-allocated variable
 *     bar(b); <- Also fine, b has not be invalided by an async operation
 *     bar(a); <- Invalid, a lifetime ended with the await call above
 *     async_end;
 * }
 */

#include "async.h"
#include <stdio.h>  /* For prints */
#include <string.h> /* For strncpy */

/**
 * This struct is used to define all of the local variables used by foo() that
 * have to be tracked across multiple async calls
 */
struct foo_stack
{
    int num;             /* Just used to demonstrate the local stack */
    struct async bar_pt; /* A more practical use for tracking the state of an async sub-function */
};

/**
 * Similarly this struct is used to define any local variables used by bar()
 */
struct bar_stack
{
    int *lock;
};

static async foo(struct async *pt);
static async bar(struct async *pt);

/**
 * function used to clobber the stack as a test
 */
static void work(void);

void example_stack(void)
{
    /* Sets up the inital async struct */
    struct async pt = {0};
    async_init(&pt);

    /**
     * Sets up the stack for bar(). The stack can also be used to pass in
     * parameters that will track with the function over its operation
     */
    int lock = 0;
    struct bar_stack bar_stack = {
        .lock = &lock,
    };

    /**
     * Similarly this sets up the stack for foo().
     */
    struct foo_stack stack = {
        .num = 0,
        .bar_pt.stack = &bar_stack,
    };
    pt.stack = &stack;

    puts("Stack Example Start");
    foo(&pt); /* Starts the foo subroutine -> bar -> waits on its lock */
    work();   /* This just clobbers the stack as a test, but could represent this thread doing some other work */
    lock = 1; /* Releases the lock, normally done by an interrupt for an I/O complete */
    foo(&pt); /* Recalls foo, should pick up where it left off and finish */
    puts("Stack Example End");
}

static async
foo(struct async *pt)
{
    struct foo_stack *stack = pt->stack;
    async_begin(pt);

    puts("foo start");
    stack->num = 23; /* stack variables can be used as normal */
    printf("num in the foo stack is %d\n", stack->num);
    int local_num = 11; /* normal stack-allocated variables should be avoided */
    printf("num in the local stack is %d\n", local_num);

    async_init(&stack->bar_pt);
    await(bar(&stack->bar_pt)); /* Calls bar() with its own pt, each async function instance should have its own */

    printf("num in the foo stack is still %d\n", stack->num); /* Should still be the same as before the await call */
    printf("but the local_num is now %d\n", local_num);       /* Is probably not the same */
    puts("foo end");

    async_end;
}

static async
bar(struct async *pt)
{
    struct bar_stack *stack = pt->stack;
    async_begin(pt);

    puts("bar start");
    await(*stack->lock); /* With the stack async functions can be reentrant, allowing for safe multithreading */
    puts("bar end");

    async_end;
}

static void
work(void)
{
    char string[256] = {'\0'};
    strncpy(string, "Hello World!, I am doing a lot of work", sizeof(string) - 1);
    puts(string);
}