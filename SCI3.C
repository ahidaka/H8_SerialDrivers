/*
 * sci3.c
 */
#include <stdio.h>
#include <3048f.h>
#include <ctype.h>	/* islower() and toupper() */
#include "akih8io.h"

#define	B2400	207
#define	B4800	103
#define	B9600	51
#define	B19200	25
#define	B31250	15
#define	B38400	12
#define MAX_SCI 0

/* SCR SCI Control Register */
#define SCR_CKE0 0x01	/* Clock Enable-0 */
#define SCR_CKE1 0x02	/* Clock Enable-1 */
#define SCR_TEIE 0x04	/* Tx-End Interrupt Enable */
#define	SCR_MPIE 0x08	/* Multi Processor Interrupt */
#define	SCR_RE	0x10	/* Rx Enable */
#define	SCR_TE	0x20	/* Tx Enable */
#define	SCR_RIE 0x40	/* Rx Interrupt Enable */
#define	SCR_TIE	0x80	/* Tx Interrupt Enable */

/* SSR - SCI Status Register */
#define	SSR_MPBT 0x01	/* Multi Processor Bit Transfer */
#define SSR_MPB	0x02	/* Multi Processor Bit */
#define SSR_TEND 0x04	/* Trandmit End */
#define	SSR_PER	0x08	/* Rx Parity Error */
#define SSR_FER 0x10	/* Rx Framing Error */
#define SSR_ORER 0x20	/* Rx Over Run Error */
#define SSR_RDRF 0x40	/* Rx Data Register Full */
#define SSR_TDRE 0x80	/* Tx Data Register Empty */

/* HSIO Status Code */
#define	HSIO_SUCCESS	0
#define	HSIO_RECEIVED	1
#define	HSIO_TRANSMITTED 2
#define	HSIO_FRAMING	-1
#define	HSIO_PARITY	-2
#define	HSIO_OVERRUN	-3
#define	HSIO_USEROFLO	-4
#define	HSIO_USERUFLO	-5
#define	HSIO_NODATA	-6
#define	HSIO_UNKNOWN	-7

/* Char CONST */
#define CODE_CR		(0x0D)
#define CODE_LF		(0x0A)
#define CTRL_C		(0x03)

/* Vector */
#define	V_SCI_ERI0	52
#define	V_SCI_RXI0	53
#define	V_SCI_TXI0	54
#define	V_SCI_TEI0	55
#define	V_SCI_ERI1	56
#define	V_SCI_RXI1	57
#define	V_SCI_TXI1	58
#define	V_SCI_TEI1	59

typedef unsigned short	ushort;
typedef unsigned char	uchar;

short sci_init(ushort speed);
short sci_inch();
short sci_outch(ushort c);
short sci_putch(ushort c);
short sci_getch();
short sci_putline(char *p);
short sci_getline(char *p);
short sci_CRLF();
void sci_wait(ushort speed);
void int_sci0();
void int_sce0();


struct sci {
	char rxdata;
	char rxstat;
	char txdata;
	char txstat;
} sci_port[1];

char prompt[] = "? ";

void main(void)
{
	short c;
	extern void vsci_eri0();
	extern void vsci_rxi0();

	printf("st..\n");

	_setvect(V_SCI_ERI0, vsci_eri0);
	_setvect(V_SCI_RXI0, vsci_rxi0);

	(void) sci_init(B9600);
	(void) sci_CRLF();
	(void) sci_putline("*SCI\n");
	(void) sci_CRLF();

	(void) sci_putline(prompt);
	do {
		c = sci_getch();
		if (c == CODE_CR || c == CODE_LF) {
			(void) sci_putline(prompt);
		}
		else if (c == '.' || c == 0x03) {
			(void) sci_CRLF();
			break;
		}
	}
	while (1);
	exit(0);
}


short sci_init(ushort speed)
{
	struct st_sci *ps;
	struct sci *pp;

	ps = &SCI0;
	pp = &sci_port[0];

	ps->SCR.BYTE = 0;
	ps->SMR.BYTE = 0;
	ps->BRR = speed;
	sci_wait(speed);

	ps->SCR.BYTE = SCR_RIE | SCR_TE | SCR_RE;
	/**
	d = ps->SSR.BYTE;		* Dummy Read *
	ps->SSR.BYTE = SSR_TDRE;	* Clear Error Flag (TDRE=1) *
	**/

	pp->rxdata = 0;
	pp->rxstat = 0;
	pp->txdata = 0;
	pp->txstat = 0;

	_enable(); /* enable interrupt */
	return(0);
}


