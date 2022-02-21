/*********************************************************************
 *
 *                MIDI input for Fraise
 *
 *********************************************************************
 * Author               Date        Comment
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Antoine Rousseau  feb 2022     Original.
 ********************************************************************/

/*
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
# MA  02110-1301, USA.
*/
#include <core.h>
#include <midiin.h>

#ifndef MIDIIN_UART_PORT
#define MIDIIN_UART_PORT AUXSERIAL_NUM
#define MIDIIN_UART_RX AUXSERIAL_RX
#define MIDIIN_UART_TX AUXSERIAL_TX
#endif

//serial port:
#if MIDIIN_UART_PORT==1
#define SPBRGx 			SPBRG1
#define SPBRGHx 		SPBRGH1
#define BAUDCONx 		BAUDCON1
#define BAUDCONxbits 	BAUDCON1bits
#define RCREGx 			RCREG1
#define RCSTAx 			RCSTA1
#define RCSTAxbits 		RCSTA1bits
#define TXREGx 			TXREG1
#define TXSTAx 			TXSTA1
#define TXSTAxbits 		TXSTA1bits
#define RCxIF	 		PIR1bits.RC1IF
#define TXxIF	 		PIR1bits.TX1IF
#define RCxIE	 		PIE1bits.RC1IE
#define TXxIE	 		PIE1bits.TX1IE
#define RCxIP	 		IPR1bits.RC1IP
#define TXxIP	 		IPR1bits.TX1IP
#else
#define SPBRGx 			SPBRG2
#define SPBRGHx 		SPBRGH2
#define BAUDCONx 		BAUDCON2
#define BAUDCONxbits 	BAUDCON2bits
#define RCREGx 			RCREG2
#define RCSTAx 			RCSTA2
#define RCSTAxbits 		RCSTA2bits
#define TXREGx 			TXREG2
#define TXSTAx 			TXSTA2
#define TXSTAxbits 		TXSTA2bits
#define RCxIF	 		PIR3bits.RC2IF
#define TXxIF	 		PIR3bits.TX2IF
#define RCxIE	 		PIE3bits.RC2IE
#define TXxIE	 		PIE3bits.TX2IE
#define RCxIP	 		IPR3bits.RC2IP
#define TXxIP	 		IPR3bits.TX2IP
#endif
//(1 port equ)
#ifndef BAUDCON1
#define SPBRG1 			SPBRG
#define SPBRGH1 		SPBRGH
#define BAUDCON1 		BAUDCON
#define BAUDCON1bits 	BAUDCONbits
#define RCREG1 			RCREG
#define RCSTA1 			RCSTA
#define RCSTA1bits 		RCSTAbits
#define TXREG1 			TXREG
#define TXSTA1 			TXSTA
#define TXSTA1bits 		TXSTAbits
#define RC1IF 			RCIF
#define TX1IF 			TXIF
#define RC1IE 			RCIE
#define TX1IE 			TXIE
#define RC1IP 			RCIP
#define TX1IP 			TXIP
#endif

//static byte RXstate = 0;
static byte buffer[256];
static byte bufhead = 0;
static byte buftail = 0;

static byte message[16];
static byte msglen = 0;

byte errframe;
byte errover;
byte status = 15;
byte channel;

static void delay(word millisecs)
{
	t_delay del;
	delayStart(del, millisecs * 1000);
	while(!delayFinished(del)){}
}

static void setBaudRate(unsigned long int br)
{
//baud rate : br=FOSC/[4 (n+1)] : n=FOSC/(4*br)-1 : br=250kHz, n=FOSC/1000000 - 1
/* 
br(n+1)*4 = fosc  
4.br.n + 4br = f         
n = (f - 4br)/ 4br = f/4br - 1
*/
//#define BRGHL (FOSC/1000000 - 1)
	unsigned long int brg = FOSC/(4 * br) - 1;
	SPBRGHx=brg/256;
	SPBRGx=brg%256;
}
#define BRGHL(br) ((FOSC / (4UL * br)) - 1)
#define SET_BAUDRATE(br) SPBRGHx = BRGHL(br) / 256UL; SPBRGx = BRGHL(br) % 256UL;

void midiin_Init()
{
	digitalSet(MIDIIN_UART_TX);
	//pinModeDigitalOut(MIDIIN_UART_TX);
	pinModeDigitalIn(MIDIIN_UART_RX);

	/*TXSTAxbits.TXEN = 1;
	TXSTAxbits.TX9 = 0;
	TXxIE = 0;*/
	TXSTAxbits.BRGH = 1;

	RCSTAxbits.RX9 = 0;
	RCSTAxbits.CREN = 0;
	RCSTAxbits.CREN = 1;
	RCSTAxbits.ADDEN = 0;
	RCxIE = 1;//1;
	RCxIP = 0; // low interrupt priority
	RCSTAxbits.SPEN = 1;
	
	BAUDCONxbits.BRG16 = 1;
	BAUDCONxbits.DTRXP = 0;
	SET_BAUDRATE(31250);
}

void midiin_lowISR()
{
	if(!RCxIF) return;
			
	if(RCSTAxbits.OERR){ // overrun error
		WREG=RCREGx;
		__asm nop __endasm ;	
		WREG=RCREGx;
		RCSTAxbits.CREN=0;
		RCSTAxbits.CREN=1;
		errover++;
		return;
	}
	if(RCSTAxbits.FERR){ // framing error
		errframe++;
		return;
	}

	buffer[bufhead] = RCREGx;
	bufhead++;
}

void midiin_printerrs()
{
	if(errframe != 0 || errover != 0) {
		printf("Ce %d %d\n", errframe, errover);
		errframe = 0;
		errover = 0;
	}
}

void midiin_deInit()
{
	RCSTAxbits.CREN = 0;
	RCxIE = 0;
}

byte midiin_available()
{
	return(bufhead - buftail);
}

byte midiin_get()
{
	byte c;
	__critical {
		c = buffer[buftail];
	}
	buftail++;
	return c;
}

void midiin_service()
{
	byte b;
	if(midiin_available()) {
		b = midiin_get();
		if(b >= 128) { // status
			status = (b >> 4) & 7;
			channel = b & 15;
			msglen = 0;
		} else { // data
			if(status == 4 || status == 5) {
				midiin_message(status, channel, b, 0);
				status = 15;
				return;
			} else if(status < 7) {
				message[msglen++] = b;
				if(msglen == 2) {
					midiin_message(status, channel, message[0], message[1]);
					status = 15;
				}
			}
		}
	}
}

