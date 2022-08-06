@hexenarm.s
@rww - custom arm routines for hexends

	#.section .rodata
	.section .itcm,"ax",%progbits

	.align	4
	.arm

.macro smuldef a
	.global	smulscale\a
	.type smulscale\a STT_FUNC
.endm


.macro smul a
.pool
smulscale\a:
	smull	r2, r3, r0, r1 
	@shift by FRACBITS 
	mov		r1, r2, lsr #\a 
	mov		r2, r3, lsl #(32-\a)
	orr		r0, r1, r2 
	bx		lr
.endm

@==========================================================================
@Math Functions
@==========================================================================
	.global	FixedMul
	.type FixedMul STT_FUNC
	.global	smulscale32
	.type smulscale32 STT_FUNC
	.global	smul_32_32_64
	.type smul_32_32_64 STT_FUNC
	smuldef 1
	smuldef 2
	smuldef 3
	smuldef 4
	smuldef 5
	smuldef 6
	smuldef 7
	smuldef 8
	smuldef 9
	smuldef 10
	smuldef 11
	smuldef 12
	smuldef 13
	smuldef 14
	smuldef 15
	smuldef 16
	smuldef 17
	smuldef 18
	smuldef 19
	smuldef 20
	smuldef 21
	smuldef 22
	smuldef 23
	smuldef 24
	smuldef 25
	smuldef 26
	smuldef 27
	smuldef 28
	smuldef 29
	smuldef 30
	smuldef 31

	smul 1
	smul 2
	smul 3
	smul 4
	smul 5
	smul 6
	smul 7
	smul 8
	smul 9
	smul 10
	smul 11
	smul 12
	smul 13
	smul 14
	smul 15
	smul 16
	smul 17
	smul 18
	smul 19
	smul 20
	smul 21
	smul 22
	smul 23
	smul 24
	smul 25
	smul 26
	smul 27
	smul 28
	smul 29
	smul 30
	smul 31

@=====================================
@FixedMul
@fast fixed multiply
@=====================================
FixedMul:
	smull	r2, r3, r0, r1
	
	@shift by FRACBITS
	mov		r1, r2, lsr #16
	mov		r2, r3, lsl #16
	orr		r0, r1, r2

	bx		lr

.pool
smulscale32:
	smull	r2, r3, r0, r1
	mov		r0, r3

	bx		lr

.pool
smul_32_32_64:
	smull	r2, r3, r0, r1
	mov		r0, r2
	mov		r1, r3

	bx		lr

	.align
	.end
