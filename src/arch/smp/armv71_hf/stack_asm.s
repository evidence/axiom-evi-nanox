.text
.globl switchStacks
.globl startHelper
/* void switchStacks (arg0, arg1, new, helper) */
.extern abort

/*
 * The extension registers must be saved in addition to the general purpose registers.
 * See sections 5.1.1 and 5.1.2.1 of Procedure Call Standard for the ARM Architecture
 * http://infocenter.arm.com/help/topic/com.arm.doc.ihi0042f/IHI0042F_aapcs.pdf
 */
switchStacks:

	/* Saves general purpose registers,
           R12 is included to make them even number */
	push {r4-r12,lr}

	/* Saves VFP and NEON registers */
	vpush {s16-s31}

	/* switch current sp to new */
	mov r4, r2
	mov r2, sp
	mov sp, r4

	/* arguments in r0=arg1, r1=arg2, r2=old sp */
	blx r3

	/* Restores VFP and NEON registers */
	vpop {s16-s31}

	/* Restores general purpose registers*/
	pop {r4-r12,pc}

startHelper:
	pop {r0,r4}  /*User Args (R0) to Call user Function (R4)*/
	blx r4       /*Jumps to user function*/
	pop {r0,r4}  /*Cleanup Args (R0) to Call cleanup function(R4)*/
	blx r4       /*Jumps to cleanup*/
	b abort      /*Aborts in case of cleanup returns*/
