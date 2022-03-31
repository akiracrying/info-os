# To boot, first write this:
#
# yasm -p gas -f bin -o bootsect.tmp bootsect.asm
# dd bs=31744 skip=1 if=bootsect.tmp of=bootsect.bin
# g++ -ffreestanding -m32 -o kernel.o -c kernel.cpp
# ////// ld --ignore-unresolved-symbol _GLOBAL_OFFSET_TABLE_ --oformat binary -Ttext 0x10000 -o kernel.bin --entry=kmain -m elf_i386 kernel.o 
# ////// This is trash for ubuntu (ubuntu == govno mocha ebal ee rot)
# ld --oformat binary -Ttext 0x10000 -o kernel.bin --entry=kmain -m elf_i386 kernel.o 
#
# Then, to boot:
#
# qemu -fda bootsect.bin -fdb kernel.bin
#
# Fine! You got it! :)

.code16
.org 0x7c00

start:
    movw %cs, %ax
	movw %ax, %ds	# Начальная инициализация регистров
	movw %ax, %ss	
	movw $start, %sp

    movw $0x0003, %ax # Очистка экрана
    int $0x10         # Прерывание BIOS для вывода экрана

movw $0x1000, %ax  # Адрес ядра
movw %ax, %es

movw $0x0000, %bx  # Начальное положение при загрузке (из-за архетиктуры Линукса)
movb $1, %dl # Номер диска
movb $0, %ch # Номер цилиндра
movb $0, %dh # Номер головки
movb $1, %cl # Номер Сегмента
movb $18, %al # Количество секторов
movb $0x02, %ah
int $0x13

movw $0x2400, %bx # Допсектора (в целом, из-за конечного размера ядра не нужны)
movb $1, %dl
movb $0, %ch
movb $1, %dh
movb $1, %cl
movb $1, %al
movb $2, %ah
int $0x13


movb $0x02, %ah # Прерывания для получения значений в функции loadtime
int $0x1A  

lgdt gdt_info  # Загрузка размера и адреса таблицы дескрипторов

inb $0x92, %al # Включение адресной линии А20
orb $2, %al
outb %al, $0x92 # Работа по переходу в защищённый режим

cli # Отключение прерываний
movl %cr0, %eax # Установка бита PE регистра CR0 - процессор перейдет в защищенный режим
orb $1, %al
movl %eax, %cr0
ljmp $0x8, $protected_mode # "Дальний" переход для загрузки корректной информации в cs, архитектурные особенности не позволяют этого сделать напрямую)


.code32
protected_mode:
    movw $0x10, %ax
    movw %ax, %es
    movw %ax, %ds
    movw %ax, %ss
    call 0x10000

gdt:
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 # Нулевой дескриптор
    .byte 0xff, 0xff, 0x00, 0x00, 0x00, 0x9A, 0xCF, 0x00 # Сегмент кода: base=0, size=4Gb, P=1, DPL=0, S=1(user), Type=1(code), Access=00A, G=1, B=32bit
    .byte 0xff, 0xff, 0x00, 0x00, 0x00, 0x92, 0xCF, 0x00 # Сегмент данных: base=0, size=4Gb, P=1, DPL=0, S=1(user), Type=0(data), Access=0W0, G=1, B=32bit


gdt_info: # Данные о таблице GDT (размер, положение в памяти)
    .word gdt_info - gdt # Размер таблицы (2 байта) 
    .word gdt, 0         # 32-битный физический адрес таблицы

    
.zero (512 - (. - start) - 2)
.byte 0x55, 0xAA 

