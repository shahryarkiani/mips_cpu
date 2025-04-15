  .set noat
	.text
	.align	2
	.globl	__start
	.ent	__start
	.type	__start, @function
__start:
   	add $1, $0, 10
	add $2, $0, 20
	add $3, $0, 30
	add $4, $0, 40
	add $5, $0, 50
	add $6, $0, 60
	add $7, $0, 70
	add $8, $0, 80
	add $9, $0, 90
	add $10, $0, 100
	endlabel:
	.end	__start
	.size	__start, .-__start
