/*
    Derived from directthreaded.c:

        - No 'interp' struct; all values are locals in the execute function

    Observations (Clang):

        - 2.3 times faster than directthreaded
        - 2.7 times faster than wordcode2

 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAYBE_UNUSED __attribute__((__unused__))

typedef uint64_t word_t;

struct function {
    size_t frame_size;
    void *code;
};

static word_t literals[] = {
    2,
    1
};

#define STACK_SIZE 1024
static word_t stack[STACK_SIZE];

MAYBE_UNUSED
static void print_stack(word_t *sp)
{
    printf("--- stack %p ---\n", sp);
    for (word_t *entry = stack; entry < sp; entry++) {
        printf("  %lld\n", *entry);
    }
    printf("------\n");
}

typedef void (*prim_handler_t)(word_t **spp);

static void lessThan(word_t **spp)
{
    int64_t rhs = *(--*spp);
    int64_t lhs = *(--*spp);
    bool result = lhs < rhs;
    #ifdef TRACE
        printf("%lld < %lld -> %s\n", lhs, rhs, result ? "true" : "false");
    #endif
    *((*spp)++) = result;
}

static void subtract(word_t **spp)
{
    int64_t rhs = *(--*spp);
    int64_t lhs = *(--*spp);
    int64_t result = lhs - rhs;
    #ifdef TRACE
        printf("%lld - %lld -> %lld\n", lhs, rhs, result);
    #endif
    *((*spp)++) = result;
}

static void add(word_t **spp)
{
    int64_t rhs = *(--*spp);
    int64_t lhs = *(--*spp);
    int64_t result = lhs + rhs;
    #ifdef TRACE
        printf("%lld + %lld -> %lld\n", lhs, rhs, result);
    #endif
    *((*spp)++) = result;
}

static prim_handler_t prim_handlers[] = {
    lessThan,
    subtract,
    add
};

#define GOTO_NEXT goto *((void*) *ip++)
#define PUSH(expr) *sp++ = expr
#define POP() *--sp
#define FETCH() *ip++

static word_t execute(word_t arg) {

    #define OP(c) ((word_t) &&c)
    static word_t fib_code[] = {
        /*  0 */  OP(LOAD), 0, // arg
        /*  2 */  OP(LIT), 0, // == 2
        /*  4 */  OP(PRIM), 0, // less than
        /*  6 */  OP(JT), 24, // JT 30 = 6 + 24
        /*  8 */  OP(LOAD), 0, // arg
        /* 10 */  OP(LIT), 1, // == 1
        /* 12 */  OP(PRIM), 1, // subtract
        /* 14 */  OP(CALL), 0, 1, // fib, 1 arg
        /* 17 */  OP(LOAD), 0, // arg
        /* 19 */  OP(LIT), 0, // == 2
        /* 21 */  OP(PRIM), 1, // subtract
        /* 23 */  OP(CALL), 0, 1, // fib, 1 arg
        /* 26 */  OP(PRIM), 2, // add
        /* 28 */  OP(JMP), 4, // JMP 32 = 28 + 4
        /* 30 */  OP(LIT), 1, // == 1
        /* 32 */  OP(RET)
    };

    static struct function functions[] = {
        {
            .frame_size = 1,
            .code = fib_code
        }
    };

    word_t *ip = (word_t *) fib_code;
    word_t *sp = stack;
    word_t *bp = stack;

    word_t word;
    word_t word2;
    word_t *words;
    word_t *words2;
    struct function *fun;
    int64_t offset;

    PUSH(0); // no prev. BP
    PUSH(0); // no prev. IP
    PUSH(0); // no args
    PUSH(arg);
    GOTO_NEXT;

LIT:
    word = FETCH();
    #ifdef TRACE
        printf("LIT %lld\n", word);
    #endif
    PUSH(literals[word]);
    GOTO_NEXT;

LOAD:
    word = FETCH();
    #ifdef TRACE
        printf("LOAD %lld\n", word);
    #endif
    PUSH(*(bp + 3 + word));
    GOTO_NEXT;

CALL:
    fun = functions + FETCH(); // function ID
    word = FETCH();
    #ifdef TRACE
        printf("CALL %lld\n", word);
    #endif
    words = sp - word;

    // push frame
    words2 = bp;
    bp = sp;
    PUSH((word_t) words2);
    PUSH((word_t) ip);
    PUSH(word); // args to pop later

    memcpy(sp, words, word * sizeof(word_t));
    sp += fun->frame_size;
    ip = fun->code;
    GOTO_NEXT;

PRIM:
    word = FETCH();
    #ifdef TRACE
        printf("PRIM %lld\n", word);
    #endif
    prim_handlers[word](&sp);
    GOTO_NEXT;

JT:
    offset = FETCH();
    word = POP();
    #ifdef TRACE
        printf("JT %lld (%lld)\n", offset, word);
    #endif
    if (word) {
        ip = ip + offset - 2;
    }
    GOTO_NEXT;

JMP:
    offset = FETCH();
    #ifdef TRACE
        printf("JMP %lld\n", offset);
    #endif
    ip = ip + offset - 2;
    GOTO_NEXT;

RET:
    word = POP();
    #ifdef TRACE
        printf("RET %lld\n", word);
    #endif

    // pop_frame
    sp = bp + 3;
    word2 = POP(); // args to pop
    ip = (word_t *) POP();
    bp = (word_t *) POP();
    sp -= word2;

    if (ip == NULL) return word;
    PUSH(word);
    GOTO_NEXT;
}

int main(int argc, const char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "A single numeric argument is required.\n");
        return 1;
    }
    int arg = atoi(argv[1]);
    printf("directthreaded2\n");

    clock_t start = clock();
	word_t result = execute(arg);
	clock_t end = clock();
	long ms = (end - start) / (CLOCKS_PER_SEC / 1000);

    printf("Done in %ld ms; result = %lld\n", ms, result);
}