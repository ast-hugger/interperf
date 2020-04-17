/*
    Derived from handlercode.c:

        - On frame push, pushing the number of arguments below the frame.
        - On frame pop, popping that many arguments after popping the frame itself.
        - This allows handling RET uniformly and eliminating the 'if' in the loop.

    Observations (Clang):

        - very slight (< 5%) performance improvement compared to handlercode.c

 */

#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef uint64_t word_t;

struct function {
    size_t frame_size;
    const word_t *code;
};

struct interpreter {
    const word_t *ip;
    word_t *sp;
    word_t *bp;
    word_t result;
    jmp_buf return_jump;
};

typedef void (*prim_handler_t)(struct interpreter *interp);

// Fetch the next word from the code vector.
static word_t fetch(struct interpreter *interp)
{
    return *(interp->ip)++;
}

static void push(word_t value, struct interpreter *interp)
{
    *(interp->sp)++ = value;
}

static word_t pop(struct interpreter *interp)
{
    return *--(interp->sp);
}

// Return the local var at the specified index in the current frame
static word_t local(size_t index, struct interpreter *interp)
{
    return *(interp->bp + 3 + index);
}

static void push_frame(struct interpreter *interp, word_t args_to_pop_later)
{
    word_t *old_bp = interp->bp;
    interp->bp = interp->sp;
    push((word_t) old_bp, interp);
    push((word_t) interp->ip, interp);
    push(args_to_pop_later, interp);
}

static void pop_frame(struct interpreter *interp)
{
    interp->sp = interp->bp + 3;
    word_t args_to_pop = pop(interp);
    interp->ip = (word_t *) pop(interp);
    interp->bp = (word_t *) pop(interp);
    interp->sp -= args_to_pop;
}

static word_t literals[] = {
    2,
    1
};

static void lessThan(struct interpreter *interp)
{
    int64_t rhs = pop(interp);
    int64_t lhs = pop(interp);
    bool result = lhs < rhs;
    // printf("%lld < %lld -> %d\n", lhs, rhs, result);
    push(result, interp);
}

static void subtract(struct interpreter *interp)
{
    int64_t rhs = pop(interp);
    int64_t lhs = pop(interp);
    int64_t result = lhs - rhs;
    // printf("%lld - %lld -> %lld\n", lhs, rhs, result);
    push(result, interp);
}

static void add(struct interpreter *interp)
{
    int64_t rhs = pop(interp);
    int64_t lhs = pop(interp);
    int64_t result = lhs + rhs;
    // printf("%lld + %lld -> %lld\n", lhs, rhs, result);
    push(result, interp);
}

static prim_handler_t prim_handlers[] = {
    lessThan,
    subtract,
    add
};

static void execute(struct interpreter *interp);

static void execute_lit(struct interpreter *interp)
{
    word_t word = fetch(interp);
    push(literals[word], interp);
}

static void execute_load(struct interpreter *interp)
{
    word_t word = fetch(interp);
    push(local(word, interp), interp);
}

static struct function functions[];

static void execute_call(struct interpreter *interp)
{
    struct function *fun = functions + fetch(interp); // function ID
    word_t argc = fetch(interp);
    word_t *args = interp->sp - argc;
    push_frame(interp, argc);
    memcpy(interp->sp, args, argc * sizeof(word_t));
    interp->sp += fun->frame_size;
    interp->ip = fun->code;
}

static void execute_prim(struct interpreter *interp)
{
    word_t word = fetch(interp);
    prim_handlers[word](interp);
}

static void execute_jt(struct interpreter *interp)
{
    int64_t offset = fetch(interp);
    word_t word = pop(interp);
    if (word) {
        interp->ip = interp->ip + offset - 2;
    }
}

static void execute_jmp(struct interpreter *interp)
{
    int64_t offset = fetch(interp);
    interp->ip = interp->ip + offset - 2;
}

static void execute_ret(struct interpreter *interp)
{
    word_t result = pop(interp);
    pop_frame(interp);
    if (interp->ip == NULL) {
        interp->result = result;
        longjmp(interp->return_jump, 1);
    }
    push(result, interp);
}

typedef void (*instr_handler_t)(struct interpreter *interp);

static void execute(struct interpreter *interp) {
    while (true) {
        instr_handler_t handler = (instr_handler_t) fetch(interp);
        handler(interp);
    }
}

#define LIT ((word_t) execute_lit)
#define LOAD ((word_t) execute_load)
#define CALL ((word_t) execute_call)
#define PRIM ((word_t) execute_prim)
#define JT ((word_t) execute_jt)
#define JMP ((word_t) execute_jmp)
#define RET ((word_t) execute_ret)

static word_t fib_code[] = {
    /*  0 */  LOAD, 0, // arg
    /*  2 */  LIT, 0, // == 2
    /*  4 */  PRIM, 0, // less than
    /*  6 */  JT, 24, // JT 30 = 6 + 24
    /*  8 */  LOAD, 0, // arg
    /* 10 */  LIT, 1, // == 1
    /* 12 */  PRIM, 1, // subtract
    /* 14 */  CALL, 0, 1, // fib, 1 arg
    /* 17 */  LOAD, 0, // arg
    /* 19 */  LIT, 0, // == 2
    /* 21 */  PRIM, 1, // subtract
    /* 23 */  CALL, 0, 1, // fib, 1 arg
    /* 26 */  PRIM, 2, // add
    /* 28 */  JMP, 4, // JMP 32 = 28 + 4
    /* 30 */  LIT, 1, // == 1
    /* 32 */  RET
};

static struct function functions[] = {
    {
        .frame_size = 1,
        .code = fib_code
    }
};

#define STACK_SIZE 1024
static word_t stack[STACK_SIZE];

int main(int argc, const char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "A single numeric argument is required.\n");
        return 1;
    }
    int arg = atoi(argv[1]);
    printf("handlercode2\n");

    struct interpreter interp = {
        .bp = stack,
        .sp = stack,
        .ip = functions[0].code
    };
    push(0, &interp); // no prev. BP
    push(0, &interp); // no prev. IP
    push(0, &interp); // no prev. args
    push(arg, &interp); // call arg

    clock_t start;
    clock_t end;
    if (!setjmp(interp.return_jump)) {
      	start = clock();
	    execute(&interp);
    }
    // returning via a jump
	end = clock();
	long ms = (end - start) / (CLOCKS_PER_SEC / 1000);

    printf("Done in %ld ms; result = %lld\n", ms, interp.result);
}