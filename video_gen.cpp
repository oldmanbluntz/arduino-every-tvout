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

#include <avr/interrupt.h>
#include <avr/io.h>

#include "video_gen.h"
#include "spec/video_properties.h"
#include "spec/asm_macros.h"
#include "spec/hardware_setup.h"

int renderLine;
TVout_vid display;
void (*render_line)();
void (*line_handler)();
void (*hbi_hook)() = &empty;
void (*vbi_hook)() = &empty;

volatile long remainingToneVsyncs;

void empty() {}

void render_setup(uint8_t mode, uint8_t x, uint8_t y, uint8_t *scrnptr) {

	display.screen = scrnptr;
	display.hres = x;
	display.vres = y;
	display.frames = 0;
	
	if (mode)
		display.vscale_const = _PAL_LINE_DISPLAY/display.vres - 1;
	else
		display.vscale_const = _NTSC_LINE_DISPLAY/display.vres - 1;
	display.vscale = display.vscale_const;
	
	unsigned char rmethod = (_TIME_ACTIVE*_CYCLES_PER_US)/(display.hres*8);
	switch(rmethod) {
		case 6: render_line = &render_line6c; break;
		case 5: render_line = &render_line5c; break;
		case 4: render_line = &render_line4c; break;
		case 3: render_line = &render_line3c; break;
		default:
			if (rmethod > 6) render_line = &render_line6c;
			else render_line = &render_line3c;
	}

	DDR_VID |= _BV(VID_PIN);
	DDR_SYNC |= _BV(SYNC_PIN);
	PORT_VID &= ~_BV(VID_PIN);
	PORT_SYNC |= _BV(SYNC_PIN);
	DDR_SND |= _BV(SND_PIN);
	
#if defined(__AVR_ATmega4809__)
    TCB0.CTRLB = TCB_CNTMODE_INT_gc; 
    TCB0.CCMP = (mode == 0) ? 1270 : 1280; // 63.5us NTSC / 64us PAL @ 20MHz
    TCB0.INTCTRL = TCB_CAPT_bm; 
    TCB0.CTRLA = TCB_ENABLE_bm; 
#else
	TCCR1A = _BV(COM1A1) | _BV(COM1A0) | _BV(WGM11);
	TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS10);
#endif
	
	if (mode) {
		display.start_render = _PAL_LINE_MID - ((display.vres * (display.vscale_const+1))/2);
		display.output_delay = _PAL_CYCLES_OUTPUT_START;
		display.vsync_end = _PAL_LINE_STOP_VSYNC;
		display.lines_frame = _PAL_LINE_FRAME;
#if defined(__AVR_ATmega4809__)
        TCB0.CCMP = 1280; 
#else
		ICR1 = _PAL_CYCLES_SCANLINE;
		OCR1A = _CYCLES_HORZ_SYNC;
#endif
	}
	else {
		display.start_render = _NTSC_LINE_MID - ((display.vres * (display.vscale_const+1))/2) + 8;
		display.output_delay = _NTSC_CYCLES_OUTPUT_START;
		display.vsync_end = _NTSC_LINE_STOP_VSYNC;
		display.lines_frame = _NTSC_LINE_FRAME;
#if defined(__AVR_ATmega4809__)
        TCB0.CCMP = 1270; 
#else
		ICR1 = _NTSC_CYCLES_SCANLINE;
		OCR1A = _CYCLES_HORZ_SYNC;
#endif
	}
	display.scanLine = display.lines_frame+1;
	line_handler = &vsync_line;
#if !defined(__AVR_ATmega4809__)
	TIMSK1 = _BV(TOIE1);
#endif
	sei();
}

#if defined(__AVR_ATmega4809__)
ISR(TCB0_INT_vect) {
    TCB0.INTFLAGS = TCB_CAPT_bm; 
    hbi_hook();
    line_handler();
}
#else
ISR(TIMER1_OVF_vect) {
	hbi_hook();
	line_handler();
}
#endif

void blank_line() {
	if ( display.scanLine == display.start_render) {
		renderLine = 0;
		display.vscale = display.vscale_const;
		line_handler = &active_line;
	}
	else if (display.scanLine == display.lines_frame) {
		line_handler = &vsync_line;
		vbi_hook();
	}
	display.scanLine++;
}

void active_line() {
	wait_until(display.output_delay);
	render_line();
	if (!display.vscale) {
		display.vscale = display.vscale_const;
		renderLine += display.hres;
	}
	else
		display.vscale--;
		
	if ((display.scanLine + 1) == (int)(display.start_render + (display.vres*(display.vscale_const+1))))
		line_handler = &blank_line;
		
	display.scanLine++;
}

