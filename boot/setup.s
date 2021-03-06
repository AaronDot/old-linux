!
!
!	/*???ڶ?ȡ??????Ӳ?????ò??������??ں?ģ??system  ?ƶ????ʵ????ڴ?λ?ô?*/
!
!	setup.s		(C) 1991 Linus Torvalds
!
! setup.s is responsible for getting the system data from the BIOS,
! and putting them into the appropriate places in system memory.
! both setup.s and system has been loaded by the bootblock.
!
! This code asks the bios for memory/disk/other parameters, and
! puts them in a "safe" place: 0x90000-0x901FF, ie where the
! boot-block used to be. It is then up to the protected mode
! system to read them from there before the area is overwritten
! for buffer-blocks.
!

! NOTE! These had better be the same as in bootsect.s!
/*??????Щ???????ú?bootsect.s  ????ͬ*/

INITSEG  = 0x9000	! we move boot here - out of the way
SYSSEG   = 0x1000	! system loaded at 0x10000 (65536).
SETUPSEG = 0x9020	! this is the current segment

.globl begtext, begdata, begbss, endtext, enddata, endbss
.text
begtext:
.data
begdata:
.bss
begbss:
.text

entry start
start:

! ok, the read went well so we get current cursor position and save it for
! posterity.

	/*????BIOS  ?ж?0x10  ??ȡ????λ?ã?????*/
	mov	ax,#INITSEG	! this is done in bootsect already, but...
	mov	ds,ax
	mov	ah,#0x03	! read cursor pos
	xor	bh,bh
	int	0x10		! save it in known place, con_init fetches
	mov	[0],dx		! it from 0x90000.

! Get memory size (extended mem, kB)

	/*????BIOS  ?ж?0x15  ??ȡ��չ?ڴ???С*/
	mov	ah,#0x88
	int	0x15
	mov	[2],ax		/*????ֵ??????0x90002??1????*/

! Get video-card data:

	/*????BIOS  ?ж?0x10  ??ȡ??ʾ????ǰ??ʾģʽ*/
	mov	ah,#0x0f
	int	0x10
	mov	[4],bx		! bh = display page				/*??ǰҳ0c90004*/
	mov	[6],ax		! al = video mode, ah = window width	/*0x90006--??ʾģʽ??0x90007---?ַ?????*/

! check for EGA/VGA and some config parameters

	/*????BIOS  ?ж?0x10  ??????ʾ??ʽ??ȡ????*/
	mov	ah,#0x12
	mov	bl,#0x10
	int	0x10
	mov	[8],ax
	mov	[10],bx		/*0x9000A = ??װ????ʾ?ڴ棬0x9000B = ??ʾ״̬(??ɫ/??ɫ)*/
	mov	[12],cx		/*0x9000C = ??ʾ?????Բ???*/

! Get hd0 data

	/*ȡ??һ??Ӳ?̵???Ϣds  =  ?ε?ַ??si  =  ????ƫ?Ƶ?ַ*/
	mov	ax,#0x0000
	mov	ds,ax			
	lds	si,[4*0x41]		/*ȡ?ж???��0x41   ??ֵ??Ҳ??hd0  ???????ĵ?ַds:si*/
	mov	ax,#INITSEG
	mov	es,ax
	mov	di,#0x0080		/*??????Ŀ?ĵ?ַ:   0x9000:0x0080  =  es:si*/
	mov	cx,#0x10			/*????16?ֽ?--???ĳ???*/
	rep
	movsb

! Get hd1 data

	mov	ax,#0x0000
	mov	ds,ax
	lds	si,[4*0x46]		/*ȡ?ж???��0x46   ??ֵ??Ҳ??hd1  ???????ĵ?ַds:si*/
	mov	ax,#INITSEG
	mov	es,ax
	mov	di,#0x0090		/*??????Ŀ?ĵ?ַ:   0x9000:0x0090  =  es:si*/
	mov	cx,#0x10
	rep
	movsb

