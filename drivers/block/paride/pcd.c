/* 
	pcd.c	(c) 1997-8  Grant R. Guenther <grant@torque.net>
		            Under the terms of the GNU public license.


	Special 2.0.34 version


	This is high-level driver for parallel port ATAPI CDrom
        drives based on chips supported by the paride module.

        By default, the driver will autoprobe for a single parallel
        port ATAPI CDrom drive, but if their individual parameters are
        specified, the driver can handle up to 4 drives.

        The behaviour of the pcd driver can be altered by setting
        some parameters from the insmod command line.  The following
        parameters are adjustable:

            drive0      These four arguments can be arrays of       
            drive1      1-6 integers as follows:
            drive2
            drive3      <prt>,<pro>,<uni>,<mod>,<slv>,<dly>

                        Where,

                <prt>   is the base of the parallel port address for
                        the corresponding drive.  (required)

                <pro>   is the protocol number for the adapter that
                        supports this drive.  These numbers are
                        logged by 'paride' when the protocol modules
                        are initialised.  (0 if not given)

                <uni>   for those adapters that support chained
                        devices, this is the unit selector for the
                        chain of devices on the given port.  It should
                        be zero for devices that don't support chaining.
                        (0 if not given)

                <mod>   this can be -1 to choose the best mode, or one
                        of the mode numbers supported by the adapter.
                        (-1 if not given)

		<slv>   ATAPI CDroms can be jumpered to master or slave.
			Set this to 0 to choose the master drive, 1 to
                        choose the slave, -1 (the default) to choose the
			first drive found.

                <dly>   some parallel ports require the driver to 
                        go more slowly.  -1 sets a default value that
                        should work with the chosen protocol.  Otherwise,
                        set this to a small integer, the larger it is
                        the slower the port i/o.  In some cases, setting
                        this to zero will speed up the device. (default -1)
                        
            major       You may use this parameter to overide the
                        default major number (46) that this driver
                        will use.  Be sure to change the device
                        name as well.

            name        This parameter is a character string that
                        contains the name the kernel will use for this
                        device (in /proc output, for instance).
                        (default "pcd")

            verbose     This parameter controls the amount of logging
                        that the driver will do.  Set it to 0 for
                        normal operation, 1 to see autoprobe progress
                        messages, or 2 to see additional debugging
                        output.  (default 0)
  
            nice        This parameter controls the driver's use of
                        idle CPU time, at the expense of some speed.
 
	If this driver is built into the kernel, you can use kernel
        the following command line parameters, with the same values
        as the corresponding module parameters listed above:

	    pcd.drive0
	    pcd.drive1
	    pcd.drive2
	    pcd.drive3
	    pcd.nice

        In addition, you can use the parameter pcd.disable to disable
        the driver entirely.

*/

/* Changes:

	1.01	GRG 1997.01.24	Added test unit ready support
	1.02    GRG 1998.05.06  Changes to pcd_completion, ready_wait,
				and loosen interpretation of ATAPI
			        standard for clearing error status.
				Use spinlocks. Eliminate sti().
	1.03    GRG 1998.06.16  Eliminated an Ugh
	1.04	GRG 1998.08.15  Added extra debugging, improvements to
				pcd_completion, use HZ in loop timing
	1.05s   GRG 1998.09.24  Added jumbo support, adjust reset timeout

*/

#define	PCD_VERSION	"1.05s"
#define PCD_MAJOR	46
#define PCD_NAME	"pcd"
#define PCD_UNITS	4

/* Here are things one can override from the insmod command.
   Most are autoprobed by paride unless set here.  Verbose is off
   by default.

*/

static int      verbose = 0;
static int      major = PCD_MAJOR;
static char     *name = PCD_NAME;
static int      nice = 0;
static int      disable = 0;

static int drive0[6] = {0,0,0,-1,-1,-1};
static int drive1[6] = {0,0,0,-1,-1,-1};
static int drive2[6] = {0,0,0,-1,-1,-1};
static int drive3[6] = {0,0,0,-1,-1,-1};

static int (*drives[4])[6] = {&drive0,&drive1,&drive2,&drive3};
static int pcd_drive_count;

#define D_PRT   0
#define D_PRO   1
#define D_UNI   2
#define D_MOD   3
#define D_SLV   4
#define D_DLY   5

#define DU              (*drives[unit])

/* end of parameters */

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/cdrom.h>

#include <asm/segment.h>
#include "spinlock.h"

#ifndef MODULE

#include "setup.h"

static STT pcd_stt[6] = {{"drive0",6,drive0},
                         {"drive1",6,drive1},
                         {"drive2",6,drive2},
                         {"drive3",6,drive3},
			 {"disable",1,&disable},
                         {"nice",1,&nice}};

void pcd_setup( char *str, int *ints)

{       generic_setup(pcd_stt,6,str);
}

#endif

#include "paride.h"

/* set up defines for blk.h,  why don't all drivers do it this way ? */

#define MAJOR_NR	major
#define DEVICE_NAME "PCD"
#define DEVICE_REQUEST do_pcd_request
#define DEVICE_NR(device) (MINOR(device))
#define DEVICE_ON(device)
#define DEVICE_OFF(device)

#include <linux/blk.h>

#include "pseudo.h"

#define PCD_RETRIES	     5
#define PCD_TMO		   800		/* timeout in jiffies */
#define PCD_DELAY           50          /* spin delay in uS */
#define PCD_READY_TMO	    20		/* in seconds */

#define PCD_SPIN	(1000000*PCD_TMO)/(HZ*PCD_DELAY)

#define IDE_ERR		0x01
#define IDE_DRQ         0x08
#define IDE_READY       0x40
#define IDE_BUSY        0x80

int pcd_init(void);
void cleanup_module( void );

static int 	pcd_open(struct inode *inode, struct file *file);
static void 	do_pcd_request(void);
static void 	do_pcd_read(void);
static int 	pcd_ioctl(struct inode *inode,struct file *file,
                         unsigned int cmd, unsigned long arg);

static void pcd_release (struct inode *inode, struct file *file);

static int 	pcd_detect(void);
static void     pcd_lock(int unit);
static void     pcd_unlock(int unit);
static void     pcd_eject(int unit);
static int      pcd_check_media(int unit);
static void     do_pcd_read_drq(void);

static int pcd_blocksizes[PCD_UNITS];

#define PCD_NAMELEN	8

struct pcd_unit {
	struct pi_adapter pia;	/* interface to paride layer */
	struct pi_adapter *pi;
	int drive;		/* master/slave */
	int last_sense;		/* result of last request sense */
	int access;		/* count of active opens */
	int present;		/* does this unit exist ? */
	char name[PCD_NAMELEN];	/* pcd0, pcd1, etc */
	};

struct pcd_unit pcd[PCD_UNITS];

/*  'unit' must be defined in all functions - either as a local or a param */

#define PCD pcd[unit]
#define PI PCD.pi

static char pcd_scratch[64];
static char pcd_buffer[2048];           /* raw block buffer */
static int pcd_bufblk = -1;             /* block in buffer, in CD units,
                                           -1 for nothing there. See also
					   pd_unit.
					 */

/* the variables below are used mainly in the I/O request engine, which
   processes only one request at a time.
*/

static int pcd_unit = -1;		/* unit of current request & bufblk */
static int pcd_retries;			/* retries on current request */
static int pcd_busy = 0;		/* request being processed ? */
static int pcd_sector;			/* address of next requested sector */
static int pcd_count;			/* number of blocks still to do */
static char * pcd_buf;			/* buffer for request in progress */

static int pcd_warned = 0;		/* Have we logged a phase warning ? */

/* kernel glue structures */

static struct file_operations pcd_fops = {
	NULL,			/* lseek - default */
	block_read,		/* read - general block-dev read */
	block_write,		/* write - general block-dev write */
	NULL,			/* readdir - bad */
	NULL,			/* select */
	pcd_ioctl,		/* ioctl */
	NULL,			/* mmap */
	pcd_open,		/* open */
	pcd_release,		/* release */
	block_fsync,		/* fsync */
	NULL,			/* fasync */
	NULL,                   /* media change ? */
	NULL			/* revalidate new media */
};

static void pcd_init_units( void )

{       int     unit, j;

        pcd_drive_count = 0;
        for (unit=0;unit<PCD_UNITS;unit++) {
                PCD.pi = & PCD.pia;
                PCD.access = 0;
                PCD.present = 0;
		PCD.last_sense = 0;
                j = 0;
                while ((j < PCD_NAMELEN-2) && (PCD.name[j]=name[j])) j++;
                PCD.name[j++] = '0' + unit;
                PCD.name[j] = 0;
                PCD.drive = DU[D_SLV];
                if (DU[D_PRT]) pcd_drive_count++;
        }
}

int pcd_init (void)	/* preliminary initialisation */

{       int 	i;

	if (disable) return -1;

	pcd_init_units();

	if (pcd_detect()) return -1;

	if (register_blkdev(MAJOR_NR,name,&pcd_fops)) {
		printk("pcd: unable to get major number %d\n",MAJOR_NR);
		return -1;
	}
	blk_dev[MAJOR_NR].request_fn = DEVICE_REQUEST;
	read_ahead[MAJOR_NR] = 8;	/* 8 sector (4kB) read ahead */

	for (i=0;i<PCD_UNITS;i++) pcd_blocksizes[i] = 1024;
        blksize_size[MAJOR_NR] = pcd_blocksizes;

	return 0;
}

static int pcd_open (struct inode *inode, struct file *file)

{	int unit = DEVICE_NR(inode->i_rdev);

	if  ((unit >= PCD_UNITS) || (!PCD.present)) return -ENODEV;

	if (file->f_mode & 2) return -EROFS;  /* wants to write ? */

	MOD_INC_USE_COUNT;

	if (pcd_check_media(unit)) {
		MOD_DEC_USE_COUNT;
		return -ENXIO;
	}

	pcd_lock(unit);

	PCD.access++;
	return 0;
}

static void do_pcd_request (void)

{       int unit;

	if (pcd_busy) return;
        while (1) {
	    if ((!CURRENT) || (CURRENT->rq_status == RQ_INACTIVE)) return;
	    INIT_REQUEST;
	    if (CURRENT->cmd == READ) {
		unit = MINOR(CURRENT->rq_dev);
		if (unit != pcd_unit) {
			pcd_bufblk = -1;
			pcd_unit = unit;
		}
	        pcd_sector = CURRENT->sector;
	        pcd_count = CURRENT->nr_sectors;
	        pcd_buf = CURRENT->buffer;
		pcd_busy = 1;
	        ps_set_intr(do_pcd_read,0,0,nice); 
		return;
	    } 
	    else end_request(0);
	}
}

static int pcd_ioctl(struct inode *inode,struct file *file,
		    unsigned int cmd, unsigned long arg)

/* we currently support only the EJECT ioctl. */

{	int unit = DEVICE_NR(inode->i_rdev);
	if  ((unit >= PCD_UNITS) || (!PCD.present)) return -ENODEV;

	switch (cmd) {
            case CDROMEJECT: if (PCD.access == 1) {
				pcd_eject(unit);
				return 0;
			     }
	    default:
	        return -EINVAL;
	}
}

static void pcd_release (struct inode *inode, struct file *file)

{	kdev_t	devp;
	int 	unit;

	devp = inode->i_rdev;
	unit = DEVICE_NR(devp);

	if  ((unit >= PCD_UNITS) || (PCD.access <= 0)) 
			return;
       
	PCD.access--;

	if (!PCD.access) { 
		fsync_dev(devp);

                invalidate_inodes(devp);

	    	invalidate_buffers(devp);
	    	pcd_unlock(unit);

	}

	MOD_DEC_USE_COUNT;

}

#ifdef MODULE

/* Glue for modules ... */

int	init_module(void)

{	int	err;

#ifdef PARIDE_JUMBO
       { extern paride_init();
         paride_init();
       } 
#endif

	err = pcd_init();

	return err;
}

void	cleanup_module(void)

{	int unit;

	unregister_blkdev(MAJOR_NR,name);
	
        for (unit=0;unit<PCD_UNITS;unit++) 
           if (PCD.present) pi_release(PI);
}

#endif

#define WR(c,r,v)       pi_write_regr(PI,c,r,v)
#define RR(c,r)         (pi_read_regr(PI,c,r))

static int pcd_wait( int unit, int go, int stop, char * fun, char * msg )

{	int j, r, e, s, p;

	j = 0;
	while ((((r=RR(1,6))&go)||(stop&&(!(r&stop))))&&(j++<PCD_SPIN))
		udelay(PCD_DELAY);

	if ((r&(IDE_ERR&stop))||(j>=PCD_SPIN)) {
	   s = RR(0,7);
	   e = RR(0,1);
	   p = RR(0,2);
       	   if (j >= PCD_SPIN) e |= 0x100;
           if (fun) printk("%s: %s %s: alt=0x%x stat=0x%x err=0x%x"
			   " loop=%d phase=%d\n",
			    PCD.name,fun,msg,r,s,e,j,p);
	   return (s<<8)+r;
	}
	return 0;
}

static int pcd_command( int unit, char * cmd, int dlen, char * fun )

{	pi_connect(PI);

        WR(0,6,0xa0 + 0x10*PCD.drive);

	if (pcd_wait(unit,IDE_BUSY|IDE_DRQ,0,fun,"before command")) {
		pi_disconnect(PI);
		return -1;
	}

        WR(0,4,dlen % 256);
        WR(0,5,dlen / 256);
        WR(0,7,0xa0);          /* ATAPI packet command */

        if (pcd_wait(unit,IDE_BUSY,IDE_DRQ,fun,"command DRQ")) {
		pi_disconnect(PI);
		return -1;
	}

        if (RR(0,2) != 1) {
           printk("%s: %s: command phase error\n",PCD.name,fun);
	   pi_disconnect(PI);
           return -1;
        }

	pi_write_block(PI,cmd,12);

	return 0;
}

static int pcd_completion( int unit, char * buf,  char * fun )

{	int r, d, p, n, k, j;

	r = -1; k = 0; j = 0;

	if (!pcd_wait(unit,IDE_BUSY,IDE_DRQ|IDE_READY|IDE_ERR,
						fun,"completion")) {
	    r = 0;
	    while (RR(0,7)&IDE_DRQ) {
	        d = (RR(0,4)+256*RR(0,5));
	        n = ((d+3)&0xfffc);
	        p = RR(0,2)&3;

	        if ((p == 2) && (n > 0) && (j == 0)) {
		    pi_read_block(PI,buf,n);
	            if (verbose > 1) 
			printk("%s: %s: Read %d bytes\n",PCD.name,fun,n);
		    r = 0; j++;
	        } else {
		    if (verbose > 1) 
		        printk("%s: %s: Unexpected phase %d, d=%d, k=%d\n",
					PCD.name,fun,p,d,k);
		    if ((verbose < 2) && !pcd_warned) {
	               	pcd_warned = 1;
			printk("%s: WARNING: ATAPI phase errors\n",PCD.name);
			}
		    udelay(1000);
	        } 
		if (k++ > PCD_TMO) {
			printk("%s: Stuck DRQ\n",PCD.name);
			break;
		}
	        if (pcd_wait(unit,IDE_BUSY,IDE_DRQ|IDE_READY|IDE_ERR,
				fun,"completion")) { 
			r = -1;
			break;
		}
	    }
	}
	
	pi_disconnect(PI); 

	return r;
}

static void pcd_req_sense( int unit, char *fun )

{	char	rs_cmd[12] = { 0x03,0,0,0,16,0,0,0,0,0,0,0 };
	char	buf[16];
	int 	r;

	r = pcd_command(unit,rs_cmd,16,"Request sense");
	udelay(1000);
	if (!r) pcd_completion(unit,buf,"Request sense");

	PCD.last_sense = -1;
	if (!r) {
            if (fun) printk("%s: %s: Sense key: %x, ASC: %x, ASQ: %x\n",
	                       PCD.name,fun,buf[2]&0xf,buf[12],buf[13]);
	    PCD.last_sense = (buf[2]&0xf) | ((buf[12]&0xff)<<8)
                                          | ((buf[13]&0xff)<<16) ;
        } 
}

static int pcd_atapi( int unit, char * cmd, int dlen, char * buf, char * fun )

{	int r;

	r = pcd_command(unit,cmd,dlen,fun);
	udelay(1000);
	if (!r) r = pcd_completion(unit,buf,fun);
	if (r) pcd_req_sense(unit,fun);
	
	return r;
}

#define DBMSG(msg)	((verbose>1)?(msg):NULL)

static void pcd_lock(int unit)

{	char	lo_cmd[12] = { 0x1e,0,0,0,1,0,0,0,0,0,0,0 };
	char	cl_cmd[12] = { 0x1b,0,0,0,3,0,0,0,0,0,0,0 };

	pcd_atapi(unit,cl_cmd,0,pcd_scratch,DBMSG("cd1")); 
	pcd_atapi(unit,cl_cmd,0,pcd_scratch,DBMSG("cd2"));
	pcd_atapi(unit,cl_cmd,0,pcd_scratch,DBMSG("cd3"));
	pcd_atapi(unit,cl_cmd,0,pcd_scratch,DBMSG("cd4"));
	pcd_atapi(unit,cl_cmd,0,pcd_scratch,"close door");

        pcd_atapi(unit,lo_cmd,0,pcd_scratch,DBMSG("ld"));
        pcd_atapi(unit,lo_cmd,0,pcd_scratch,"lock door");
}

static void pcd_unlock( int unit )

{	char	un_cmd[12] = { 0x1e,0,0,0,0,0,0,0,0,0,0,0 };

	pcd_atapi(unit,un_cmd,0,pcd_scratch,"unlock door");
}

static void pcd_eject( int unit)

{	char	ej_cmd[12] = { 0x1b,0,0,0,2,0,0,0,0,0,0,0 };

	pcd_unlock(unit);
	pcd_atapi(unit,ej_cmd,0,pcd_scratch,"eject");
}

#define PCD_RESET_TMO	100		/* in tenths of a second */

static void pcd_sleep( int cs )

{       current->state = TASK_INTERRUPTIBLE;
        current->timeout = jiffies + cs;
        schedule();
}

static int pcd_reset( int unit )

/* the ATAPI standard actually specifies the contents of all 7 registers
   after a reset, but the specification is ambiguous concerning the last
   two bytes, and different drives interpret the standard differently.
*/

{	int	i, k, flg;
	int	expect[5] = {1,1,1,0x14,0xeb};

	pi_connect(PI);
	WR(0,6,0xa0 + 0x10*PCD.drive);
	WR(0,7,8);

	pcd_sleep(2);  		/* delay a bit*/

	k = 0;
	while ((k++ < PCD_RESET_TMO) && (RR(1,6)&IDE_BUSY))
		pcd_sleep(10);

	flg = 1;
	for(i=0;i<5;i++) flg &= (RR(0,i+1) == expect[i]);

	if (verbose) {
		printk("%s: Reset (%d) signature = ",PCD.name,k);
		for (i=0;i<5;i++) printk("%3x",RR(0,i+1));
		if (!flg) printk(" (incorrect)");
		printk("\n");
	}
	
	pi_disconnect(PI);
	return flg-1;	
}

static int pcd_ready_wait( int unit, int tmo )

{       char    tr_cmd[12] = {0,0,0,0,0,0,0,0,0,0,0,0};
        int     k, p;

        k = 0;
        while (k < tmo) {
          PCD.last_sense = 0;
          pcd_atapi(unit,tr_cmd,0,NULL,DBMSG("test unit ready"));
          p = PCD.last_sense;
          if (!p) return 0;
	  if (!(((p & 0xffff) == 0x0402)||((p & 0xff) == 6))) return p;
          k++;
          pcd_sleep(100);
        }
        return 0x000020;        /* timeout */
}

static int pcd_check_media( int unit )

{	char    rc_cmd[12] = { 0x25,0,0,0,0,0,0,0,0,0,0,0};

	pcd_ready_wait(unit,PCD_READY_TMO);
	return (pcd_atapi(unit,rc_cmd,8,pcd_scratch,DBMSG("check media")));
}

static int pcd_identify( int unit, char * id )

{	int k, s;
	char   id_cmd[12] = {0x12,0,0,0,36,0,0,0,0,0,0,0};

	pcd_bufblk = -1;

        s = pcd_atapi(unit,id_cmd,36,pcd_buffer,"identify");

	if (s) return -1;
	if ((pcd_buffer[0] & 0x1f) != 5) {
	  if (verbose) printk("%s: %s is not a CDrom\n",
			PCD.name,PCD.drive?"Slave":"Master");
	  return -1;
	}
	for (k=0;k<16;k++) id[k] = pcd_buffer[16+k]; id[16] = 0;
	k = 16; while ((k >= 0) && (id[k] <= 0x20)) { id[k] = 0; k--; }

	printk("%s: %s: %s\n",PCD.name,PCD.drive?"Slave":"Master",id);

	return 0;
}

static int pcd_probe( int unit, int ms, char * id )

/*	returns  0, with id set if drive is detected
	        -1, if drive detection failed
*/

