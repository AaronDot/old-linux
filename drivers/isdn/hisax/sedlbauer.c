/* $Id: sedlbauer.c,v 1.1.2.16 1998/11/08 13:01:01 niemann Exp $

 * sedlbauer.c  low level stuff for Sedlbauer cards
 *              includes support for the Sedlbauer speed star (speed star II),
 *              support for the Sedlbauer speed fax+,
 *              support for the Sedlbauer ISDN-Controller PC/104 and
 *              support for the Sedlbauer speed pci
 *              derived from the original file asuscom.c from Karsten Keil
 *
 * Copyright (C) 1997,1998 Marcus Niemann (for the modifications to
 *                                         the original file asuscom.c)
 *
 * Author     Marcus Niemann (niemann@www-bib.fh-bielefeld.de)
 *
 * Thanks to  Karsten Keil
 *            Sedlbauer AG for informations
 *            Edgar Toernig
 *
 * $Log: sedlbauer.c,v $
 * Revision 1.1.2.16  1998/11/08 13:01:01  niemann
 * Added doc for Sedlbauer ISDN cards,
 * added info for downloading firmware (Sedlbauer speed fax+)
 *
 * Revision 1.1.2.15  1998/11/03 00:07:32  keil
 * certification related changes
 * fixed logging for smaller stack use
 *
 * Revision 1.1.2.14  1998/10/30 22:51:41  niemann
 * Added new card, Sedlbauer speed pci works now.
 *
 * Revision 1.1.2.13  1998/10/16 12:46:06  keil
 * fix pci detection for more as one card
 *
 * Revision 1.1.2.12  1998/10/13 18:38:53  keil
 * Fix PCI detection
 *
 * Revision 1.1.2.11  1998/10/13 10:27:30  keil
 * New cards, minor fixes
 *
 * Revision 1.1.2.10  1998/10/11 19:33:52  niemann
 * Added new IPAC based cards.
 * Code cleanup and simplified (sedlbauer.c)
 *
 * Revision 1.1.2.9  1998/10/04 23:05:03  keil
 * ISAR works now
 *
 * Revision 1.1.2.8  1998/09/30 22:28:10  keil
 * more work for isar support
 *
 * Revision 1.1.2.7  1998/09/27 13:07:01  keil
 * Apply most changes from 2.1.X (HiSax 3.1)
 *
 * Revision 1.1.2.6  1998/09/12 18:44:06  niemann
 * Added new card: Sedlbauer ISDN-Controller PC/104
 *
 * Revision 1.1.2.5  1998/04/08 21:58:44  keil
 * New init code
 *
 * Revision 1.1.2.4  1998/02/09 11:21:17  keil
 * Sedlbauer PCMCIA support from Marcus Niemann
 *
 * Revision 1.1.2.3  1998/01/27 22:37:29  keil
 * fast io
 *
 * Revision 1.1.2.2  1997/11/15 18:50:56  keil
 * new common init function
 *
 * Revision 1.1.2.1  1997/10/17 22:10:56  keil
 * new files on 2.0
 *
 * Revision 1.1  1997/09/11 17:32:04  keil
 * new
 *
 *
 */

/* Supported cards:
 * Card:	Chip:		Configuration:	Comment:
 * ---------------------------------------------------------------------
 * Speed Card	ISAC_HSCX	DIP-SWITCH
 * Speed Win	ISAC_HSCX	ISAPNP
 * Speed Fax+	ISAC_ISAR	ISAPNP		#HDLC works#
 * Speed Star	ISAC_HSCX	CARDMGR
 * Speed Win2	IPAC		ISAPNP
 * ISDN PC/104	IPAC		DIP-SWITCH
 * Speed Star2	IPAC		CARDMGR
 * Speed PCI	IPAC		PNP		
 *
 * Important:
 * For the sedlbauer speed fax+ to work properly you have to download 
 * the firmware onto the card.
 * For example: hisaxctrl <DriverID> 9 ISAR.BIN
*/

#define SEDLBAUER_PCI 1

#define __NO_VERSION__
#include <linux/config.h>
#include "hisax.h"
#include "isac.h"
#include "ipac.h"
#include "hscx.h"
#include "isar.h"
#include "isdnl1.h"
#include <linux/pci.h>
#include <linux/bios32.h>

