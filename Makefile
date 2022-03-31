all:
	yasm -p gas -f bin -o $(boot).tmp $(boot).asm
	dd bs=31744 skip=1 if=$(boot).tmp of=$(boot).bin
	g++ -ffreestanding -m32 -o $(ker).o -c $(ker).cpp
	ld --oformat binary -Ttext 0x10000 -o $(ker).bin --entry=kmain -m elf_i386 $(ker).o 
	qemu -fda $(boot).bin -fdb $(ker).bin

