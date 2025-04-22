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
   nop
   
   sw $10, 4($0) # mem[4] = 1
   lw $2, 4($0) # r2 = mem[4] == 1
   
   add $1, $1, $1 # r1 = 2
   sw $2, 8($0) # mem[8] = 1
   
   lw $3, 8($0) # r3 = 1
   add $1, $1, $1 # r1 = 4
   sw $1, 0($0) # mem[0] = 4
   lw $4, 0($0) # r4 = 4
   add $1, $1, $1 # r1 = 8
   add $2, $2, $2 # r2 = 2
   sw $1, 4($0) # mem[4] = 8
   lw $5, 4($0) # r5 = 8
	.end	__start # r1 = 8, r2 = 2, r3 = 1, r4 = 4, r5 = 8, r10 = 1
	.size	__start, .-__start
