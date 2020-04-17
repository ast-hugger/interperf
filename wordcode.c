/*
    A basic stack machine. Features:

        - Code vector contains opcodes. (Essentially, bytecode but with word-size opcodes).
        - Dispatch using a switch.
        - Interpreter is a struct, any use is indirected through a pointer.
        - All functions have arity 1.

    Observations (compiled with Clang):

        - "inline" on push and other functions makes no performance difference

 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAYBE_UNUSED __attribute__((__unused__))

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
    return *(interp->bp + 2 + index);
}

static void push_frame(struct interpreter *interp)
{
    word_t *old_bp = interp->bp;
    interp->bp = interp->sp;
    push((word_t) old_bp, interp);
    push((word_t) interp->ip, interp);
}

static void pop_frame(struct interpreter *interp)
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
    /*  6 */  JT, 22, // 22 + 6 -> 28
    /*  8 */  LOAD, 0, // arg
    /* 10 */  LIT, 1, // == 1
    /* 12 */  PRIM, 1, // subtract
    /* 14 */  CALL, 0, // fib
    /* 16 */  LOAD, 0, // arg
    /* 18 */  LIT, 0, // == 2
    /* 20 */  PRIM, 1, // subtract
    /* 22 */  CALL, 0, // fib
    /* 24 */  PRIM, 2, // add
    /* 26 */  JMP, 4, // 26 + 4 -> 30
    /* 28 */  LIT, 1, // == 1
    /* 30 */  RET
};

static struct function functions[] = {
    {
        .frame_size = 1,
        .code = fib_code
    }
};

word_t execute(struct interpreter *interp) {
    word_t word;
    int64_t offset;
    struct function *fun;
    while (true) {
        word_t opcode = fetch(interp);
        // printf("%s\n", opcode_name(opcode));
        switch (opcode) {
            case LIT:
                word = fetch(interp);
                push(literals[word], interp);
                break;
            case LOAD:
                word = fetch(interp);
                push(local(word, interp), interp);
                break;
            case CALL:
                word = fetch(interp);
                fun = functions + word;
                word = pop(interp);
                push_frame(interp);
                push(word, interp);
                interp->ip = fun->code;
                break;
            case PRIM:
                word = fetch(interp);
                prim_handlers[word](interp);
                break;
            case JT:
                offset = fetch(interp);
                word = pop(interp);
                if (word) {
                    interp->ip = interp->ip + offset - 2;
                }
                break;
            case JMP:
                offset = fetch(interp);
                interp->ip = interp->ip + offset - 2;
                break;
            case RET:
                word = pop(interp);
                pop_frame(interp);
                if (interp->ip == 0) return word;
                push(word, interp);
                break;
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
    printf("wordcode\n");

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