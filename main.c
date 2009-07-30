 /**********************************************************************
 *    Filename:     main.c       ROBOT TEST SOFTWARE                   *
 *      This is set to run at 8MHz - use the PLL to run faster....     *
 *            timings will have to be adjusted though                  *
 **********************************************************************/
 

#include "p24HJ64GP506.h"
#include <stdio.h>
#include <stdlib.h>

_FOSCSEL( FNOSC_PRI & IESO_OFF );
_FOSC( FCKSM_CSDCMD & OSCIOFNC_OFF & POSCMD_XT );
_FWDT( FWDTEN_OFF );

#include "LCDDRIVE.h"

#define echosound 0   // if 1 then buzzer changes pitch on echo distance

#define segmentA _RB2
#define segmentB _RB3
#define segmentC _RC13
#define segmentD _RC14
#define segmentE _RB8
#define segmentF _RB9
#define segmentG _RB14

volatile char UltraSonicChannel;
volatile unsigned int UltraSonicCycles;
volatile char UltraSonicState;
volatile unsigned int lastad;
volatile unsigned int ECHO;
volatile unsigned int LEFTDIST;
volatile unsigned int RIGHTDIST;

// Variables for distance travel
volatile int leftclicksmoved=0;
volatile int rightclicksmoved=0;
unsigned int distance = 100;
int error = 0;				//Difference in required and actual distance
int lerror = 0;				//Error for left distance
int rerror = 0;				//Error for right distance
unsigned int counter = 0;	//Used for averaging function
unsigned int P=1;			//proportional gain
int leftcom;				//Intermediate variable for left motor speed
int rightcom;				//Intermediate variable for right motor speed
volatile int SevenSegNumber=0;

// Vars for object avoid
unsigned int escaping = 0;
unsigned int turning = 0;
long initial_acc = 10000;
unsigned int times_escapecon_met = 0;
unsigned int times_turncon_met = 0;
volatile int SSCounter;
int turning_right = -1;


// Var for mode switching
int modecounter = 0;

// this sets up the ports.
//   compare to the circuit diagram in datasheet etc
void SetupPORTS(void)
{
	AD1PCFGL=0xFdcc;
	AD1PCFGH=0xFFFF;
 	AD1CON2=0;
 	AD1CON3=0x1010;
 	AD1CON1=0x0400;
	AD1CON1=0x8400;

      /*
  50.
      PORTA
  51.
      15  14  13  12  11  10   9   8   7   6   5   4   3   2   1   0
  52.
       
  53.
       
  54.
      PORTB
  55.
      15  14  13  12  11  10   9   8   7   6   5   4   3   2   1   0
  56.
              I   I   O   O    A               A   A           A   A
  57.
       X   X  SW  SW  L2  L1  POT  x  PGC PGD  HV  V1  x   x   V2  VE
  58.
       
  59.
      PORTC
  60.
      15  14  13  12  11  10   9   8   7   6   5   4   3   2   1   0
  61.
       C   X   X   C                                       x   x
  62.
       
  63.
      PORTD
  64.
      15  14  13  12  11  10   9   8   7   6   5   4   3   2   1   0
  65.
                                               O   O   O   O   O   O
  66.
                      D3  D2   D1  D0  x   x  BZZ Ua  M2b M2a M1b  M1a
  67.
       
  68.
      PORTE
  69.
      15  14  13  12  11  10   9   8   7   6   5   4   3   2   1   0
  70.
       
  71.
      PORTF
  72.
      15  14  13  12  11  10   9   8   7   6   5   4   3   2   1   0
  73.
                                           O   O   I   O   I   O   I
  74.
                                           X   T2  R2 T1  R1 CNT CNR
  75.
       
  76.
      PORTG
  77.
      15  14  13  12  11  10   9   8   7   6   5   4   3   2   1   0
  78.
                                                                   O
  79.
           E  RS  RW           x   x   x   x           X   X   x   Ub
  80.
       
  81.
      */
	LATB = 0x0800;
	TRISB = 0x3233;
	LATC = 0x0000;
	TRISC = 0x9007;
	LATD = 0x0000;
	TRISD = 0x0000;
	LATF = 0x0000;
	TRISF = 0x0015;
	LATG = 0x0000;
	TRISG = 0x0000;
}

int getad(int chan)
{
	AD1CHS0=chan;
  	AD1CON1bits.SAMP=1;smalldel(10);
	AD1CON1bits.SAMP=0;while (!AD1CON1bits.DONE);

	return(ADC1BUF0);
}
////////////////////////////////////////Motor Speed Control Function//////////////////////////////////////////////////