void vsync_line() {
	if (display.scanLine >= display.lines_frame) {
#if !defined(__AVR_ATmega4809__)
		OCR1A = _CYCLES_VIRT_SYNC;
#endif
		display.scanLine = 0;
		display.frames++;
		
		if (remainingToneVsyncs != 0) {
			if (remainingToneVsyncs > 0) remainingToneVsyncs--;
		} else {
#if defined(__AVR_ATmega4809__)
			TCB1.CTRLA = 0;             
			VPORTE.OUT &= ~PIN3_bm;    
#else
			TCCR2B = 0;                
 			PORT_SND &= ~(_BV(SND_PIN));
#endif
		}
	}
	else if (display.scanLine == display.vsync_end) {
#if !defined(__AVR_ATmega4809__)
		OCR1A = _CYCLES_HORZ_SYNC;
#endif
		line_handler = &blank_line;
	}
	display.scanLine++;
}

static void inline wait_until(uint8_t time) {
	__asm__ __volatile__ (
			"subi	%[time], 10\n"
			"sub	%[time], %[tcnt1l]\n\t"
		"100:\n\t"
			"subi	%[time], 3\n\t"
			"brcc	100b\n\t"
			"subi	%[time], 0-3\n\t"
			"breq	101f\n\t"
			"dec	%[time]\n\t"
			"breq	102f\n\t"
			"rjmp	102f\n"
		"101:\n\t"
			"nop\n" 
		"102:\n"
		:
		: [time] "a" (time),
#if defined(__AVR_ATmega4809__)
		[tcnt1l] "a" (TCB0.CNTL)
#else
		[tcnt1l] "a" (TCNT1L)
#endif
	);
}

void render_line6c() {
	__asm__ __volatile__ (
		"ADD	r26,r28\n\t"
		"ADC	r27,r29\n\t"
		"svprt	%[port]\n\t"
		"rjmp	enter6\n"
	"loop6:\n\t"
		"bst	__tmp_reg__,0\n\t"
		"o1bs	%[port]\n"
	"enter6:\n\t"
		"LD		__tmp_reg__,X+\n\t"
		"delay1\n\t"
		"bst	__tmp_reg__,7\n\t"
		"o1bs	%[port]\n\t"
#if defined(__AVR_ATmega4809__)
		"delay4\n\t"
#else
		"delay3\n\t"
#endif
		"bst	__tmp_reg__,6\n\t"
		"o1bs	%[port]\n\t"
#if defined(__AVR_ATmega4809__)
		"delay4\n\t"
#else
		"delay3\n\t"
#endif
		"bst	__tmp_reg__,5\n\t"
		"o1bs	%[port]\n\t"
#if defined(__AVR_ATmega4809__)
		"delay4\n\t"
#else
		"delay3\n\t"
#endif
		"bst	__tmp_reg__,4\n\t"
		"o1bs	%[port]\n\t"
#if defined(__AVR_ATmega4809__)
		"delay4\n\t"
#else
		"delay3\n\t"
#endif
		"bst	__tmp_reg__,3\n\t"
		"o1bs	%[port]\n\t"
#if defined(__AVR_ATmega4809__)
		"delay4\n\t"
#else
		"delay3\n\t"
#endif
		"bst	__tmp_reg__,2\n\t"
		"o1bs	%[port]\n\t"
#if defined(__AVR_ATmega4809__)
		"delay4\n\t"
#else
		"delay3\n\t"
#endif
		"bst	__tmp_reg__,1\n\t"
		"o1bs	%[port]\n\t"
		"dec	%[hres]\n\t"
		"brne	loop6\n\t"
		"delay2\n\t"
		"bst	__tmp_reg__,0\n\t"
		"o1bs	%[port]\n"
		"svprt	%[port]\n\t"
		"bst_hws\n\t" 
		"o1bs	%[port]\n\t"
		:
		: [port] "i" (_SFR_IO_ADDR(PORT_VID)),
		"x" (display.screen),
		"y" (renderLine),
		[hres] "d" (display.hres)
		: "r16"
	);
}

