

void smalldel(int v);
void delay_100uS(void);
void delay_ms(int v);

void Init_LCD(void );			/* This is called once to initialise the display */

void Clear_LCD(void );			/* Erases the LCD display */
void Pos_xy_LCD(int x,int y); 	/* Sets the position for the next character write */
void Put_LCD(char v);		/* Writes the character v to the display */

void hex2LCD(int v);