void Motor(int V1,int V2)  

 //      V1 *= (PR3/100);
 //      V2 *= (PR3/100);   
{      
		V1 *=0.95;
        if (V1 >= 0) 			// If left speed is positive
		{
           OC2RS = 0;			//Set left backwards speed to zero
           OC1RS = (V1);			//Set left forwards speed to input
		}       
 		else 					//If left speed is negative
		{
 		   OC1RS = 0;			//Set left forwards speed to zero
 		   OC2RS = (-V1);			//Set left backwards speed to magnitude of input
		}
		if (V2 >= 0) 			//If right speed is positive
	    {
 			OC4RS = 0;			//Set right backwards speed to zero			
			OC3RS = (V2);			//Set right forwards speed to input
		}
 		else					//If right speed is negative
		{
 			OC3RS = 0;			//Set right forwards speed to zero
			OC4RS = (-V2);		//Set right backwards speed to magnitude of input
		}
}

////////////////////////////////////Switch Control Code///////////////////////////////////////////////////////////////

int testswitch( void )  			// returns 0,1,2,3 for no switch,Left,Right and both

{
	switch (PORTB & 0x3000) 	// switches work upsidedown
	{
		case 0x0000:return(3);
		case 0x1000:return(2);
		case 0x2000:return(1);
	}
	return(0);
}

///////////////////////////////////////////////LED Interface code////////////////////////////////////////////////////

void SetLEDS(int val)  			// leds controlled on bottom two bits
 {

	switch (val & 0x3)
	{
		case 0x0:_LATB10=1;_LATB11=1;break;
		case 0x1:_LATB10=1;_LATB11=0;break;
		case 0x2:_LATB10=0;_LATB11=1;break;
		case 0x3:_LATB10=0;_LATB11=0;break;
	}
}

////////////////////////Timer 1 Interrupt, resets Ultrasonics and updates Clicks Moved///////////////////////////////

void __attribute__((interrupt,auto_psv)) _T1Interrupt(void)
{                                                                       // When Timer1 //resets the Ultrasonics chirp - Alternate sides
	UltraSonicChannel^=1;SetLEDS(0);
	_LATG0=UltraSonicChannel;
	UltraSonicCycles=8;
	UltraSonicState=0;
	_T3IE=1;
	_T1IF=0;

	if ((OC1RS != 0) && (OC2RS == 0))		//If left motor is moving forwards,
	{
		leftclicksmoved += TMR6;			//Add last number of clicks moved to total of left motor
	}
	else if ((OC1RS == 0) && (OC2RS != 0))	//If left motor is moving backwards,
	{
		leftclicksmoved -= TMR6;			//Subtract last number of clicks moved to total of left motor
	}

	if ((OC3RS != 0) && (OC4RS == 0))		//If right motor is moving forwards,
	{
		rightclicksmoved += TMR7;			//Add last number of clicks moved to total of right motor
	}
	else if ((OC3RS == 0) && (OC4RS != 0))	//If right motor is moving backwards,
	{
		rightclicksmoved -= TMR7;			//Subtract last number of clicks moved to total of right motor
	}

	TMR6=0; TMR7=0;

}

/////////////////////////////////////////Minchinton's Code for Ultrasonics//////////////////////////////////////

void __attribute__(( interrupt, auto_psv )) _T3Interrupt( void )
{                               // use the 40KHz Timer 3 to send the chirp and to measure echo
	unsigned int v;
		switch ( UltraSonicState )
		{
			case 0:                         // send the chirp
				if ( UltraSonicCycles-- )
				{
					OC5CON = 0xe;
				}
 				else
				{
					OC5CON=0x0;
					UltraSonicState = 1;
					UltraSonicCycles = 20; //delay to calm down
				}
				break;
	
			case 1:                

				if ( !UltraSonicCycles-- )  // wait to settle down after chirp
				{
					UltraSonicState=2;      // Sample the A/D
					AD1CON2 = 0;
					AD1CON1 = 0x04e0;
					AD1CON3 = 0x0202;
					AD1CON1 = 0x84e0;
					AD1CHS0 = 0;AD1CON1bits.SAMP=1;lastad=0x7fff;
				}
				break;
			
			case 2:

          		UltraSonicCycles++;
				v = ADC1BUF0;AD1CON1bits.SAMP = 1; 
	 			if ( v > lastad )   // Crude test to look for echo and see how far we have counted
	 			{
					ECHO = UltraSonicCycles;
					if ( !UltraSonicChannel )
					{
						LEFTDIST = ECHO;
					}
	 				else
					{
						RIGHTDIST = ECHO;
					}
	
					UltraSonicState = 3;
					_T3IE = 0;
	 			}
				else
				{
	 				lastad = v + 32;
				}
	 			if( UltraSonicCycles > 800 )
				{
					if ( !UltraSonicChannel )
					{                               
						LEFTDIST = 0x0FFF;
					}
					else
					{
						RIGHTDIST = 0x0FFF;
	 				}
	 				UltraSonicState = 3;
					_T3IE = 0;
				}
				break;
		}      
		_T3IF = 0;
}