extern const char *CardType[];

const char *Sedlbauer_revision = "$Revision: 1.1.2.16 $";

const char *Sedlbauer_Types[] =
	{"None", "speed card/win", "speed star", "speed fax+", 
	"speed win II / ISDN PC/104", "speed star II", "speed pci"};

#ifdef SEDLBAUER_PCI
#define PCI_VENDOR_SEDLBAUER	0xe159
#define PCI_SPEEDPCI_ID	0x02
#endif
 
#define SEDL_SPEED_CARD_WIN	1
#define SEDL_SPEED_STAR 	2
#define SEDL_SPEED_FAX		3
#define SEDL_SPEED_WIN2_PC104 	4
#define SEDL_SPEED_STAR2 	5
#define SEDL_SPEED_PCI   	6

#define SEDL_CHIP_TEST		0
#define SEDL_CHIP_ISAC_HSCX	1
#define SEDL_CHIP_ISAC_ISAR	2
#define SEDL_CHIP_IPAC		3

#define SEDL_BUS_ISA		1
#define SEDL_BUS_PCI		2
#define	SEDL_BUS_PCMCIA		3

#define byteout(addr,val) outb(val,addr)
#define bytein(addr) inb(addr)

#define SEDL_HSCX_ISA_RESET_ON	0
#define SEDL_HSCX_ISA_RESET_OFF	1
#define SEDL_HSCX_ISA_ISAC	2
#define SEDL_HSCX_ISA_HSCX	3
#define SEDL_HSCX_ISA_ADR	4

#define SEDL_HSCX_PCMCIA_RESET	0
#define SEDL_HSCX_PCMCIA_ISAC	1
#define SEDL_HSCX_PCMCIA_HSCX	2
#define SEDL_HSCX_PCMCIA_ADR	4

#define SEDL_ISAR_ISA_ISAC		4
#define SEDL_ISAR_ISA_ISAR		6
#define SEDL_ISAR_ISA_ADR		8
#define SEDL_ISAR_ISA_ISAR_RESET_ON	10
#define SEDL_ISAR_ISA_ISAR_RESET_OFF	12

#define SEDL_IPAC_ANY_ADR 	0
#define SEDL_IPAC_ANY_IPAC	2

#define SEDL_IPAC_PCI_BASE	0
#define SEDL_IPAC_PCI_ADR	0xc0
#define SEDL_IPAC_PCI_IPAC	0xc8

#define SEDL_RESET      0x3	/* same as DOS driver */

static inline u_char
readreg(unsigned int ale, unsigned int adr, u_char off)
{
	register u_char ret;
	long flags;

	save_flags(flags);
	cli();
	byteout(ale, off);
	ret = bytein(adr);
	restore_flags(flags);
	return (ret);
}

static inline void
readfifo(unsigned int ale, unsigned int adr, u_char off, u_char * data, int size)
{
	/* fifo read without cli because it's allready done  */

	byteout(ale, off);
	insb(adr, data, size);
}


static inline void
writereg(unsigned int ale, unsigned int adr, u_char off, u_char data)
{
	long flags;

	save_flags(flags);
	cli();
	byteout(ale, off);
	byteout(adr, data);
	restore_flags(flags);
}

static inline void
writefifo(unsigned int ale, unsigned int adr, u_char off, u_char * data, int size)
{
	/* fifo write without cli because it's allready done  */
	byteout(ale, off);
	outsb(adr, data, size);
}

/* Interface functions */

static u_char
ReadISAC(struct IsdnCardState *cs, u_char offset)
{
	return (readreg(cs->hw.sedl.adr, cs->hw.sedl.isac, offset));
}

static void
WriteISAC(struct IsdnCardState *cs, u_char offset, u_char value)
{
	writereg(cs->hw.sedl.adr, cs->hw.sedl.isac, offset, value);
}

static void
ReadISACfifo(struct IsdnCardState *cs, u_char * data, int size)
{
	readfifo(cs->hw.sedl.adr, cs->hw.sedl.isac, 0, data, size);
}

static void
WriteISACfifo(struct IsdnCardState *cs, u_char * data, int size)
{
	writefifo(cs->hw.sedl.adr, cs->hw.sedl.isac, 0, data, size);
}

