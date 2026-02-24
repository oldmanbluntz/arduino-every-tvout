/*
 Copyright (c) 2010 Myles Metzer

 Permission is hereby granted, free of charge, to any person
 obtaining a copy of this software and associated documentation
 files (the "Software"), to deal in the Software without
 restriction, including without limitation the rights to use,
 copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the
 Software is furnished to do so, subject to the following
 conditions:

 The above copyright notice and this permission notice shall be
 included in all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 OTHER DEALINGS IN THE SOFTWARE.
*/

#include "hardware_setup.h"

#ifndef ASM_MACROS_H
#define ASM_MACROS_H

#if defined(__AVR_ATmega4809__)
#define bst_hws "bst r16, 2 \n\t"
// --- Nano Every (ATmega4809) @ 20MHz Version ---

__asm__ __volatile__ (
	".macro delay1\n\tnop\n.endm\n"
	".macro delay2\n\tnop\n\tnop\n.endm\n"
	".macro delay3\n\tnop\n\tnop\n\tnop\n.endm\n"
	".macro delay4\n\tnop\n\tnop\n\tnop\n\tnop\n.endm\n"
	".macro delay5\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n.endm\n"
	
	// VOUT is used for 1-cycle Virtual Port access on 4809
	// svprt saves the port and clears the video bit (bit 0)
	".macro svprt p\n\tin r16,\\p\n\tandi r16, 0xFE\n.endm\n" 
	
	// o1bs loads the video bit from the T-flag and outputs via vout
	".macro o1bs p\n\tbld r16, 0\n\tvout \\p, r16\n.endm\n"

	// BST_HWS stores the Sync bit (D5 is Bit 2 on Port B) 
	// This ensures sync isn't lost during the pixel loop
	".macro bst_hws\n\tbst r16, 2\n.endm\n"
);

#else

// --- Classic Arduino (328P/2560/etc) Version ---

__asm__ __volatile__ (
	".macro delay1\n\tnop\n.endm\n"
	".macro delay2\n\tnop\n\tnop\n.endm\n"
	".macro delay3\n\tnop\n\tnop\n\tnop\n.endm\n"
	".macro delay4\n\tnop\n\tnop\n\tnop\n\tnop\n.endm\n"
	".macro delay5\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n.endm\n"
	".macro delay6\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n.endm\n"
	".macro delay7\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n.endm\n"
	".macro delay8\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n.endm\n"
	".macro delay9\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n.endm\n"
	".macro delay10\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n.endm\n"
);

__asm__ __volatile__ (
	".macro svprt p\n\tin r16,\\p\n\t" ANDI_HWS "\n.endm\n"
	".macro o1bs p\n\t" BLD_HWS "\nout \\p,r16\n.endm\n"
	".macro bst_hws\n\t" BST_HWS "\n.endm\n"
);

#endif

#endif

#endif


