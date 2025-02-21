  .set noat
	.text
	.align	2
	.globl	__start
	.ent	__start
	.type	__start, @function
__start:
   	addi $1, $7, 37 # r1 = 15
	add  $2, $1, $1 # r0 = 15, but writes should be ignored
	add  $3, $1, $1
   	addi  $1, $0, 55 # r1 should not equal 30
   	nop
	nop
	nop
	nop
	nop
	nop
	.end	__start
	.size	__start, .-__start
