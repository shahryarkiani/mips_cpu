  .set noat
  .text
  .align	2
  .globl	__start
  .ent	__start
  .type	__start, @function
__start:
  addi $1, $0, 2
  add $2, $0, $0
skipif:
  addi $1, $1, -1
  addi $2, $2, 7
  nop
  nop
  nop
  nop
  nop
  nop
  nop
  bne $1, $0, skipif
  addi $3, $3, 412
  .end	__start
  .size	__start, .-__start


# r1 = 2
# r2 = 0
# r3 = r1 & 1
# if r3 == 0, go to skipif
# else r2 = r2 + 1