short sci_inch()
{
	struct sci *pp;
	short c;

	pp = &sci_port[0];
	do {
		c = pp->rxstat;
	} while (c == 0);
	
	if (c == HSIO_RECEIVED) {	/* Data Received */
		c = pp->rxdata;
		pp->rxstat = HSIO_SUCCESS; /* Clear flag */
	} else { /* any error */
		pp->rxstat = HSIO_SUCCESS; /* Clear flag */
	}
	return(c);
}


short sci_outch(ushort c)
{
	struct st_sci *ps;
	short s;

	ps = &SCI0;
	do {
		s = ps->SSR.BIT.TDRE;
	} while (s == 0);
	ps->TDR = c;
	ps->SSR.BIT.TDRE = 0;
	return(0);
}


void int_sci0()
{
	struct st_sci *ps;
	struct sci *pp;
	short c;

	ps = &SCI0;
	pp = &sci_port[0];

	if (pp->rxstat != HSIO_SUCCESS){	/* Check the previous User Status */
		c = ps->SSR.BYTE; /* Dummy Read */
		pp->rxstat = HSIO_USEROFLO;
	}
	if (ps->SSR.BIT.RDRF) {
		c = ps->RDR;	/* else get Rx Data */
		ps->SSR.BIT.RDRF = 0; /* Celar flag */
		pp->rxdata = c;
		pp->rxstat = HSIO_RECEIVED;
	} else {
		c = ps->SSR.BYTE; /* Dummy Read */
		pp->rxstat = HSIO_UNKNOWN;
	}
}


void int_sce0()
{
	struct st_sci *ps;
	struct sci *pp;
	short c;

	ps = &SCI0;
	pp = &sci_port[0];

	if (ps->SSR.BIT.ORER){
		pp->rxstat = HSIO_OVERRUN;
	} else if (ps->SSR.BIT.FER){
		pp->rxstat = HSIO_FRAMING;
	} else if (ps->SSR.BIT.PER){
		pp->rxstat = HSIO_PARITY;
	} else {
		pp->rxstat = HSIO_UNKNOWN;
	}
	return;
}


short sci_putch(ushort c)
{
	/* to be modified for formatted output */

	if (c == '\n')
		(void) sci_CRLF();
	else
		(void) sci_outch(c);
	return(0);
}


short sci_getch()
{
	short c;

	if((c = sci_inch()) < 0)
		return(c);

	if (c == CODE_CR || c == CODE_LF) {
		(void) sci_CRLF();
	}
	else if (isprint(c) || isspace(c))
		(void) sci_putch(c);	/* echo */
	else if (c == CTRL_C) {
		(void) sci_CRLF();
		(void) sci_putline("^C");	/* Ctrl-C Echo back */
	}
	else
		(void) sci_putch('?');	/* invalid char */
	return(c);
}


short sci_putline(char *p)
{
	while(*p)
		sci_putch(*p++);
	return(0);
}


short sci_inline(char *p)
{
	short c;
	short cnt = 0;

	do {
		if((c = sci_inch()) < 0) { /* any error ? */
			*p = '\0';
			return(c);
		}
		cnt++;
		*p++ = c;
	}
	while(c != '\n');
	*p = '\0';
	return(cnt);
}


short sci_getline(char *p)
{
	short c;
	short cnt = 0;

	do {
		if((c = sci_getch()) < 0) { /* any error ? */
			*p = '\0';
			return(c);
		}
		cnt++;
		*p++ = c;
	}
	while(c != '\n');
	*p = '\0';
	return(cnt);
}


short sci_CRLF()
{
	(void) sci_outch(CODE_CR);
	(void) sci_outch(CODE_LF);
	return(0);
}


void sci_wait(ushort speed)
{
	ushort i;

	speed *= 6; /* waiting time rate */
	for (i = 0; i < speed; i++) {
		/* wait 1 bit time (1/9600 sec) */
	}
}
