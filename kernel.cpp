
__asm("jmp kmain"); // Эта инструкция обязательно должна быть первой, т.к. этот код компилируется в бинарный,
                    // и загрузчик передает управление по адресу первой инструкции бинарногообраза ядра ОС.

#define VIDEO_BUF_PTR (0xb8000) // Видеобуфер
#define IDT_TYPE_INTR (0x0E) // Константы
#define IDT_TYPE_TRAP (0x0F) // прерываний
#define GDT_CS (0x8)
#define PIC1_PORT (0x20)
#define CURSOR_PORT (0x3D4)
#define VIDEO_WIDTH (80) // Ширина 
#define WHITE (0x07) // Код белого цвета

#define BACKSPACE 14
#define SPACE 32     // Коды спецклавиш
#define ENTER 28

unsigned char LINE = 0, COLUMN = 0; // Линия и колонка в которую сейчас поместится символ
unsigned char is_shift = 0x00;
char console_buf[100]; // Буфер символов, считываемы с клавиатуры
int len = 0; // Длина буфера символов
int ticks = 0; // Тики

// Различные переменные времени:
char hours;
char mins;      // loadtime
char secs;

char cur_hours;
char cur_mins;      // curtime
char cur_secs;

int up_hours;
int up_mins;        // uptime
int up_secs;

short ISUP = 0;   // Вспомогательные переменные для вывода незначащего нуля
short ISTIME = 0;

enum iterrup_stats{
    KEYBOARD = 0xFF ^ 0x02,   // Коды прерываний таймера и клавитуры
    TICK_TIMER = 0xFF ^ 0x01 
};
int cur_stat;                 

// Структура описывает данные об обработчике прерывания
struct idt_entry{
    unsigned short base_lo;// Младшие биты адреса обработчика
    unsigned short segm_sel;// Селектор сегмента кода
    unsigned char always0;// Этот байт всегда 0
    unsigned char flags;// Флаги тип. Флаги: P, DPL, Типы - это константы - IDT_TYPE
    unsigned short base_hi; // Старшие биты адреса обработчика
} __attribute__((packed)); // Выравнивание запрещено

struct idt_ptr{
    unsigned short limit;
    unsigned int base;
} __attribute__((packed));

struct idt_entry g_idt[256];// Реальная таблица IDT
struct idt_ptr g_idtp;// Описатель таблицы для команды lidt
// Пустой обработчик прерываний. Другие обработчики могут быть реализованы по этому шаблону

const unsigned char SYMBOLS[] = "**1234567890-=-\0qwertyuiop[]\0\0asdfghjkl;'\0\0\0zxcvbnm,./\0\0\0";// PS2 таблица

typedef void (*intr_handler)();

// Прототипы реализованных функций
void line_func();
void enter_pressed();
void backspace_pressed();
void cursor_moveto();
void write_sym(const unsigned char);
void on_key(unsigned char key);
void default_intr_handler();
void intr_reg_handler(int, unsigned short , unsigned short , intr_handler );
void intr_init();
void intr_start();
void intr_enable();
void intr_disable();
short funtions();
static inline unsigned char inb (unsigned short);
static inline void outb (unsigned short, unsigned char );
void keyb_process_keys();
void keyb_handler();
void keyb_init();
void out_str(int , const char*, unsigned int, unsigned int);
void clear();
void ticks_handler();
int cmp(char*, char*, int);
void out_num(int, int, int);
char int_to_char(long n);
static int bcd_decimal(char);
void curtime_func();
void ticks_init();
void turn_on_ticks();
void turn_off_ticks();