static u_char
ReadISAC_IPAC(struct IsdnCardState *cs, u_char offset)
{
        return (readreg(cs->hw.sedl.adr, cs->hw.sedl.isac, offset|0x80));}

static void
WriteISAC_IPAC(struct IsdnCardState *cs, u_char offset, u_char value)
{
        writereg(cs->hw.sedl.adr, cs->hw.sedl.isac, offset|0x80, value);
}

static void
ReadISACfifo_IPAC(struct IsdnCardState *cs, u_char * data, int size)
{
        readfifo(cs->hw.sedl.adr, cs->hw.sedl.isac, 0x80, data, size);
}

static void
WriteISACfifo_IPAC(struct IsdnCardState *cs, u_char * data, int size)
{
        writefifo(cs->hw.sedl.adr, cs->hw.sedl.isac, 0x80, data, size);
}

static u_char
ReadHSCX(struct IsdnCardState *cs, int hscx, u_char offset)
{
	return (readreg(cs->hw.sedl.adr,
			cs->hw.sedl.hscx, offset + (hscx ? 0x40 : 0)));
}

static void
WriteHSCX(struct IsdnCardState *cs, int hscx, u_char offset, u_char value)
{
	writereg(cs->hw.sedl.adr,
		 cs->hw.sedl.hscx, offset + (hscx ? 0x40 : 0), value);
}

/* ISAR access routines
 * mode = 0 access with IRQ on
 * mode = 1 access with IRQ off
 * mode = 2 access with IRQ off and using last offset
 */
  
static u_char
ReadISAR(struct IsdnCardState *cs, int mode, u_char offset)
{	
	if (mode == 0)
		return (readreg(cs->hw.sedl.adr, cs->hw.sedl.hscx, offset));
	else if (mode == 1)
		byteout(cs->hw.sedl.adr, offset);
	return(bytein(cs->hw.sedl.hscx));
}

static void
WriteISAR(struct IsdnCardState *cs, int mode, u_char offset, u_char value)
{
	if (mode == 0)
		writereg(cs->hw.sedl.adr, cs->hw.sedl.hscx, offset, value);
	else {
		if (mode == 1)
			byteout(cs->hw.sedl.adr, offset);
		byteout(cs->hw.sedl.hscx, value);
	}
}

/*
 * fast interrupt HSCX stuff goes here
 */

#define READHSCX(cs, nr, reg) readreg(cs->hw.sedl.adr, \
		cs->hw.sedl.hscx, reg + (nr ? 0x40 : 0))
#define WRITEHSCX(cs, nr, reg, data) writereg(cs->hw.sedl.adr, \
		cs->hw.sedl.hscx, reg + (nr ? 0x40 : 0), data)

#define READHSCXFIFO(cs, nr, ptr, cnt) readfifo(cs->hw.sedl.adr, \
		cs->hw.sedl.hscx, (nr ? 0x40 : 0), ptr, cnt)

#define WRITEHSCXFIFO(cs, nr, ptr, cnt) writefifo(cs->hw.sedl.adr, \
		cs->hw.sedl.hscx, (nr ? 0x40 : 0), ptr, cnt)

#include "hscx_irq.c"

static void
sedlbauer_interrupt(int intno, void *dev_id, struct pt_regs *regs)
{
	struct IsdnCardState *cs = dev_id;
	u_char val, stat = 0;

	if (!cs) {
		printk(KERN_WARNING "Sedlbauer: Spurious interrupt!\n");
		return;
	}

	if ((cs->hw.sedl.bus == SEDL_BUS_PCMCIA) && (*cs->busy_flag == 1)) {
          /* The card tends to generate interrupts while being removed
             causing us to just crash the kernel. bad. */
          printk(KERN_WARNING "Sedlbauer: card not available!\n");
          return;
        }

	val = readreg(cs->hw.sedl.adr, cs->hw.sedl.hscx, HSCX_ISTA + 0x40);
      Start_HSCX:
	if (val) {
		hscx_int_main(cs, val);
		stat |= 1;
	}
	val = readreg(cs->hw.sedl.adr, cs->hw.sedl.isac, ISAC_ISTA);
      Start_ISAC:
	if (val) {
		isac_interrupt(cs, val);
		stat |= 2;
	}
	val = readreg(cs->hw.sedl.adr, cs->hw.sedl.hscx, HSCX_ISTA + 0x40);
	if (val) {
		if (cs->debug & L1_DEB_HSCX)
			debugl1(cs, "HSCX IntStat after IntRoutine");
		goto Start_HSCX;
	}
	val = readreg(cs->hw.sedl.adr, cs->hw.sedl.isac, ISAC_ISTA);
	if (val) {
		if (cs->debug & L1_DEB_ISAC)
			debugl1(cs, "ISAC IntStat after IntRoutine");
		goto Start_ISAC;
	}
	if (stat & 1) {
		writereg(cs->hw.sedl.adr, cs->hw.sedl.hscx, HSCX_MASK, 0xFF);
		writereg(cs->hw.sedl.adr, cs->hw.sedl.hscx, HSCX_MASK + 0x40, 0xFF);
		writereg(cs->hw.sedl.adr, cs->hw.sedl.hscx, HSCX_MASK, 0x0);
		writereg(cs->hw.sedl.adr, cs->hw.sedl.hscx, HSCX_MASK + 0x40, 0x0);
	}
	if (stat & 2) {
		writereg(cs->hw.sedl.adr, cs->hw.sedl.isac, ISAC_MASK, 0xFF);
		writereg(cs->hw.sedl.adr, cs->hw.sedl.isac, ISAC_MASK, 0x0);
	}
}

static void
sedlbauer_interrupt_ipac(int intno, void *dev_id, struct pt_regs *regs)
{
	struct IsdnCardState *cs = dev_id;
	u_char ista, val, icnt = 20;

	if (!cs) {
		printk(KERN_WARNING "Sedlbauer: Spurious interrupt!\n");
		return;
	}
	ista = readreg(cs->hw.sedl.adr, cs->hw.sedl.isac, IPAC_ISTA);
Start_IPAC:
	if (cs->debug & L1_DEB_IPAC)
		debugl1(cs, "IPAC ISTA %02X", ista);
	if (ista & 0x0f) {
		val = readreg(cs->hw.sedl.adr, cs->hw.sedl.hscx, HSCX_ISTA + 0x40);
		if (ista & 0x01)
			val |= 0x01;
		if (ista & 0x04)
			val |= 0x02;
		if (ista & 0x08)
			val |= 0x04;
		if (val)
			hscx_int_main(cs, val);
	}
	if (ista & 0x20) {
		val = 0xfe & readreg(cs->hw.sedl.adr, cs->hw.sedl.isac, ISAC_ISTA | 0x80);
		if (val) {
			isac_interrupt(cs, val);
		}
	}
	if (ista & 0x10) {
		val = 0x01;
		isac_interrupt(cs, val);
	}
	ista  = readreg(cs->hw.sedl.adr, cs->hw.sedl.isac, IPAC_ISTA);
	if ((ista & 0x3f) && icnt) {
		icnt--;
		goto Start_IPAC;
	}
	if (!icnt)
		printk(KERN_WARNING "Sedlbauer IRQ LOOP\n");
	writereg(cs->hw.sedl.adr, cs->hw.sedl.isac, IPAC_MASK, 0xFF);
	writereg(cs->hw.sedl.adr, cs->hw.sedl.isac, IPAC_MASK, 0xC0);
}