////////////////////////////////////////////////Mario Music Code//////////////////////////////////////////////////////

void music ()				//Plays the first few notes of the Mario music on the buzzer

{
	T2CON=0x8000;
	OC6RS=1000;     
	OC6CON=0x6;
	PR2=1720;                      
	delay_ms(100);
	OC6CON=0x0;                            
	delay_ms(30);
	OC6CON=0x6;
	delay_ms(100);
	OC6CON=0x0;     
	delay_ms(150);
	OC6CON=0x6;
	delay_ms(100);
	OC6CON=0x0;     
	delay_ms(150);
	PR2=2100;       
	OC6CON=0x6;
	delay_ms(150);
	OC6CON=0x0;     
	delay_ms(10);
	PR2=1720;       
	OC6CON=0x6;
	delay_ms(150);
	OC6CON=0x0;     
	delay_ms(100);
	PR2=1400;       
	OC6CON=0x6;
	delay_ms(180);
	OC6CON=0x0;     
	delay_ms(200);
	PR2=2900;
	OC6CON=0x6;     
	 delay_ms(200);
	
	T2CON=0x0;
	OC6CON=0x0;     
	delay_ms(2000);
}

//////////////////////////////////////////////"pew" noise code///////////////////////////////////////////////////////

void pew (long limit)			//plays a "pew" noise of variable length

{
	T2CON=0x8000;
	OC6RS=1000;     
	OC6CON=0x6;
 	PR2=1000;
 	while( PR2 < limit )
	{
		PR2 = PR2+40;
		delay_ms(1);
 	}
 	OC6CON = 0x0;
 	T2CON = 0x0;
}

void resetdist()
{
	leftclicksmoved=0;
	rightclicksmoved=0;
	error = 0;				//Difference in required and actual distance
	lerror = 0;				//Error for left distance
	rerror = 0;				//Error for right distance
	counter = 0;	//Used for averaging function
	P=1;			//proportional gain
	TMR6 = 0;
	TMR7 = 0;
}


void __attribute__((interrupt,auto_psv)) _T5Interrupt(void)
{
	if( testswitch() == 1 && modecounter < 5 )
	{
		modecounter++;
		Motor( 0, 0 );
		resetdist();
		pew(5000);
	}
	else if( testswitch() == 1 )
	{
		modecounter = 0;
		resetdist();
		Motor( 0, 0 );
		pew(5000);
	}
	else if( testswitch() == 2 )
	{
		music();
	}
	_T5IF=0;
}

