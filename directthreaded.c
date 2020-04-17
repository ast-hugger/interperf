/*
    Derived from handlercode2.c:

        - Uses labels-as-values for a proper direct threaded implementation
        - No need for longjmp now

    Observations (Clang):

        - ~ 15% performance improvement

 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef uint64_t word_t;

struct function {
    size_t frame_size;
    void *code;
};

struct interpreter {
    void **ip;
    word_t *sp;
    word_t *bp;
};

typedef void (*prim_handler_t)(struct interpreter *interp);

// Fetch the next word from the code vector.
static word_t fetch(struct interpreter *interp)
{
    word_t *words = (word_t *) interp->ip;
    word_t result = *words++;
    interp->ip = (void**) words;
    return result;
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
    interp->ip = (void**) pop(interp);
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

#define GOTO_NEXT goto **(interp->ip)++
#define WORD(x) ((void*) x)

static word_t execute(struct interpreter *interp) {
    static void* fib_code[] = {
        /*  0 */  &&LOAD, WORD(0), // arg
        /*  2 */  &&LIT, WORD(0), // == 2
        /*  4 */  &&PRIM, WORD(0), // less than
        /*  6 */  &&JT, WORD(24), // JT 30 = 6 + 24
        /*  8 */  &&LOAD, WORD(0), // arg
        /* 10 */  &&LIT, WORD(1), // == 1
        /* 12 */  &&PRIM, WORD(1), // subtract
        /* 14 */  &&CALL, WORD(0), WORD(1), // fib, 1 arg
        /* 17 */  &&LOAD, WORD(0), // arg
        /* 19 */  &&LIT, WORD(0), // == 2
        /* 21 */  &&PRIM, WORD(1), // subtract
        /* 23 */  &&CALL, WORD(0), WORD(1), // fib, 1 arg
        /* 26 */  &&PRIM, WORD(2), // add
        /* 28 */  &&JMP, WORD(4), // JMP 32 = 28 + 4
        /* 30 */  &&LIT, WORD(1), // == 1
        /* 32 */  &&RET
    };

    static struct function functions[] = {
        {
            .frame_size = 1,
            .code = fib_code
        }
    };

    interp->ip = fib_code;
    GOTO_NEXT;

    word_t word;
    word_t *words;
    struct function *fun;
    int64_t offset;

LIT:
    word = fetch(interp);
    push(literals[word], interp);
    GOTO_NEXT;

LOAD:
    word = fetch(interp);
    push(local(word, interp), interp);
    GOTO_NEXT;

CALL:
    fun = functions + fetch(interp); // function ID
    word = fetch(interp);
    words = interp->sp - word;
    push_frame(interp, word);
    memcpy(interp->sp, words, word * sizeof(word_t));
    interp->sp += fun->frame_size;
    interp->ip = fun->code;
    GOTO_NEXT;

PRIM:
    word = fetch(interp);
    prim_handlers[word](interp);
    GOTO_NEXT;

JT:
    offset = fetch(interp);
    word = pop(interp);
    if (word) {
        interp->ip = interp->ip + offset - 2;
    }
    GOTO_NEXT;

JMP:
    offset = fetch(interp);
    interp->ip = interp->ip + offset - 2;
    GOTO_NEXT;

RET:
    word = pop(interp);
    pop_frame(interp);
    if (interp->ip == NULL) {
        return word;
    }
    push(word, interp);
    GOTO_NEXT;
}

#define STACK_SIZE 1024
static word_t stack[STACK_SIZE];

int main(int argc, const char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "A single numeric argument is required.\n");
        return 1;
    }
    int arg = atoi(argv[1]);
    printf("directthreaded\n");

    struct interpreter interp = {
        .bp = stack,
        .sp = stack,
    };
    push(0, &interp); // no prev. BP
    push(0, &interp); // no prev. IP
    push(0, &interp); // no prev. args
    push(arg, &interp); // call arg

    clock_t start = clock();
	word_t result = execute(&interp);
	clock_t end = clock();
	long ms = (end - start) / (CLOCKS_PER_SEC / 1000);

    printf("Done in %ld ms; result = %lld\n", ms, result);
}