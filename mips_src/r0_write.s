  .set noat
	.text
	.align	2
	.globl	__start
	.ent	__start
	.type	__start, @function
__start:
   	add $1, $0, 25
	add $2, $1, 55
	add $3, $1, 25
	endlabel:
	.end	__start
	.size	__start, .-__start