void movedistance( )
{
			
	lerror = distance - leftclicksmoved;	//find both errors
	rerror = distance - rightclicksmoved;
	
	
	if (lerror > 100)				//if left error is larger than 100
	{
		leftcom=100;				//set left motor to maximum forwards speed
	}
	else if (lerror < -100)			//if left error is smaller than -100
	{
		leftcom=-100;				//set left motor to maximum reverse speed
	}
	else							//if left error is between 100 and -100
	{
		leftcom=lerror;				//set left motor speed value to equal left error
	}
	
	if (rerror > 100)				//if right error is larger than 100
	{
		rightcom=100;				//set right motor to maximum forwards speed
	}
	else if (rerror < -100)			//if right error is smaller than -100
	{
		rightcom=-100;				//set right motor to maximum reverse speed
	}
	else							//if right error is between 100 and -100
	{
		rightcom=rerror;			//set right motor speed value to equal right error
	}
	leftcom *=P;					//set proportional gain
	rightcom *=P;					//set proportional gain
	Motor (rightcom,rightcom);		//set motor speeds
}
////////////////////////////////////////////////////// MAIN //////////////////////////////////////////////////////////
int main ( void )
{
	
//	int SwitchState = 0;

	OSCCON=0x2200;

/*	segmentA=1;
	segmentB=1;
	segmentC=1;
	segmentD=1;
	segmentE=1;
	segmentF=1;
	segmentG=1;
*/
	SetupPORTS();
	Init_LCD();
 	UltraSonicChannel=UltraSonicCycles=UltraSonicState=0;

	PR3=99;                         // setup the timer for 40KHz
	PR4=499;						// setup timer 4 for 1KHz
	PR5=4999;
 	T3CON=0x8000;
	T4CON=0x8010;					//setup seven segment timer (8010)
	T5CON=0x8030;
	T6CON=0x8002;
	T7CON=0x8002;
 	OC5RS=49;                       // Ultrasonic width
	TMR7=0;							//Sets right click counter to 0
	TMR6=0;							//Sets left click counter to 0

	PR1=12499;                      // Set up timer for Ultra sonic chirps
                                    // work out time from data sheet
	T1CON=0x8010;
	_T1IF=0;
	_T1IE=1;
	_T4IE=1;
	_T5IE=1;

	PR2=2900;                       // setup the buzzer
	T2CON=0x8000;

	OC1CON =0xE;  					//Left Wheel Backwards
	OC2CON = 0xE; 					//L Fowards //
	OC3CON = 0xE;  					// R Backwards //
	OC4CON = 0xE;   				// R Fowards //

	OC6RS=1000;     
	OC6CON=0x0;
	#if echosound==0
		times_turncon_met = 0;
		times_escapecon_met = 0;
		turning = 0;
		escaping = 0;
		turning_right = -1;
	#endif

////////////////////////////////////////////////////////////Main Loop///////////////////////////////////////////////////

	while (1)
	{
		switch( modecounter )
		{	
			case 1:
//////////////////////////////////////////////////Distance Keeping Code/////////////////////////////////////////////////
				if( counter < 4 )					//If less than 4 measurements
				{
					lerror = lerror + LEFTDIST;		//Add current left distance to left error accumulator
					rerror = rerror + RIGHTDIST;	//Add current right distance to right error accumulator
					counter +=1;					//Increment counter
				}
				else								//If 4 measurements made,
				{	
					lerror = lerror;				//Average out measurements
					rerror = rerror;
					error = lerror + rerror;		//Find average distance
					error -= 100;					//Subtract desired distance to find error
					if( error > 140 )				//If error is over 100
					{
						Motor( 100, 100 );			//Set motors to maximum speed
					}
					else							//If error is under 100,
					{
						if( error < 0 )				//If error is negative
						{
							P = 2;					//Set proportional gain to 3
						}	
						else						//If it is positive
						{
							P = 0.7;					//Set proportional gain to 1
						}
						error *= P;					//Apply proportional gain
						Motor( error, error );		//Set motor speeds
					}
					counter = 0;						//reset counter and errors
					lerror = 0;
					rerror = 0;	
				}
				break;
			case 2:
////////////////////////////////////////////Distance Moving Code////////////////////////////////////////////////////////
				distance = 500;
				movedistance();
				break;
			case 4:
				distance = 2500;
				movedistance();
				break;
			case 0:
///////////////////////////////////////////////OBJECT AVOIDANCE CODE////////////////////////////////////////////
				if ( escaping == 1 )
				{
					Motor( -100, -100 );
					if( times_escapecon_met > 500 )
					{
						escaping = 0;
						turning = 1;   
						times_escapecon_met = 0;
					}
					else if ( (RIGHTDIST > 50 && LEFTDIST > 30) || (RIGHTDIST > 30 && LEFTDIST > 50))
					{
						times_escapecon_met++;
					}
				}
			
				else if( turning == 1 )
				{					
					if ( turning_right == -1 )
					{
					
						if( RIGHTDIST > LEFTDIST )
						{
							turning_right = 0;
						}
						else
						{
							turning_right = 1;
						}
					}
					else if( turning_right == 0 )
					{
						Motor( -100, 100 );
					}
					else
					{
						Motor( 100, -100 );
					}                      
					
					if( times_turncon_met > 2000 )
					{
						turning = 0;
						times_turncon_met = 0;
						turning_right = -1;     
					}
					else if( (RIGHTDIST > 70 && LEFTDIST > 40) || (RIGHTDIST > 40 && LEFTDIST > 70) )
					{
						times_turncon_met++;
					}
				}
				else
				{
					if( RIGHTDIST < 20 || LEFTDIST < 20 )
					{
						escaping = 1;
					}
					else if( RIGHTDIST < 30 && LEFTDIST < 30 )
					{
						escaping = 1;
					}
					/*else if( initial_acc < 25000 )
					{
						Motor( initial_acc / 250, initial_acc / 250 );
						initial_acc++;
					}*/
					else if(( LEFTDIST > RIGHTDIST ) && ( RIGHTDIST < 40 ))
					{
						Motor(100,0);
					}
					else if(( RIGHTDIST > LEFTDIST ) && ( LEFTDIST < 40 ))
					{
						Motor(0,100);
					}
					else
					{
						Motor (100,100);
					}
				}
				break;			
		}
	}
}
