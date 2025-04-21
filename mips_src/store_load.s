  .set noat
	.text
	.align	2
	.globl	__start
	.ent	__start
	.type	__start, @function
__start:
   addi $1, $0, 1 # r1 = 1 
   sw $1, 0($0) # mem[0] = 1
   lw $10, 0($0) # r10 = 1
   sw $10, 4($0) # mem[4] = 1
   lw $2, 4($0) # r2 = 1
	.end	__start
	.size	__start, .-__start
