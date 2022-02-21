/*********************************************************************
 *               analog example for Versa1.0
 *	Analog capture on connectors K1, K2, K3 and K5. 
 *********************************************************************/

#define BOARD 8X2A

#include <fruit.h>
#include <midiin.h>
#include <eeparams.h>
#include "../widthTable.c"
#include "../freqTable.c"

// Timer macros
#define TIMER 3
#include <timer.h>

//  val = /*0xFFFF - */pulseWidth;
#define SET_TIMER(us) do {\
	val = 0xFFFF - us;\
	TIMER_H = val>>8;\
	TIMER_L = val&255;\
	TIMER_IF = 0;\
	TIMER_ON = 1; } while(0)

#define TIMER_INIT() do{\
	TIMER_CON = 0; \
	TIMER_PS1 = 0;  /* 	no prescaler */\
	TIMER_16BIT = 1;/* 	16bits                          */\
	TIMER_IP = 1;	/* 	high/low priority interrupt */\
	TIMER_ON = 0;	/* 	stop timer                      */\
	TIMER_IF = 0;   /*  clear flag                      */\
	TIMER_IE = 1;	/* 	enable timer interrupt         */\
} while(0)


t_delay mainDelay;
volatile char pulsePhase = 0;
char pulseChan;
unsigned int pulseWidth = 1000;
unsigned int val;
unsigned int pWidth[4];

unsigned char freq[4];
unsigned int width[4];
t_delay lastPulse[4];
t_delay lastStart[4];
unsigned char widthCtl[4];
unsigned char masterWidthCtl;

unsigned char midiAddress = 1;

#define ON_MAXTIME 2000000UL

#define PHASE0(chan) do{ digitalClear(M##chan##1); digitalClear(M##chan##2); } while(0)
#define PHASE1(chan) do{ digitalSet(M##chan##1); digitalClear(M##chan##2); } while(0)
#define PHASE2(chan) do{ digitalClear(M##chan##1); digitalSet(M##chan##2); } while(0)
#define PHASE_(chan, phase) PHASE##phase(chan)

#define PHASE(numchan, phase) do { \
	if(numchan == 0) PHASE_(A,phase); \
	else if(numchan == 1) PHASE_(B,phase); \
	else if(numchan == 2) PHASE_(C,phase); \
	else if(numchan == 3) PHASE_(D,phase);} while(0)

void pulseFire()
{
	pulsePhase = 2;
	__critical {
		PHASE(pulseChan, 1);
		SET_TIMER(pulseWidth);
	}
}

void pulseService()
{
	static char chan = 0;
	unsigned int final_width;
	
	if(pulsePhase == 0) { // if last pulse finished
		if(chan > 3) {
			digitalSet(SYNCOUT);
			if(chan > 5) chan = 0; // give the other one some time to catch its turn;
			else chan++;
			//chan = 0;
			return;
		}

		if(chan == 0) { // catch the turn
			if(! digitalRead(SYNCIN)) return;
			digitalClear(SYNCOUT);
			if(! digitalRead(SYNCIN)) { // too late... the other one has taken its turn
				digitalSet(SYNCOUT);
				return;
			}
		} //else digitalSet(SYNCOUT);

		if(width[chan]) {
			if(delayFinished(lastStart[chan])) {
				width[chan] = 0;
			} else if(delayFinished(lastPulse[chan])) {
				delayStart(lastPulse[chan], /*1000000UL/freq[chan]*/freqTable[freq[chan]]);
				if(widthCtl[chan] == 0) final_width = widthTable[width[chan]];
				else final_width = widthTable[widthCtl[chan]];
				if(masterWidthCtl != 0) final_width = (unsigned int)(((unsigned long)final_width * (masterWidthCtl + 1)) / 128);
				pWidth[chan] = final_width;
			}
		}
		if(pWidth[chan]) {
			pulseWidth = pWidth[chan];
			pulseChan = chan;
			pulseFire();
			pWidth[chan] = 0;
		}
		chan++;
		//if(chan > 3) chan = 0;
	}
}

// ----------- Interrupts --------------
 
void highInterrupts(void)
{
	if(TIMER_IF == 0) return;

	TIMER_ON = 0;
	TIMER_IF = 0;

	if(pulsePhase == 1) {
		PHASE(pulseChan, 0);
		SET_TIMER(pulseWidth);
		pulsePhase = 2;
		return;
	} else if(pulsePhase == 2) {
		PHASE(pulseChan, 2);
		SET_TIMER(pulseWidth);
		pulsePhase = 3;
		return;
	} else if(pulsePhase == 3) {
		PHASE(pulseChan, 0);
		SET_TIMER(pulseWidth);
		pulsePhase = 4;
		return;
	} else if(pulsePhase == 4) {
		pulsePhase = 0;
		return;
	}
}

