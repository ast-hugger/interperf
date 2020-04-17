Lineage and performance difference compared to the ancestor:

    wordcode
    wordcode2               -5%
    wordcode3               -25%
    wordcode4               +25%
    handlercode             -5-10%
    handlercode2            +5%
    directthreaded          +15%
    directthreaded2         +2.3x
    directthreaded3         +11%
    directthreaded3const    +4%                    directthreaded3primtweak -8%
    comboinstructions       +17%
    comboinstructions2      +4% (+3.75x compared to wordcode2)

Key incremental changes:

  - wordcode2: more general; the ancestor only supports functions of arity 1.
  - wordcode3: instruction implementations in separate functions.
  - wordcode4: dispatch by opcode indexing instead of a switch
  - handlercode: code vector contains function pointers instead of opcodes.
  - handlercode2: no 'if' in the loop; 'ret' uses a long jump.
  - directthreaded: no more dispatch loop.
  - directthreaded2: interp state in locals (registers) instead of interp struct.
  - directthreaded3: call args accessed from the caller frame without copying into the callee frame.
  - directthreaded3const: instructions to push common constants such as 1 and 2.
  - directthreaded3primtweak: only one `pop` plus a stack top replacement in binary primitives.
  - comboinstructions: shortcut instructions for common operations such as `ADD1`.
  - comboinstructions2: introduced a `SUB2` instruction.