unsigned char line_inc(){
    COLUMN = 2;
    LINE++;
    return LINE;
}
// Функция перевода из двоичнодесятичной в десятичную
static int bcd_decimal(char bcd){
    return bcd - 6 * (bcd >> 4);
}       
// Преобразлвание int в char
char int_to_char(long n) {
	char res;
	if (n < 10) {
		res = char(n + '0');
	}
	else {
		res = char(n + 'a' - 10);
	}
	return res;
}
// Сравнение двух массивов char
int cmp(char *first, char *second, int n) {
    int cmp_tmp = 0;
	for (int i = 0; i < n; i++){
		if (first[i] != second[i]) {
			return 0;
		}else{
            cmp_tmp++;
        }
	}
    if(cmp_tmp == len){
	    return 1;
    }else{
        return 0;
    }
}
// Очистка экрана
void clear(){
    unsigned char* video_buf = (unsigned char*)VIDEO_BUF_PTR;
    for (unsigned short i = 0; i < 48 * VIDEO_WIDTH; i += 2)
        video_buf[i] = 0;
    LINE = 0;
    COLUMN = 2;                                         
    cursor_moveto();
}
// Консольные команды
short funtions(){

    if(cmp(console_buf, "info", 4)){
        out_str(WHITE, "InfoOS is made by Tsvetkov Timofey, 2022", LINE++, COLUMN);
        out_str(WHITE, "Compiler: gcc", LINE++, COLUMN);
        out_str(WHITE, "x86 Assembly, YASM, AT&T", LINE++, COLUMN);
    }else if(cmp(console_buf, "help", 4)){
        out_str(WHITE, "info - shows information about system", LINE++, COLUMN);
        out_str(WHITE, "clear - clearing the console", LINE++, COLUMN);
        out_str(WHITE, "ticks - amount of ticks passed from the boot", LINE++, COLUMN);
        out_str(WHITE, "loadtime - up-to-date time during BIOS loading ", LINE++, COLUMN);
        out_str(WHITE, "curtime - current time", LINE++, COLUMN);
        out_str(WHITE, "uptime - total working time", LINE++, COLUMN);

    }else if(cmp(console_buf, "clear", 5)){
        clear();
    }else if(cmp(console_buf, "ticks", 5)){

        out_str(WHITE, "Passed", LINE, COLUMN); 
        short tmp = 0;
        int ticks_tmps = ticks;
        while(ticks_tmps){
            tmp++;
            ticks_tmps/=10;
        }
        COLUMN+=7;
        out_num(ticks, COLUMN, LINE);
        COLUMN+=1+tmp;
        out_str(WHITE, "ticks", LINE, COLUMN); 
        line_inc();

    }else if(cmp(console_buf, "loadtime", 8)){
        ISTIME=1;
        out_str(WHITE, "InfoOS loaded at: ",LINE, COLUMN); 
        COLUMN+=18;
        char tmp_h = hours;
        switch(tmp_h){
            case(24):
                tmp_h = 0;
                break;
            case(25):
                tmp_h = 1;
                break;
            case 26:
                tmp_h = 2;
                break;
        }
        out_num(tmp_h, COLUMN, LINE);
        COLUMN+=2;
        write_sym(':');
        COLUMN-=2;
        out_num(mins, COLUMN+2,LINE);
        COLUMN+=4;
        write_sym(':');
        COLUMN-=4;
        out_num(secs, COLUMN + 4, LINE);
        line_inc();
        ISTIME = 0;
    }else if(cmp(console_buf, "curtime", 7)){
        ISTIME = 1;
        curtime_func();
        out_str(WHITE, "Current time is: ",LINE, COLUMN); 
        COLUMN+=17;
        char tmp_h = cur_hours;
        switch(tmp_h){
            case(24):
                tmp_h = 0;
                break;
            case(25):
                tmp_h = 1;
                break;
            case 26:
                tmp_h = 2;
                break;
        }
        out_num(tmp_h, COLUMN, LINE);
        COLUMN+=2;
        write_sym(':');
        COLUMN-=2;
        out_num(cur_mins, COLUMN+2,LINE);
        COLUMN+=4;
        write_sym(':');
        COLUMN-=4;
        out_num(cur_secs, COLUMN + 4, LINE);
        line_inc();
        ISTIME = 0;

    }else if(cmp(console_buf, "uptime", 6)){
        ISUP = 1;
        curtime_func();
        int up_time_cur = (int)cur_hours*60*60 + (int)cur_mins*60 + (int)cur_secs;
        int up_time_load = (int)hours*60*60 + (int)mins*60 + (int)secs;
        int alltime = up_time_cur - up_time_load; // Время между текущим и загрузки
        up_hours = alltime / 3600;alltime = alltime%3600;
        up_mins = alltime/60; alltime = alltime%60;
        up_secs = alltime;
        int tmp, tmp2, tmp3, tmp_len = 0, tmp2_len = 0,tmp3_len = 0;
        tmp = up_mins;
        tmp2 = up_secs;
        while(tmp){
            tmp/=10;
            tmp_len++;
        }
        while(tmp2){
            tmp2/=10;
            tmp2_len++;
        }
        while(tmp3){
            tmp3/=10;
            tmp3_len++;
        }
            out_str(WHITE, "Uptime is ",LINE, COLUMN); 
            COLUMN+=10;
            if(up_hours!=0){
                out_num(up_hours, COLUMN, LINE);
                COLUMN+=2+tmp_len;
                if(up_hours == 1){
                    out_str(WHITE, "hour",LINE, COLUMN); 
                }else{
                    out_str(WHITE, "hours and",LINE, COLUMN); 
                }
                COLUMN+=9;
            }
            if(up_mins!=0){
                out_num(up_mins, COLUMN, LINE);
                COLUMN+=2+tmp_len;
                if(up_mins == 1){
                    out_str(WHITE, "minute and",LINE, COLUMN); 
                }else{
                    out_str(WHITE, "minutes and",LINE, COLUMN); 
                }
                COLUMN+=12;
            }
            out_num(up_secs, COLUMN, LINE);
            COLUMN+=1+tmp2_len;
            if(up_secs == 1){
                out_str(WHITE, "second",LINE, COLUMN); 
            }else{
                out_str(WHITE, "seconds",LINE, COLUMN); 
            }
        ISUP = 0;
        up_hours = 0;
        up_mins = 0;
        up_secs = 0;
        line_inc();
    }else{
        out_str(WHITE, "Unknown command", LINE++, COLUMN);
    }
    for (int i = 0; i < len; ++i){
        console_buf[i] = '\0';
    }
    len = 0;
    out_str(0x07, ">", LINE, 0);

    cursor_moveto();
}
// Вывод числа в консоль
void out_num(int value, int col, int ln){
    int i_arr[100];
    char c_arr[100];
    int elems = 0;
    if (value == 0){
        c_arr[0] = '0';
        elems++;
    }else{
        int b = value;
        while(b!=0){
            i_arr[elems] = b%10;
            b/= 10;
            elems++;
        }
        int tmp = elems;
        for(int i = 1; i <= elems; i++){
            c_arr[tmp-1] =int_to_char(i_arr[i - 1]);
            tmp--;
        }
    }
    if(elems == 1 && ISUP == 0 && ISTIME == 1){
        c_arr[1]= c_arr[0];
        c_arr[0] = '0';
        elems++;
    }
    c_arr[elems] = '\0';
    //COLUMN = 2;
    out_str(WHITE, c_arr, ln, col);
}
// Регистрация нажатия клавиши ENTER
void enter_pressed(){
    if(console_buf[0] == '\0'){
        return;
    }
    unsigned char* video_buf = (unsigned char*)VIDEO_BUF_PTR + 2 * (VIDEO_WIDTH * LINE + COLUMN);
    video_buf[0] = COLUMN = 0;   
    COLUMN = 2;       
   // line_func();
    LINE++;                                                       
    cursor_moveto();
    funtions();
}
// Регистрация нажатия клавиши BACKSPACE
void backspace_pressed(){
    if (COLUMN <= 2)
        return;
    else{
        console_buf[len] = '\0';
        len--;
        unsigned char* video_buf = (unsigned char*)VIDEO_BUF_PTR + 2 * (VIDEO_WIDTH * LINE + COLUMN); 
        video_buf[0] = 0;
        COLUMN--;
        cursor_moveto();
    }
}
// Изменение положения кратеки
void cursor_moveto(){
    unsigned char* video_buf = (unsigned char*)VIDEO_BUF_PTR + 2 * (VIDEO_WIDTH * LINE + COLUMN);
    video_buf[0] = ' ';

    unsigned short new_pos = (LINE * VIDEO_WIDTH) + COLUMN;
    outb(CURSOR_PORT, 0x0F);
    outb(CURSOR_PORT + 1, (unsigned char)(new_pos & 0xFF));
    outb(CURSOR_PORT, 0x0E);
    outb(CURSOR_PORT + 1, (unsigned char)( (new_pos >> 8) & 0xFF));
}
// Вывод одного символа после нажантия клавиши
void write_sym(const unsigned char outwrite_sym){
    if (outwrite_sym == '\0')
        return;
    console_buf[len] = outwrite_sym;
    len++;
    unsigned char *video_buf = (unsigned char *)VIDEO_BUF_PTR + 2 * (VIDEO_WIDTH * LINE + COLUMN);
    video_buf[0] = outwrite_sym;
    video_buf[1] = WHITE; 
    video_buf[3] = WHITE; 
    COLUMN++;
    cursor_moveto(); // Пермещение каретки
}
// Регистрация нажати
void on_key(unsigned char key){
    switch (key){
    case BACKSPACE:
        backspace_pressed();
        break;
    case ENTER:
        enter_pressed();
        break;
    default:
        write_sym(SYMBOLS[key]);
        break;
    }
}
// Функции работы с прерываниями
void intr_init(){
    int i;
    int idt_count = sizeof(g_idt) / sizeof(g_idt[0]);
    for(i = 0; i < idt_count; i++)
    intr_reg_handler(i, GDT_CS, 0x80 | IDT_TYPE_INTR, default_intr_handler); // segm_sel=0x8, P=1, DPL=0, Type=Intr
}
void intr_start(){
    int idt_count = sizeof(g_idt) / sizeof(g_idt[0]);
    g_idtp.base = (unsigned int) (&g_idt[0]);
    g_idtp.limit = (sizeof (struct idt_entry) * idt_count) - 1;
    asm("lidt %0" : : "m" (g_idtp) );
}
void intr_enable(){
    asm("sti");
}
void intr_disable(){
    asm("cli");
}
// ЗАпись в порт
static inline unsigned char inb (unsigned short port){
    unsigned char data;
    asm volatile ("inb %w1, %b0" : "=a" (data) : "Nd" (port));
    return data;
}
// Чтение из порта
static inline void outb (unsigned short port, unsigned char data){
    asm volatile ("outb %b0, %w1" : : "a" (data), "Nd" (port));
    cur_stat = data;
}
void default_intr_handler(){
    asm("pusha");
    // ... 
    asm("popa; leave; iret");
}
void intr_reg_handler(int num, unsigned short segm_sel, unsigned short flags, intr_handler hndlr){
    unsigned int hndlr_addr = (unsigned int) hndlr;
    g_idt[num].base_lo = (unsigned short) (hndlr_addr & 0xFFFF);
    g_idt[num].segm_sel = segm_sel;
    g_idt[num].always0 = 0;
    g_idt[num].flags = flags;
    g_idt[num].base_hi = (unsigned short) (hndlr_addr >> 16);
}
void out_str(int color, const char* ptr, unsigned int strnum, unsigned int col){
    unsigned char* video_buf = (unsigned char*) VIDEO_BUF_PTR;
    video_buf += 80*2 * strnum + col*2;
    while (*ptr)
    {   
        video_buf[0] = (unsigned char) *ptr; 
        video_buf[1] = color; 
        video_buf += 2;
        ptr++;
    }
}
void start_info(){
    const char info[][50] = {
       // "-------------------------------------------",
        "######-##--##-######--####-----####---####-", //1
        "--##---###-##-##-----##--##---##--##-##----", //2
        "--##---##-###-####---##--##---##--##--####-", //3
        "--##---##--##-##-----##--##---##--##-----##", //4
        "######-##--##-##------####-----####---####-", //5
        "-----------------TYPE-help-----------------"  //6
    };
    out_str(0x02, info[0], LINE, COLUMN);
    out_str(0x02, info[1], ++LINE, COLUMN);
    out_str(0x02, info[2], ++LINE, COLUMN);
    out_str(0x02, info[3], ++LINE, COLUMN);
    out_str(0x02, info[4], ++LINE, COLUMN);
    out_str(0x02, info[5], ++LINE, COLUMN);
    out_str(0x07, ">", ++LINE, COLUMN);
    COLUMN = 2;
    cursor_moveto();
}
void time_manage(){
    asm volatile("movb %%ch, %0" : "=r"(hours));
    asm volatile("movb %%cl, %0" : "=r"(mins));
    asm volatile("movb %%dh, %0" : "=r"(secs));
    hours = bcd_decimal(hours) + 3; 
    mins = bcd_decimal(mins);
    secs = bcd_decimal(secs);
}
void curtime_func() {
        outb(0x70, 4);
        cur_hours = inb(0x71);
        outb(0x70, 2);
        cur_mins = inb(0x71);
        outb(0x70, 0);
        cur_secs = inb(0x71);

        cur_hours = bcd_decimal(cur_hours) + 3; 
        cur_mins = bcd_decimal(cur_mins);
        cur_secs = bcd_decimal(cur_secs);
}
void keyb_process_keys(){
    if (inb(0x64) & 0x01){
        unsigned char scan_code;
        unsigned char state;
        scan_code = inb(0x60); 
		turn_on_ticks();
        if (scan_code < 128)
        on_key(scan_code);

    }
	turn_off_ticks();

}
void keyb_handler(){
    asm("pusha");
    keyb_process_keys();
    outb(PIC1_PORT, 0x20);
    asm("popa; leave; iret");
}
void keyb_init(){
    intr_reg_handler(0x09, GDT_CS, 0x80 | IDT_TYPE_INTR, keyb_handler);
    outb(PIC1_PORT + 1, KEYBOARD);
}
// Включение прерываний тиков и выключение клавиатуры
void turn_on_ticks() {
	intr_disable();
	outb(PIC1_PORT + 1, TICK_TIMER); 
	intr_enable();
}
// Вылючение прерываний тиков и включение клавиатуры
void turn_off_ticks() {
	intr_disable();
	outb(PIC1_PORT + 1, KEYBOARD);
	intr_enable();
}
void ticks_handler(){
    asm("pusha");
	ticks++;
	if (ticks > 999) {
		ticks = 0;
	}
    outb(PIC1_PORT, PIC1_PORT);
    asm("popa; leave; iret");
}
void ticks_init() {
    intr_reg_handler(0x08, GDT_CS, 0x80 | IDT_TYPE_INTR, ticks_handler); //0x08 - code for ticks timer
    outb(PIC1_PORT + 1, TICK_TIMER);
}

extern "C" int kmain(){

    time_manage();
    start_info();

    intr_enable();
    intr_init();
    intr_start();

    ticks_init();
    keyb_init();

    while(1){
        asm("hlt");
    }

    return 0;
}