void render_line5c() {
	__asm__ __volatile__ (
		"ADD	r26,r28\n\t"
		"ADC	r27,r29\n\t"
		"svprt	%[port]\n\t"
		"rjmp	enter5\n"
	"loop5:\n\t"
		"bst	__tmp_reg__,0\n\t"
		"o1bs	%[port]\n"
	"enter5:\n\t"
		"LD		__tmp_reg__,X+\n\t"
		"bst	__tmp_reg__,7\n\t"
		"o1bs	%[port]\n\t"
#if defined(__AVR_ATmega4809__)
		"delay3\n\t" 
#else
		"delay2\n\t"
#endif
		"bst	__tmp_reg__,6\n\t"
		"o1bs	%[port]\n\t"
#if defined(__AVR_ATmega4809__)
		"delay3\n\t"
#else
		"delay2\n\t"
#endif
		"bst	__tmp_reg__,5\n\t"
		"o1bs	%[port]\n\t"
#if defined(__AVR_ATmega4809__)
		"delay3\n\t"
#else
		"delay2\n\t"
#endif
		"bst	__tmp_reg__,4\n\t"
		"o1bs	%[port]\n\t"
#if defined(__AVR_ATmega4809__)
		"delay3\n\t"
#else
		"delay2\n\t"
#endif
		"bst	__tmp_reg__,3\n\t"
		"o1bs	%[port]\n\t"
#if defined(__AVR_ATmega4809__)
		"delay3\n\t"
#else
		"delay2\n\t"
#endif
		"bst	__tmp_reg__,2\n\t"
		"o1bs	%[port]\n\t"
#if defined(__AVR_ATmega4809__)
		"delay2\n\t" 
#else
		"delay1\n\t"
#endif
		"dec	%[hres]\n\t"
		"bst	__tmp_reg__,1\n\t"
		"o1bs	%[port]\n\t"
		"brne	loop5\n\t"
#if defined(__AVR_ATmega4809__)
		"delay2\n\t"
#else
		"delay1\n\t"
#endif
		"bst	__tmp_reg__,0\n\t"
		"o1bs	%[port]\n"
		"svprt	%[port]\n\t"
		"bst_hws\n\t"
		"o1bs	%[port]\n\t"
		:
		: [port] "i" (_SFR_IO_ADDR(PORT_VID)),
		"x" (display.screen),
		"y" (renderLine),
		[hres] "d" (display.hres)
		: "r16"
	);
}

void render_line4c() {
	__asm__ __volatile__ (
		"ADD	r26,r28\n\t"
		"ADC	r27,r29\n\t"
		"rjmp	enter4\n"
	"loop4:\n\t"
		"lsl	__tmp_reg__\n\t"
#if defined(__AVR_ATmega4809__)
		"vout	%[port],__tmp_reg__\n\t"
#else
		"out	%[port],__tmp_reg__\n\t"
#endif
	"enter4:\n\t"
		"LD		__tmp_reg__,X+\n\t"
#if defined(__AVR_ATmega4809__)
		"delay2\n\t"
		"vout	%[port],__tmp_reg__\n\t"
		"delay3\n\t"
#else
		"delay1\n\t"
		"out	%[port],__tmp_reg__\n\t"
		"delay2\n\t"
#endif
		"lsl	__tmp_reg__\n\t"
#if defined(__AVR_ATmega4809__)
		"vout	%[port],__tmp_reg__\n\t"
		"delay3\n\t"
#else
		"out	%[port],__tmp_reg__\n\t"
		"delay2\n\t"
#endif
		"lsl	__tmp_reg__\n\t"
#if defined(__AVR_ATmega4809__)
		"vout	%[port],__tmp_reg__\n\t"
		"delay3\n\t"
#else
		"out	%[port],__tmp_reg__\n\t"
		"delay2\n\t"
#endif
		"lsl	__tmp_reg__\n\t"
#if defined(__AVR_ATmega4809__)
		"vout	%[port],__tmp_reg__\n\t"
		"delay3\n\t"
#else
		"out	%[port],__tmp_reg__\n\t"
		"delay2\n\t"
#endif
		"lsl	__tmp_reg__\n\t"
#if defined(__AVR_ATmega4809__)
		"vout	%[port],__tmp_reg__\n\t"
		"delay3\n\t"
#else
		"out	%[port],__tmp_reg__\n\t"
		"delay2\n\t"
#endif
		"lsl	__tmp_reg__\n\t"
#if defined(__AVR_ATmega4809__)
		"vout	%[port],__tmp_reg__\n\t"
		"delay2\n\t"
#else
		"out	%[port],__tmp_reg__\n\t"
		"delay1\n\t"
#endif
		"lsl	__tmp_reg__\n\t"
		"dec	%[hres]\n\t"
#if defined(__AVR_ATmega4809__)
		"vout	%[port],__tmp_reg__\n\t"
#else
		"out	%[port],__tmp_reg__\n\t"
#endif
		"brne	loop4\n\t"
		"delay1\n\t"
		"lsl	__tmp_reg__\n\t"
#if defined(__AVR_ATmega4809__)
		"vout	%[port],__tmp_reg__\n\t"
		"delay3\n\t"
		"ldi	__tmp_reg__,0\n\t"
		"vout	%[port],__tmp_reg__\n\t"
#else
		"out	%[port],__tmp_reg__\n\t"
		"delay1\n\t"
		"ldi	__tmp_reg__,0\n\t"
		"out	%[port],__tmp_reg__\n\t"
#endif
		:
		: [port] "i" (_SFR_IO_ADDR(PORT_VID)),
		"x" (display.screen),
		"y" (renderLine),
		[hres] "d" (display.hres)
		: "r16"
	);
}

void render_line3c() {
	// render_line3c is rarely used at these resolutions but 
	// would require significant rework for 20MHz/4809.
	// For now, it remains original.
}