! Check that there IS a hd1 :-)

	/*????BIOS  ?ж?0x13  ??ȡ??????
		?????Ƿ??е?2  ??Ӳ?̣?û?оͰѵ?2????????*/
	mov	ax,#0x01500
	mov	dl,#0x81
	int	0x13
	jc	no_disk1
	cmp	ah,#3			/*??Ӳ?????*/
	je	is_disk1
no_disk1:
	mov	ax,#INITSEG		/*??2 ??Ӳ?̲????ڣ??򽫵?2 ??????0*/
	mov	es,ax
	mov	di,#0x0090
	mov	cx,#0x10
	mov	ax,#0x00
	rep
	stosb
is_disk1:

! now we want to move to protected mode ...
	/*???뱣??ģʽ*/

	cli			! no interrupts allowed !	/*?Ӵ˿?ʼ???????ж????????????????????*/

! first we move the system to it's rightful place

	/*??????system  ģ????0x10000  ?ƶ???0x00000  λ??*/
	mov	ax,#0x0000
	cld			! 'direction'=0, movs moves forward		/*??????־λ??0*/
do_move:
	mov	es,ax		! destination segment
	add	ax,#0x1000
	cmp	ax,#0x9000					/*????һ???ƶ??? ????*/
	jz	end_move
	mov	ds,ax		! source segment
	sub	di,di
	sub	si,si
	mov 	cx,#0x8000		/*64kb?ֽ?*/
	rep
	movsw
	jmp	do_move

! then we load the segment descriptors

/*???ض???????
	??????32  λ????ģʽ?Ĳ???
	?ڽ??뱣??ģʽ??????֮ǰ??????Ҫ???????ú?Ҫʹ?õĶ?????????
	??????ȫ?????????????ж?????????*/

end_move:
	mov	ax,#SETUPSEG	! right, forgot this at first. didn't work :-)
	mov	ds,ax			/*ds  ָ?򱾳????Ķ?*/
	lidt	idt_48		! load idt with 0,0						/*????IDT  ?Ĵ???*/
	lgdt	gdt_48		! load gdt with whatever appropriate		/*????GDT  ?Ĵ???*/

! that was painless, now we enable A20

	call	empty_8042					/*????8042  ״̬?Ĵ??����ȴ????뻺??????
										ֻ?е????뻺????Ϊ??ʱ?ſ??Զ???ִ??д????*/
	mov	al,#0xD1		! command write	/*0xD1----Ҫд???ݵ?*/
	out	#0x64,al						/*8042 ??P2  ?˿ڣ?λ1  ????A20  ?ߵ?ѡͨ
										????Ҫд??0x60  ??*/
	call	empty_8042					/*?ȴ????뻺?????գ????????Ƿ񱻽???*/
	mov	al,#0xDF		! A20 on			/*ѡͨA20  ??ַ?ߵĲ???*/
	out	#0x60,al
	call	empty_8042					/*????ʱ???뻺????Ϊ?գ?????ʾA20  ??ַ???Ѿ?ѡͨ*/

