/*
    Same as wordcode2.c, except:

        - All instruction implementations are factored into individual functions,
          the dispatch switch calls the functions.

    Observations (Clang):

        - 20-25% slowdown compared to wordcode2
        - Explicit 'inline' on the instruction functions makes no difference
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAYBE_UNUSED __attribute__((__unused__))

// #define INLINE
#define INLINE inline

typedef uint64_t word_t;

enum opcode {
    LIT,    // 0
    LOAD,   // 1
    CALL,   // 2
    PRIM,   // 3
    JT,     // 4
    JMP,    // 5
    RET     // 6
};

static const char *opcode_names[] = {
    "LIT",
    "LOAD",
    "CALL",
    "PRIM",
    "JT",
    "JMP",
    "RET"
};

#define OPCODE_COUNT (sizeof(opcode_names) / sizeof(*opcode_names))

MAYBE_UNUSED
static const char *opcode_name(enum opcode opcode)
{
    return opcode < OPCODE_COUNT ? opcode_names[opcode] : "?";
}

struct function {
    size_t frame_size;
    const word_t *code;
};

struct interpreter {
    const word_t *ip;
    word_t *sp;
    word_t *bp;
};

typedef void (*prim_handler_t)(struct interpreter *interp);

// Fetch the next word from the code vector.
static INLINE word_t fetch(struct interpreter *interp)
{
    return *(interp->ip)++;
}

static INLINE void push(word_t value, struct interpreter *interp)
{
    *(interp->sp)++ = value;
}

static INLINE word_t pop(struct interpreter *interp)
{
    return *--(interp->sp);
}

// Return the local var at the specified index in the current frame
static INLINE word_t local(size_t index, struct interpreter *interp)
{
    return *(interp->bp + 2 + index);
}

static INLINE void push_frame(struct interpreter *interp)
{
    word_t *old_bp = interp->bp;
    interp->bp = interp->sp;
    push((word_t) old_bp, interp);
    push((word_t) interp->ip, interp);
}

static INLINE void pop_frame(struct interpreter *interp)
{
    interp->sp = interp->bp + 2;
    interp->ip = (word_t *) pop(interp);
    interp->bp = (word_t *) pop(interp);
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

static word_t execute(struct interpreter *interp);

static INLINE void execute_lit(struct interpreter *interp)
{
    word_t word = fetch(interp);
    push(literals[word], interp);
}

static INLINE void execute_load(struct interpreter *interp)
{
    word_t word = fetch(interp);
    push(local(word, interp), interp);
}

static INLINE void execute_call(struct interpreter *interp)
{
    struct function *fun = functions + fetch(interp); // function ID
    word_t argc = fetch(interp);
    word_t *args = interp->sp - argc;
    push_frame(interp);
    memcpy(interp->sp, args, argc * sizeof(word_t));
    interp->sp += fun->frame_size;
    interp->ip = fun->code;
    word_t result = execute(interp);
    pop_frame(interp);
    interp->sp -= argc;
    push(result, interp);
}

static INLINE void execute_prim(struct interpreter *interp)
{
    word_t word = fetch(interp);
    prim_handlers[word](interp);
}

static INLINE void execute_jt(struct interpreter *interp)
{
    int64_t offset = fetch(interp);
    word_t word = pop(interp);
    if (word) {
        interp->ip = interp->ip + offset - 2;
    }
}

static INLINE void execute_jmp(struct interpreter *interp)
{
    int64_t offset = fetch(interp);
    interp->ip = interp->ip + offset - 2;
}


static word_t execute(struct interpreter *interp) {
    while (true) {
        word_t opcode = fetch(interp);
        // printf("%s\n", opcode_name(opcode));
        switch (opcode) {
            case LIT:
                execute_lit(interp);
                break;
            case LOAD:
                execute_load(interp);
                break;
            case CALL:
                execute_call(interp);
                break;
            case PRIM:
                execute_prim(interp);
                break;
            case JT:
                execute_jt(interp);
                break;
            case JMP:
                execute_jmp(interp);
                break;
            case RET:
                return pop(interp);
            default:
                fprintf(stderr, "ERROR: Invalid opcode: %lld.\n", opcode);
                abort();
        }
    }
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
    printf("wordcode3\n");

    struct interpreter interp = {
        .bp = stack,
        .sp = stack,
        .ip = functions[0].code
    };
    push(0, &interp); // no prev. BP
    push(0, &interp); // no prev. IP
    push(arg, &interp); // call arg

  	clock_t start = clock();
	word_t result = execute(&interp);
	clock_t end = clock();
	long ms = (end - start) / (CLOCKS_PER_SEC / 1000);

    printf("Done in %ld ms; result = %lld\n", ms, result);
}