static void
sedlbauer_interrupt_isar(int intno, void *dev_id, struct pt_regs *regs)
{
	struct IsdnCardState *cs = dev_id;
	u_char val;
	int cnt = 20;

	if (!cs) {
		printk(KERN_WARNING "Sedlbauer: Spurious interrupt!\n");
		return;
	}

	val = readreg(cs->hw.sedl.adr, cs->hw.sedl.hscx, ISAR_IRQBIT);
      Start_ISAR:
	if (val & ISAR_IRQSTA)
		isar_int_main(cs);
	val = readreg(cs->hw.sedl.adr, cs->hw.sedl.isac, ISAC_ISTA);
      Start_ISAC:
	if (val)
		isac_interrupt(cs, val);
	val = readreg(cs->hw.sedl.adr, cs->hw.sedl.hscx, ISAR_IRQBIT);
	if ((val & ISAR_IRQSTA) && --cnt) {
		if (cs->debug & L1_DEB_HSCX)
			debugl1(cs, "ISAR IntStat after IntRoutine");
		goto Start_ISAR;
	}
	val = readreg(cs->hw.sedl.adr, cs->hw.sedl.isac, ISAC_ISTA);
	if (val && --cnt) {
		if (cs->debug & L1_DEB_ISAC)
			debugl1(cs, "ISAC IntStat after IntRoutine");
		goto Start_ISAC;
	}
	if (!cnt)
		printk(KERN_WARNING "Sedlbauer IRQ LOOP\n");

	writereg(cs->hw.sedl.adr, cs->hw.sedl.hscx, ISAR_IRQBIT, 0);
	writereg(cs->hw.sedl.adr, cs->hw.sedl.isac, ISAC_MASK, 0xFF);
	writereg(cs->hw.sedl.adr, cs->hw.sedl.isac, ISAC_MASK, 0x0);
	writereg(cs->hw.sedl.adr, cs->hw.sedl.hscx, ISAR_IRQBIT, ISAR_IRQMSK);
}

void
release_io_sedlbauer(struct IsdnCardState *cs)
{
	int bytecnt = (cs->subtyp == SEDL_SPEED_FAX) ? 16 : 8;

	if (cs->hw.sedl.bus == SEDL_BUS_PCI) {
		bytecnt = 256;
	}
	if (cs->hw.sedl.cfg_reg)
		release_region(cs->hw.sedl.cfg_reg, bytecnt);
}

static void
reset_sedlbauer(struct IsdnCardState *cs)
{
	long flags;

	printk(KERN_INFO "Sedlbauer: resetting card\n");

	if (!((cs->hw.sedl.bus == SEDL_BUS_PCMCIA) &&
	   (cs->hw.sedl.chip == SEDL_CHIP_ISAC_HSCX))) {
		if (cs->hw.sedl.chip == SEDL_CHIP_IPAC) {
			writereg(cs->hw.sedl.adr, cs->hw.sedl.isac, IPAC_POTA2, 0x20);
			save_flags(flags);
			sti();
			current->state = TASK_INTERRUPTIBLE;
			current->timeout = jiffies + 1;
			schedule();
			writereg(cs->hw.sedl.adr, cs->hw.sedl.isac, IPAC_POTA2, 0x0);
			current->state = TASK_INTERRUPTIBLE;
			current->timeout = jiffies + 1;
			schedule();
			writereg(cs->hw.sedl.adr, cs->hw.sedl.isac, IPAC_CONF, 0x0);
			writereg(cs->hw.sedl.adr, cs->hw.sedl.isac, IPAC_ACFG, 0xff);
			writereg(cs->hw.sedl.adr, cs->hw.sedl.isac, IPAC_AOE, 0x0);
			writereg(cs->hw.sedl.adr, cs->hw.sedl.isac, IPAC_MASK, 0xc0);
			writereg(cs->hw.sedl.adr, cs->hw.sedl.isac, IPAC_PCFG, 0x12);
			restore_flags(flags);
		} else {		
			byteout(cs->hw.sedl.reset_on, SEDL_RESET);	/* Reset On */
			save_flags(flags);
			sti();
			current->state = TASK_INTERRUPTIBLE;
			current->timeout = jiffies + 1;
			schedule();
			byteout(cs->hw.sedl.reset_off, 0);	/* Reset Off */
			current->state = TASK_INTERRUPTIBLE;
			current->timeout = jiffies + 1;
			schedule();
			restore_flags(flags);
		}
	}
}