! well, that went ok, I hope. Now we have to reprogram the interrupts :-(
! we put them right after the intel-reserved hardware interrupts, at
! int 0x20-0x2F. There they won't mess up anything. Sadly IBM really
! messed this up with the original PC, and they haven't been able to
! rectify it afterwards. Thus the bios puts interrupts at 0x08-0x0f,
! which is used for the internal hardware interrupts as well. We just
! have to reprogram the 8259's, and it isn't fun.

	mov	al,#0x11		! initialization sequence
	out	#0x20,al		! send it to 8259A-1				/*??оƬ*/
	.word	0x00eb,0x00eb		! jmp $+2, jmp $+2	/*'$'  ??ʾ??ǰָ???ĵ?ַ??????ʱ????*/
	out	#0xA0,al		! and to 8259A-2					/*??оƬ*/
	.word	0x00eb,0x00eb
	
	/*Linux ϵͳӲ???жϺű????óɴ?0x20  ??ʼ??*/
	mov	al,#0x20		! start of hardware int's (0x20)
	out	#0x21,al		/*????оƬICW2  ?????֣???????ʼ?жϺ?*/
	.word	0x00eb,0x00eb
	mov	al,#0x28		! start of hardware int's 2 (0x28)
	out	#0xA1,al		/*?ʹ?оƬICW2  ?????֣???оƬ????ʼ?жϺ?*/
	.word	0x00eb,0x00eb
	mov	al,#0x04		! 8259-1 is master
	out	#0x21,al		/*????оƬCW3  ?????֣???оƬ??IR2   ��?Ӵ?оƬ??INT*/
	.word	0x00eb,0x00eb
	mov	al,#0x02		! 8259-2 is slave
	out	#0xA1,al		/*?ʹ?оƬICW3  ?????֣???оƬ??INT  ��?ӵ???оƬ??IR2????*/
	.word	0x00eb,0x00eb
	mov	al,#0x01		! 8086 mode for both
	out	#0x21,al		/*????оƬCW4  ?????֣?8086  ģʽ*/
	.word	0x00eb,0x00eb
	out	#0xA1,al		/*?ʹ?оƬCW4  ?????֣?*/
	.word	0x00eb,0x00eb
	mov	al,#0xFF		! mask off all interrupts for now
	out	#0x21,al		/*?��???оƬ?????ж?????*/
	.word	0x00eb,0x00eb
	out	#0xA1,al		/*?��δ?оƬ?????ж?????*/

! well, that certainly wasn't fun :-(. Hopefully it works, and we don't
! need no steenking BIOS anyway (except for the initial loading :-).
! The BIOS-routine wants lots of unnecessary data, and it's less
! "interesting" anyway. This is how REAL programmers do it.
!
! Well, now's the time to actually move into protected mode. To make
! things as simple as possible, we do no register set-up or anything,
! we let the gnu-compiled 32-bit programs do that. We just jump to
! absolute address 0x00000, in 32-bit protected mode.

	mov	ax,#0x0001	! protected mode (PE) bit		/*????ģʽ????λ(PE)*/
	lmsw	ax		! This is it!					/???ػ???״̬??*/
	jmpi	0,8		! jmp offset 0 of segment 8 (cs)		/*?μ???ת??????ˢ??CPU   ??ǰָ??????
													??????head.s   ??????ʼ??????ִ????ȥ
													8:  ??ѡ??????ָ??????Ҫ??????????
													0:  ???????ڵ?ƫ??ֵ*/

! This routine checks that the keyboard command queue is empty
! No timeout is used - if this hangs there is something wrong with
! the machine, and we probably couldn't proceed anyway.

/*ֻ???????뻺????Ϊ??ʱ(????λ1  =  0)???ſ??Զ???д??????*/
empty_8042:
	.word	0x00eb,0x00eb
	in	al,#0x64	! 8042 status port			/*??AT???̿?????״̬?Ĵ???*/
	test	al,#2		! is input buffer full?	/*????λ1   ?????뻺?????Ƿ???*/
	jnz	empty_8042	! yes - loop
	ret


/*ȫ?????????���ʼ??*/
gdt:
	.word	0,0,0,0		! dummy		/*??һ???????�����??*/

/*??GDT  ???У???????ƫ??��??0x08   ???ں˴?????ѡ??????ֵ*/
	.word	0x07FF		! 8Mb - limit=2047 (2048*4096=8Mb)
	.word	0x0000		! base address=0
	.word	0x9A00		! code read/exec
	.word	0x00C0		! granularity=4096, 386

/*??GDT  ???У???????ƫ??��??0x10   ???ں????ݶ?ѡ??????ֵ*/
	.word	0x07FF		! 8Mb - limit=2047 (2048*4096=8Mb)
	.word	0x0000		! base address=0
	.word	0x9200		! data read/write
	.word	0x00C0		! granularity=4096, 386


/*?????ж?????????
	CPU  Ҫ???ڽ??뱣??ģʽ֮ǰҪ????IDT  ?���
	????????????һ?����?Ϊ0   ?Ŀձ?*/
idt_48:
	.word	0			! idt limit=0
	.word	0,0			! idt base=0L

/*????ȫ????????*/
gdt_48:
	.word	0x800		! gdt limit=2048, 256 GDT entries
	.word	512+gdt,0x9	! gdt base = 0X9xxxx
	
.text
endtext:
.data
enddata:
.bss
endbss:
