#include "o3.h"
#include "gpio.h"
#include "systick.h"


typedef struct {
	volatile word CTRL;
	volatile word MODEL;
	volatile word MODEH;
	volatile word DOUT;
	volatile word DOUTSET;
	volatile word DOUTCLR;
	volatile word DOUTTGL;
	volatile word DIN;
	volatile word PINLOCKN;
} gpio_port_map_t;

volatile struct gpio_map_t {
	volatile gpio_port_map_t ports[6];
	volatile word unused_space[10];
	volatile word EXTIPSELL;
	volatile word EXTIPSELH;
	volatile word EXTIRISE;
	volatile word EXTIFALL;
	volatile word IEN;
	volatile word IF;
	volatile word IFS;
	volatile word IFC;
	volatile word ROUTE;
	volatile word INSENSE;
	volatile word LOCK;
	volatile word CTRL;
	volatile word CMD;
	volatile word EM4WUEN;
	volatile word EM4WUPOL;
	volatile word EM4WUCAUSE;
} *gpio = (struct gpio_map_t*) GPIO_BASE;

volatile struct systick_t {
	word CTRL;
	word LOAD;
	word VAL;
	word CALIB;
} *systick = (struct systick_t*)SYSTICK_BASE;

/**************************************************************************//**
 * @brief Konverterer nummer til string 
 * Konverterer et nummer mellom 0 og 99 til string
 *****************************************************************************/
void int_to_string(char *timestamp, unsigned int offset, int i) {
    if (i > 99) {
        timestamp[offset]   = '9';
        timestamp[offset+1] = '9';
        return;
    }

    while (i > 0) {
	    if (i >= 10) {
		    i -= 10;
		    timestamp[offset]++;
		
	    } else {
		    timestamp[offset+1] = '0' + i;
		    i=0;
	    }
    }
}

/**************************************************************************//**
 * @brief Konverterer 3 tall til en timestamp-string
 * timestamp-argumentet må være et array med plass til (minst) 7 elementer.
 * Det kan deklareres i funksjonen som kaller som "char timestamp[7];"
 * Kallet blir dermed:
 * char timestamp[7];
 * time_to_string(timestamp, h, m, s);
 *****************************************************************************/
void time_to_string(char *timestamp, int h, int m, int s) {
    timestamp[0] = '0';
    timestamp[1] = '0';
    timestamp[2] = '0';
    timestamp[3] = '0';
    timestamp[4] = '0';
    timestamp[5] = '0';
    timestamp[6] = '\0';

    int_to_string(timestamp, 0, h);
    int_to_string(timestamp, 2, m);
    int_to_string(timestamp, 4, s);
}

struct timetable {int h; int m; int s;};

static int node = SET_SEK;
struct timetable time = {0};
char timestr[8] = "0000000\0";

void write_display(){
	time_to_string(timestr, time.h, time.m, time.s);
	lcd_write(timestr);
}

void add_sek() {
	++time.s;
	if (time.s >= 60) {
		time.s = 0;
		add_min();
	}
}

void add_min() {
	++time.m;
	if (time.m >= 60) {
		time.m = 0;
		add_hour();
	}
}

void add_hour() {
	++time.h;
}


void set_4bit_flag(volatile word *w, int i, word flag) {
	*w &= ~(0b1111 << (i * 4));
	*w |= flag << (i*4);
}

void start_countdown() {
	systick->VAL = systick->LOAD;
}

void stop_countdown() {
	node = ALARM;
}

void init_io() {
	// Set DOUT
	gpio->ports[LED_PORT].DOUTSET = (0b0 << LED_PIN);
	gpio->ports[PB0_PORT].DOUTSET = (0b0 << PB0_PIN);
	gpio->ports[PB1_PORT].DOUTSET = (0b0 << PB1_PIN);

	// Set MODE
	set_4bit_flag(&gpio->ports[LED_PORT].MODEL, LED_PIN, GPIO_MODE_OUTPUT);
	set_4bit_flag(&gpio->ports[PB0_PORT].MODEH, PB0_PIN - 8, GPIO_MODE_INPUT);
	set_4bit_flag(&gpio->ports[PB1_PORT].MODEH, PB1_PIN - 8, GPIO_MODE_INPUT);

	// EXTIPSEL
	set_4bit_flag(&gpio->EXTIPSELH, PB0_PIN - 8, 0b0001);
	set_4bit_flag(&gpio->EXTIPSELH, PB1_PIN - 8, 0b0001);

	// EXTIFALL
	gpio->EXTIFALL |= (0b1 << PB0_PIN);
	gpio->EXTIFALL |= (0b1 << PB1_PIN);

	// IF FLAG
	gpio->IFC &= ~(0b1 << PB0_PIN);
	gpio->IFC &= ~(0b1 << PB1_PIN);

	// IEN
	gpio->IEN |= (0b1 << PB0_PIN);
	gpio->IEN |= (0b1 << PB1_PIN);

	// SYSTICK
	systick->CTRL = 0b111;
	systick->LOAD = 0b110101011001111110000000; // Hvert sekund
	systick->VAL = systick->LOAD;
}

void GPIO_ODD_IRQHandler(){
	switch (node) {
		case SET_SEK: {
			add_sek();
			write_display();
		} break;
		case SET_MIN: {
			add_min();
			write_display();
		} break;
		case SET_HOUR: {
			add_hour();
			write_display();
		} break;
	}
	gpio->IFC = (0b1 << PB0_PIN);
}

void GPIO_EVEN_IRQHandler(){

	switch (node) {
		case SET_SEK: {
			node = SET_MIN;
		} break;
		case SET_MIN: {
			node = SET_HOUR;
		} break;
		case SET_HOUR: {
			node = COUNTDOWN;
			start_countdown();
		} break;
		case COUNTDOWN: {
		} break;
		case ALARM: {
			gpio->ports[LED_PORT].DOUTCLR = GPIO_MODE_OUTPUT;
			node = SET_SEK;
		}
	}
	write_display();
	gpio->IFC = (0b1 << PB1_PIN);
}

void SysTick_Handler() {
	if (node == COUNTDOWN) {
		if (time.s == 0) {
			if (time.m == 0) {
				if (time.h == 0) {
					stop_countdown();
				}
				else --time.h;
			} else --time.m;
		} else --time.s;
		write_display();
	} else if (node == ALARM) {
		lcd_write("ALARM!");
		gpio->ports[LED_PORT].DOUTSET = GPIO_MODE_OUTPUT;
	}
}

int main(void) {
    init();
    init_io();
    write_display();

    while (true);

    return 0;
}

