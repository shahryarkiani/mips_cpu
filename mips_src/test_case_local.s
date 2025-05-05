    .set noat
    .text
    .align	2
    .globl	__start
    .ent	__start
    .type	__start, @function
  __start:
    addi $1, $0, 500 # 0
    add $2, $0, $0 # 4
  loop1:
    andi $3, $1, 1 # 8
    beq $3, $0, skipif # 12
    # 16
    addi $2, $2, 1 # 20
  skipif:
    addi $1, $1, -1 # 24
    bne $1, $0, loop1 # 28
    .end	__start
    .size	__start, .-__start
