  .set noat
	.text
	.align	2
	.globl	__start
	.ent	__start
	.type	__start, @function
__start:
   addi $1, $0, 77 # r1 = 77 
   sw $1, 0($0) # mem[0] = r1 == 77
   lw $10, 0($0) # r10 = mem[0] == 77
   nop
	.end	__start
	.size	__start, .-__start