static int
Sedl_card_msg(struct IsdnCardState *cs, int mt, void *arg)
{
	switch (mt) {
		case CARD_RESET:
			reset_sedlbauer(cs);
			return(0);
		case CARD_RELEASE:
			release_io_sedlbauer(cs);
			return(0);
		case CARD_SETIRQ:
			if (cs->hw.sedl.chip == SEDL_CHIP_ISAC_ISAR) {
				return(request_irq(cs->irq, &sedlbauer_interrupt_isar,
					I4L_IRQ_FLAG, "HiSax", cs));
			} else if (cs->hw.sedl.chip == SEDL_CHIP_IPAC) {
				return(request_irq(cs->irq, &sedlbauer_interrupt_ipac,
					I4L_IRQ_FLAG, "HiSax", cs));
			} else {
				return(request_irq(cs->irq, &sedlbauer_interrupt,
					I4L_IRQ_FLAG, "HiSax", cs));
			}
		case CARD_INIT:
			if (cs->hw.sedl.chip == SEDL_CHIP_ISAC_ISAR) {
				clear_pending_isac_ints(cs);
				writereg(cs->hw.sedl.adr, cs->hw.sedl.hscx,
					ISAR_IRQBIT, 0);
				initisac(cs);
				initisar(cs);
				/* Reenable all IRQ */
				cs->writeisac(cs, ISAC_MASK, 0);
				/* RESET Receiver and Transmitter */
				cs->writeisac(cs, ISAC_CMDR, 0x41);
			} else {
				inithscxisac(cs, 3);
			}
			return(0);
		case CARD_TEST:
			return(0);
		case CARD_LOAD_FIRM:
			if (cs->hw.sedl.chip == SEDL_CHIP_ISAC_ISAR) {
				if (isar_load_firmware(cs, arg))
					return(1);
				else 
					ll_run(cs);
			}
			return(0);
	}
	return(0);
}


#ifdef SEDLBAUER_PCI
static  int pci_index __initdata = 0;
#endif