{	if (ms == -1) {
            for (PCD.drive=0;PCD.drive<=1;PCD.drive++)
	       if (!pcd_reset(unit) && !pcd_identify(unit,id)) 
		  return 0;
	} else {
	    PCD.drive = ms;
            if (!pcd_reset(unit) && !pcd_identify(unit,id)) 
		return 0;
	}
	return -1;
}

static int pcd_detect( void )

{	char    id[18];
	int	k, unit;

	printk("%s: %s version %s, major %d, nice %d\n",
		name,name,PCD_VERSION,major,nice);

	k = 0;
	if (pcd_drive_count == 0) {  /* nothing spec'd - so autoprobe for 1 */
	    unit = 0;
	    if (pi_init(PI,1,-1,-1,-1,-1,-1,pcd_buffer,
                     PI_PCD,verbose,PCD.name)) {
		if (!pcd_probe(unit,-1,id)) {
			PCD.present = 1;
			k++;
		} else pi_release(PI);
	    }

	} else for (unit=0;unit<PCD_UNITS;unit++) if (DU[D_PRT])
	    if (pi_init(PI,0,DU[D_PRT],DU[D_MOD],DU[D_UNI],
                        DU[D_PRO],DU[D_DLY],pcd_buffer,PI_PCD,verbose,
                        PCD.name)) {
		if (!pcd_probe(unit,DU[D_SLV],id)) {
                        PCD.present = 1;
                        k++;
                } else pi_release(PI);
            }

	if (k) return 0;
	
	printk("%s: No CDrom drive found\n",name);
	return -1;
}

/* I/O request processing */

static int pcd_ready( void )

{	int	unit = pcd_unit;

	return (((RR(1,6)&(IDE_BUSY|IDE_DRQ))==IDE_DRQ)) ;
}

static void pcd_transfer( void )

{	int	k, o;

	while (pcd_count && (pcd_sector/4 == pcd_bufblk)) {
		o = (pcd_sector % 4) * 512;
		for(k=0;k<512;k++) pcd_buf[k] = pcd_buffer[o+k];
		pcd_count--;
		pcd_buf += 512;
		pcd_sector++;
	}
}

static void pcd_start( void )

{	int	unit = pcd_unit;
	int	b, i;
	char	rd_cmd[12] = {0xa8,0,0,0,0,0,0,0,0,1,0,0};
	long    saved_flags;

	pcd_bufblk = pcd_sector / 4;
        b = pcd_bufblk;
	for(i=0;i<4;i++) { 
	   rd_cmd[5-i] = b & 0xff;
	   b = b >> 8;
	}

	if (pcd_command(unit,rd_cmd,2048,"read block")) {
		pcd_bufblk = -1; 
		spin_lock_irqsave(&io_request_lock,saved_flags);
		pcd_busy = 0;
		end_request(0);
		do_pcd_request();
		spin_unlock_irqrestore(&io_request_lock,saved_flags);
		return;
	}

	udelay(1000);

	ps_set_intr(do_pcd_read_drq,pcd_ready,PCD_TMO,nice);

}

static void do_pcd_read( void )


{	int	unit = pcd_unit;
	long    saved_flags;

	pcd_busy = 1;
	pcd_retries = 0;
	pcd_transfer();
	if (!pcd_count) {
		spin_lock_irqsave(&io_request_lock,saved_flags);
		end_request(1);
		pcd_busy = 0;
		do_pcd_request();
		spin_unlock_irqrestore(&io_request_lock,saved_flags);
		return;
	}

	pi_do_claimed(PI,pcd_start);
}

static void do_pcd_read_drq( void )

{	int	unit = pcd_unit;
	long    saved_flags;

	if (pcd_completion(unit,pcd_buffer,"read block")) {
                if (pcd_retries < PCD_RETRIES) {
                        udelay(1000);
                        pcd_retries++;
			pi_do_claimed(PI,pcd_start);
                        return;
                        }
		spin_lock_irqsave(&io_request_lock,saved_flags);
		pcd_busy = 0;
		pcd_bufblk = -1;
		end_request(0);
		do_pcd_request();
		spin_unlock_irqrestore(&io_request_lock,saved_flags);
		return;
	}

	do_pcd_read();
	spin_lock_irqsave(&io_request_lock,saved_flags);
	do_pcd_request();
	spin_unlock_irqrestore(&io_request_lock,saved_flags); 
}
	
/* end of pcd.c */

