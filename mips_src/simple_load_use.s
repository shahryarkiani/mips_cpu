  .set noat
	.text
	.align	2
	.globl	__start
	.ent	__start
	.type	__start, @function
__start:
	add $5, $0, 123
    nop
    nop
    nop
    nop
    nop
	nop
   	lw $5, 300($0)
	add $2, $2, $5
	endlabel:
	.end	__start
	.size	__start, .-__start
