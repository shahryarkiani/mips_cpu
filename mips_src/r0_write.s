  .set noat
	.text
	.align	2
	.globl	__start
	.ent	__start
	.type	__start, @function
__start:
   addi $1, $0, 15 # r1 = 15
   add  $0, $1, $0 # r0 = 15, but writes should be ignored
   add  $1, $0, $0 # r1 should not equal 30

	.end	__start
	.size	__start, .-__start