__initfunc(int
setup_sedlbauer(struct IsdnCard *card))
{
	int bytecnt, ver, val;
	struct IsdnCardState *cs = card->cs;
	char tmp[64];

	strcpy(tmp, Sedlbauer_revision);
	printk(KERN_INFO "HiSax: Sedlbauer driver Rev. %s\n", HiSax_getrev(tmp));
	
 	if (cs->typ == ISDN_CTYPE_SEDLBAUER) {
 		cs->subtyp = SEDL_SPEED_CARD_WIN;
		cs->hw.sedl.bus = SEDL_BUS_ISA;
		cs->hw.sedl.chip = SEDL_CHIP_TEST;
 	} else if (cs->typ == ISDN_CTYPE_SEDLBAUER_PCMCIA) {	
 		cs->subtyp = SEDL_SPEED_STAR;
		cs->hw.sedl.bus = SEDL_BUS_PCMCIA;
		cs->hw.sedl.chip = SEDL_CHIP_TEST;
 	} else if (cs->typ == ISDN_CTYPE_SEDLBAUER_FAX) {	
 		cs->subtyp = SEDL_SPEED_FAX;
		cs->hw.sedl.bus = SEDL_BUS_ISA;
		cs->hw.sedl.chip = SEDL_CHIP_ISAC_ISAR;
 	} else
		return (0);

	bytecnt = 8;
	if (card->para[1]) {
		cs->hw.sedl.cfg_reg = card->para[1];
		cs->irq = card->para[0];
		if (cs->hw.sedl.chip == SEDL_CHIP_ISAC_ISAR) {
			bytecnt = 16;
		}
	} else {
/* Probe for Sedlbauer speed pci */
#if SEDLBAUER_PCI
#if CONFIG_PCI
		for (; pci_index < 255; pci_index++) {
			unsigned char pci_bus, pci_device_fn;
			unsigned int ioaddr;
			unsigned char irq;

			if (pcibios_find_device (PCI_VENDOR_SEDLBAUER,
						PCI_SPEEDPCI_ID, pci_index,
						&pci_bus, &pci_device_fn) != 0) {
				continue;
			}
			pcibios_read_config_byte(pci_bus, pci_device_fn,
					PCI_INTERRUPT_LINE, &irq);
			pcibios_read_config_dword(pci_bus, pci_device_fn,
					PCI_BASE_ADDRESS_0, &ioaddr);
			cs->irq = irq;
			cs->hw.sedl.cfg_reg = ioaddr & PCI_BASE_ADDRESS_IO_MASK; 
			if (!cs->hw.sedl.cfg_reg) {
				printk(KERN_WARNING "Sedlbauer: No IO-Adr for PCI card found\n");
				return(0);
			}
			cs->hw.sedl.bus = SEDL_BUS_PCI;
			cs->hw.sedl.chip = SEDL_CHIP_IPAC;
			cs->subtyp = SEDL_SPEED_PCI;
			bytecnt = 256;
			byteout(cs->hw.sedl.cfg_reg, 0xff);
			byteout(cs->hw.sedl.cfg_reg, 0x00);
			byteout(cs->hw.sedl.cfg_reg+ 2, 0xdd);
			byteout(cs->hw.sedl.cfg_reg+ 5, 0x02);
			break;
		}	
		if (pci_index == 255) {
			printk(KERN_WARNING "Sedlbauer: No PCI card found\n");
			return(0);
		}
		pci_index++;
#else
		printk(KERN_WARNING "Sedlbauer: NO_PCI_BIOS\n");
		return (0);
#endif /* CONFIG_PCI */
#endif /* SEDLBAUER_PCI */
	}	
	
       	/* In case of the sedlbauer pcmcia card, this region is in use,
           reserved for us by the card manager. So we do not check it
           here, it would fail. */
	if (cs->hw.sedl.bus != SEDL_BUS_PCMCIA &&
		check_region((cs->hw.sedl.cfg_reg), bytecnt)) {
		printk(KERN_WARNING
			"HiSax: %s config port %x-%x already in use\n",
			CardType[card->typ],
			cs->hw.sedl.cfg_reg,
			cs->hw.sedl.cfg_reg + bytecnt);
			return (0);
	} else {
		request_region(cs->hw.sedl.cfg_reg, bytecnt, "sedlbauer isdn");
	}

	printk(KERN_INFO
	       "Sedlbauer: defined at 0x%x-0x%x IRQ %d\n",
	       cs->hw.sedl.cfg_reg,
	       cs->hw.sedl.cfg_reg + bytecnt,
	       cs->irq);

	cs->BC_Read_Reg = &ReadHSCX;
	cs->BC_Write_Reg = &WriteHSCX;
	cs->BC_Send_Data = &hscx_fill_fifo;
	cs->cardmsg = &Sedl_card_msg;

/*
 * testing ISA and PCMCIA Cards for IPAC, default is ISAC 
 * do not test for PCI card, because ports are different
 * and PCI card uses only IPAC (for the moment)
 */	
	if (cs->hw.sedl.bus != SEDL_BUS_PCI) {
		val = readreg(cs->hw.sedl.cfg_reg + SEDL_IPAC_ANY_ADR,
        	        cs->hw.sedl.cfg_reg + SEDL_IPAC_ANY_IPAC, IPAC_ID);
	        if (val == 1) {
		/* IPAC */
                	cs->subtyp = SEDL_SPEED_WIN2_PC104;
			if (cs->hw.sedl.bus == SEDL_BUS_PCMCIA) {
				cs->subtyp = SEDL_SPEED_STAR2;
			}
			cs->hw.sedl.chip = SEDL_CHIP_IPAC;
		} else {
		/* ISAC_HSCX oder ISAC_ISAR */
			if (cs->hw.sedl.chip == SEDL_CHIP_TEST) {
				cs->hw.sedl.chip = SEDL_CHIP_ISAC_HSCX;
			}
		}
	}

/*
 * hw.sedl.chip is now properly set
 */
	printk(KERN_INFO "Sedlbauer: %s detected\n",
		Sedlbauer_Types[cs->subtyp]);


	if (cs->hw.sedl.chip == SEDL_CHIP_IPAC) {
	/* IPAC */
		if (cs->hw.sedl.bus == SEDL_BUS_PCI) {
	                cs->hw.sedl.adr  = cs->hw.sedl.cfg_reg + SEDL_IPAC_PCI_ADR;
        	        cs->hw.sedl.isac = cs->hw.sedl.cfg_reg + SEDL_IPAC_PCI_IPAC;
                	cs->hw.sedl.hscx = cs->hw.sedl.cfg_reg + SEDL_IPAC_PCI_IPAC;
		} else {
	                cs->hw.sedl.adr  = cs->hw.sedl.cfg_reg + SEDL_IPAC_ANY_ADR;
        	        cs->hw.sedl.isac = cs->hw.sedl.cfg_reg + SEDL_IPAC_ANY_IPAC;
                	cs->hw.sedl.hscx = cs->hw.sedl.cfg_reg + SEDL_IPAC_ANY_IPAC;
		}
                test_and_set_bit(HW_IPAC, &cs->HW_Flags);
                cs->readisac = &ReadISAC_IPAC;
                cs->writeisac = &WriteISAC_IPAC;
                cs->readisacfifo = &ReadISACfifo_IPAC;
                cs->writeisacfifo = &WriteISACfifo_IPAC;

		val = readreg(cs->hw.sedl.adr,cs->hw.sedl.isac, IPAC_ID);
                printk(KERN_INFO "Sedlbauer: IPAC version %x\n", val);
		reset_sedlbauer(cs);
	} else {
	/* ISAC_HSCX oder ISAC_ISAR */
		cs->readisac = &ReadISAC;
		cs->writeisac = &WriteISAC;
		cs->readisacfifo = &ReadISACfifo;
		cs->writeisacfifo = &WriteISACfifo;
		if (cs->hw.sedl.chip == SEDL_CHIP_ISAC_ISAR) {
			cs->hw.sedl.adr = cs->hw.sedl.cfg_reg + SEDL_ISAR_ISA_ADR;
			cs->hw.sedl.isac = cs->hw.sedl.cfg_reg + SEDL_ISAR_ISA_ISAC;
			cs->hw.sedl.hscx = cs->hw.sedl.cfg_reg + SEDL_ISAR_ISA_ISAR;
			cs->hw.sedl.reset_on = cs->hw.sedl.cfg_reg + SEDL_ISAR_ISA_ISAR_RESET_ON;
			cs->hw.sedl.reset_off = cs->hw.sedl.cfg_reg + SEDL_ISAR_ISA_ISAR_RESET_OFF;
			cs->bcs[0].hw.isar.reg = &cs->hw.sedl.isar;
			cs->bcs[1].hw.isar.reg = &cs->hw.sedl.isar;
			test_and_set_bit(HW_ISAR, &cs->HW_Flags);
	
			ISACVersion(cs, "Sedlbauer:");
		
			cs->BC_Read_Reg = &ReadISAR;
			cs->BC_Write_Reg = &WriteISAR;
			cs->BC_Send_Data = &isar_fill_fifo;
			ver = ISARVersion(cs, "Sedlbauer:");
			if (ver < 0) {
				printk(KERN_WARNING
					"Sedlbauer: wrong ISAR version (ret = %d)\n", ver);
				release_io_sedlbauer(cs);
				return (0);
			}
		} else {
			if (cs->hw.sedl.bus == SEDL_BUS_PCMCIA) {
				cs->hw.sedl.adr = cs->hw.sedl.cfg_reg + SEDL_HSCX_PCMCIA_ADR;
				cs->hw.sedl.isac = cs->hw.sedl.cfg_reg + SEDL_HSCX_PCMCIA_ISAC;
				cs->hw.sedl.hscx = cs->hw.sedl.cfg_reg + SEDL_HSCX_PCMCIA_HSCX;
				cs->hw.sedl.reset_on = cs->hw.sedl.cfg_reg + SEDL_HSCX_PCMCIA_RESET;
				cs->hw.sedl.reset_off = cs->hw.sedl.cfg_reg + SEDL_HSCX_PCMCIA_RESET;
			} else {
				cs->hw.sedl.adr = cs->hw.sedl.cfg_reg + SEDL_HSCX_ISA_ADR;
				cs->hw.sedl.isac = cs->hw.sedl.cfg_reg + SEDL_HSCX_ISA_ISAC;
				cs->hw.sedl.hscx = cs->hw.sedl.cfg_reg + SEDL_HSCX_ISA_HSCX;
				cs->hw.sedl.reset_on = cs->hw.sedl.cfg_reg + SEDL_HSCX_ISA_RESET_ON;
				cs->hw.sedl.reset_off = cs->hw.sedl.cfg_reg + SEDL_HSCX_ISA_RESET_OFF;
			}
			ISACVersion(cs, "Sedlbauer:");
		
			if (HscxVersion(cs, "Sedlbauer:")) {
				printk(KERN_WARNING
					"Sedlbauer: wrong HSCX versions check IO address\n");
				release_io_sedlbauer(cs);
				return (0);
			}
			reset_sedlbauer(cs);
		}
	}
	return (1);
}