void lowInterrupts(void)
{
	midiin_lowISR();
}

//----------- Setup ----------------
#define SETUP_BRIDGE(X) do {\
	digitalClear(M##X##1);\
	digitalClear(M##X##2);\
	digitalSet(M##X##EN);\
	pinModeDigitalOut(M##X##1);\
	pinModeDigitalOut(M##X##2);\
	pinModeDigitalOut(M##X##EN); } while(0)

void setup(void) {	
	fruitInit();
	
	widthCtl[0] = widthCtl[1] = widthCtl[2] = widthCtl[3] = 0;
	masterWidthCtl = 0;

	SETUP_BRIDGE(A);
	SETUP_BRIDGE(B);
	SETUP_BRIDGE(C);
	SETUP_BRIDGE(D);

	pulsePhase = 0;
	TIMER_INIT();
	pinModeDigitalIn(SYNCIN);
	pinModeDigitalOut(SYNCOUT);
	digitalSet(SYNCOUT);
	
	midiin_Init();

	EEreadMain();

	delayStart(mainDelay, 5000); 	// init the mainDelay to 5 ms
}

// ---------- Main loop ------------

void loop() {
	fraiseService();	// listen to Fraise events
	pulseService();
	midiin_service();

	if(delayFinished(mainDelay)) // when mainDelay triggers :
	{
		delayStart(mainDelay, 5000); 	// re-init mainDelay
	}
}


// ---------- Receiving ------------

void fraiseReceiveChar() // receive text
{
	unsigned char c;
	
	c=fraiseGetChar();
	if(c=='L'){		//switch LED on/off 
		c=fraiseGetChar();
		//digitalWrite(LED, c!='0');		
	}
	else if(c=='E') { 	// echo text (send it back to host)
		printf("C");
		c = fraiseGetLen(); 			// get length of current packet
		while(c--) printf("%c",fraiseGetChar());// send each received byte
		putchar('\n');				// end of line
	}	
}

#define PINSET(K) do{ pinModeDigitalOut(K); if(c2) digitalClear(K); else digitalSet(K); } while(0)

void fraiseReceive() // receive raw
{
	unsigned char c,c2;
	unsigned int i;
	c=fraiseGetChar();
	
	switch(c) {
		PARAM_INT(10, i); c2 = fraiseGetChar() ; if(c2 < 4) pWidth[c2] = i; break;
		PARAM_CHAR(20, c); // 20 chan width freq
			if(c > 4) break;
			width[c] = fraiseGetChar(); if(width[c] > 127) width[c] = 127;
			freq[c] = fraiseGetChar();
			delayStart(lastStart[c], ON_MAXTIME);
			delayStart(lastPulse[c], 0);
			//printf("CN c=%d w=%d f=%d\n", c, width[c], freq[c]);
			break;
		PARAM_CHAR(100, midiAddress); EEwriteMain(); break;
		case 101: printf("CA %d\n", midiAddress); break;
	}
}

// ----------- MIDI input -------------

void midiin_message(byte status, byte channel, byte d1, byte d2)
{
#if 0 
	static byte buf[19] = {'B', 200, 0};
	buf[2] = status;
	buf[3] = channel;
	buf[4] = d1;
	buf[5] = d2;
	buf[6] = '\n';
	fraiseSend(buf, 7);
#endif
	if(channel != 8) return;
	if(status == 0 || status == 1) { /*note off/on, d1=note d2=velo */
		if(d1 < midiAddress || d1 > (midiAddress + 3)) return; /* not our notes... */
		d1 -= midiAddress;
		if(status == 0) d2 = 0; /* if note_off then velocity=0 */
		//if(d2 != 0 && widthCtl[d1] != 0) d2 = widthCtl[d1];
		//printf("Cchan %d velo %d\n", d1, d2);
		width[d1] = d2;
		delayStart(lastStart[d1], ON_MAXTIME);
		delayStart(lastPulse[d1], 0);
	} else if(status == 3) { /* control change, d1=ctlnum d2=value */
		if(d1 == 20) { /* masterWidth */
			masterWidthCtl = d2;
		} else if(d1 >= midiAddress && d1 < (midiAddress + 4)) { /* freq */
			d1 -= midiAddress;
			if(d2 == 0) d2 = 1; /* 0Hz forbidden */
			freq[d1] = d2;
		} else if(d1 >= (midiAddress + 10 ) && d1 < (midiAddress + 14)) { /* width */
			d1 -= (midiAddress + 10);
			widthCtl[d1] = d2;
		}
	}
}

// ---------- EEPROM -----------
void EEdeclareMain()
{
	EEdeclareChar(&midiAddress);
}
