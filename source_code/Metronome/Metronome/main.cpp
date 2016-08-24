/*Begining of Auto generated code by Atmel studio */
#include <Arduino.h>
#include <TimerOne.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <SD.h>

//playground.arduino.cc/Main/PinChangeInterrupt
///playground.arduino.cc/Code/Timer1
/*
 * SD card attached to SPI bus as follows:
 ** MOSI - pin 11
 ** MISO - pin 12
 ** CLK - pin 13
 ** CS - pin 4
 */
  //////////////////
  //5 inputs
  //[Start/Stop] [Up] [Down] [Next] [Prev]

#define DEBUG_ON 
//#define DEBUG_INFO_ON 
#define ACTIVE_DEBUG_ON

#define INDICATOR_PIN  3
#define NEXT_ENTRY_PIN 8
#define PREV_ENTRY_PIN  9
#define PWM_PIN  0

#define START_STOP_PIN  4
#define INC_BPM_PIN  5
#define DEC_BPM_PIN  6

#define MIN_BPM  50
#define MAX_BPM  220

#define LINE_1 0
#define LINE_2 1
#define LINE_3 2
#define LINE_4 3

#define MAX_WORD_LEN 12
#define MAX_WORDS_LEN 7

#define NEW_LINE_CHAR_COUNT 2

#define MAX_SONGS 5//70
#define MAX_SONG_TEXT_LEN 29

const char CONFIG_FILE_NAME[] = "config.met";
 
#define LCD_ADDRESS 0x3F // set the address of the I2C device the LCD is connected to
    
volatile uint8_t start_stop_flag = 0;//0=off 1=on
volatile uint8_t current_bpm = 120;
volatile uint8_t current_file_line_index=0;
volatile uint8_t num_file_lines=0;
bool config_present = false;

volatile uint8_t bpm_interupt=false;
volatile uint8_t song_interupt=false;
volatile uint8_t indicator_interupt=false;


struct CONFIG_LINE
{
  unsigned int start_byte_index;
  uint16_t byte_count;  
  
} config_data[MAX_SONGS] = {0};

char current_song_text[MAX_SONG_TEXT_LEN] = {"None"};

// create an lcd instance with correct constructor for how the lcd is wired to the I2C chip

LiquidCrystal_I2C lcd(LCD_ADDRESS, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE); 

unsigned long calc_timer_val_from_bpm(uint8_t bpm);
void decrement_file_line_index();
void increment_file_line_index();
void toggle_indicator();
bool initConfigFileData();
void  paddString(char* string,uint8_t len);

void initButtonIoPins();
void initDisplay();
bool initSdCard();
void initTimer();
void loop();
void setup();
void update_start_stop();
bool readConfigLine(uint8_t linenum);
void printStatusText(char *pText);
void  updateDisplay();
void  updateBpmDisplay();
void  updateSongDisplay();
void update_indicator();
void stripNewLine(char* pString);

//////////////////////////
//
/////////////////////////
void loop()
 {	
	while(1)
	{			
		if(bpm_interupt)
		{			
			updateBpmDisplay();
			bpm_interupt = false;
		}		
		
		if(song_interupt)
		{
			updateSongDisplay();	
			song_interupt = false;				
		}
		
		if(indicator_interupt)
		{	
			update_indicator();
			indicator_interupt = false;
		}	
		
	}
}

//////////////////////////
//
/////////////////////////
void setup()
{
	Serial.begin(9600);
	
	#ifdef DEBUG_ON		
		Serial.println("Starting...");
	#endif

	if( !initSdCard() )
	{
	  #ifdef DEBUG_ON
		Serial.println("initSdCard failed");
	  #endif		
	}	
	
	initTimer();
	initButtonIoPins();
	initDisplay();
 
	printStatusText("Starting...");
	delay(2000);	
	printStatusText("Metro-Gnome - Mike H");
	delay(2000);
	printStatusText("Copyright 2016");
	delay(2000);
	
	if(config_present)
	{		
		printStatusText("Reading data file...");
		readConfigLine(current_file_line_index);
	}	
	
	bpm_interupt = false;
	song_interupt = false;
	
	updateDisplay();		
}

//---------------------------------------------------------------------------------------
//***************************
//Timer Stuff
//***************************
//---------------------------------------------------------------------------------------

//////////////////////////
//
/////////////////////////
void initTimer()
{
	unsigned long timer_val = calc_timer_val_from_bpm(current_bpm);
  
	Timer1.initialize(timer_val);         // initialize timer1, and set period -  microseconds  
	Timer1.pwm(PWM_PIN, 512);                // setup pwm on pin 9, 50% duty cycle
	Timer1.attachInterrupt(toggle_indicator);  // attaches callback() as a timer overflow interrupt
	Timer1.stop(); 		
}

//////////////////////////
//update_start_stop
/////////////////////////
void update_start_stop()
{
    start_stop_flag = start_stop_flag ^ 1;
       
  //  Serial.print("start_stop_flag: "); 
   // Serial.println(start_stop_flag); 
    
    if (start_stop_flag == 0)
    {   
      Timer1.stop(); 
      digitalWrite(INDICATOR_PIN, 0);
        
      #ifdef DEBUG_ON
        Serial.println("Timer Stop ");
      #endif  
    }      
    else
    {    
       long interval =  calc_timer_val_from_bpm(current_bpm);
       Timer1.initialize(interval) ;
       
		#ifdef DEBUG_INFO_ON
			Serial.println("------------------------" );
			Serial.println("Timer Start ");
		
			Serial.print("current_bpm: ");
			Serial.println(current_bpm);
		
			Serial.print("interval: ");
			Serial.println(interval);
			Serial.println("------------------------\n" );
		#endif                    
     }  
}

//////////////////////////
//
/////////////////////////
unsigned long calc_timer_val_from_bpm(uint8_t bpm)
{
	unsigned long timer_val = 0;

	// Serial.print("Current BPM: "); 
	// Serial.println(bpm); 
  
	// (bpm / seconds_in_min) = beats/second  
	//  1 000 000 uS/ beats_second = total uS interval 
	//timer interval = total uS interval/2
	//
	//timer_val = ( 1/(bpm/60)/1000000) /2;
  
	float bps = ( (float)bpm/60.0);
  
	// Serial.print("BPS: "); 
	//Serial.println(bps); 

	float full_interval = 1000000/bps;
    
	timer_val = full_interval/2;
  
	#ifdef DEBUG_ON
		// Serial.print("timer_val: "); 
		//  Serial.println(timer_val); 
    
		//Serial.print("bpm ");      
	//	Serial.println(bpm);  
	#endif  
   
	return  timer_val;
}


//////////////////////////
//
/////////////////////////
void decrement_bpm()
{
	if( (current_bpm+1) > MIN_BPM)
	{		
		current_bpm = current_bpm - 1;
		long val =  calc_timer_val_from_bpm(current_bpm);
		Timer1.setPeriod(val);
		
		bpm_interupt = true;

		#ifdef DEBUG_INFO_ON
			Serial.println("-------------------");
			Serial.println("Dec BPM");
			Serial.print("current_bpm: ");
			Serial.println(current_bpm);
			
			Serial.print("calc_timer_val: ");
			Serial.println(val);
			Serial.println("-------------------");
		#endif
	}
}

//////////////////////////
//
/////////////////////////
void increment_bpm()
{   
	if( (current_bpm+1) < MAX_BPM)
	{
		current_bpm = current_bpm + 1;
		long val =  calc_timer_val_from_bpm(current_bpm);
		Timer1.initialize(val) ;
		
		bpm_interupt = true;
		
		#ifdef DEBUG_INFO_ON
		Serial.println("-------------------");
		Serial.println("Dec BPM");
		Serial.print("current_bpm: ");
		Serial.println(current_bpm);
		
		Serial.print("calc_timer_val");
		Serial.println(val);
		Serial.println("-------------------");
		#endif
	}
}


//-------------------------------------------------------------------------------------------------
//***************************
//Initialization Stuff
//***************************
//-------------------------------------------------------------------------------------------------

//////////////////////////
//
/////////////////////////
void pciSetup(byte pin)
{
	*digitalPinToPCMSK(pin) |= bit (digitalPinToPCMSKbit(pin));  // enable pin
	PCIFR  |= bit (digitalPinToPCICRbit(pin)); // clear any outstanding interrupt
	PCICR  |= bit (digitalPinToPCICRbit(pin)); // enable interrupt for the group
}


//-------------------------------------------------------------------------------------------------
//***************************
//Interrupt Stuff
//***************************
//-------------------------------------------------------------------------------------------------

//////////////////////////
//Next-Prev Entry Interupt
/////////////////////////
ISR (PCINT0_vect)
{ 	 
		
	*digitalPinToPCICR(NEXT_ENTRY_PIN) &= ~(1<<digitalPinToPCICRbit(NEXT_ENTRY_PIN));	
	PCIFR  |= bit (digitalPinToPCICRbit(NEXT_ENTRY_PIN)); // clear any outstanding interrupt
   
	delay(10000);
      
	if( digitalRead(NEXT_ENTRY_PIN) == 0 && digitalRead(PREV_ENTRY_PIN) == 1)
	{
		increment_file_line_index();    
	}
	else if( digitalRead(PREV_ENTRY_PIN) == 0 && digitalRead(NEXT_ENTRY_PIN) == 1)
	{
		decrement_file_line_index();		 
	}
  
	PCICR  |= bit (digitalPinToPCICRbit(NEXT_ENTRY_PIN)); // enable interrupt for the group	
	PCIFR  |= bit (digitalPinToPCICRbit(NEXT_ENTRY_PIN)); // clear any outstanding interrupt	
	
}

//////////////////////////
//Inc-Dec BPM Interupt
/////////////////////////
ISR (PCINT2_vect) 
{      
	*digitalPinToPCICR(START_STOP_PIN) &= ~(1<<digitalPinToPCICRbit(START_STOP_PIN));
	PCIFR  |= bit (digitalPinToPCICRbit(START_STOP_PIN)); // clear any outstanding interrupt
   
	delay(10000);
      
	// handle pin change interrupt for D0 to D7 here
	//Serial.println("Interrupt Fired...For D0 to D7 - Inc-Dec BPM Interupt ");   
 
	if( digitalRead(START_STOP_PIN) == 0)
	{  
		//   Serial.println("start_stop");       
		update_start_stop();   
	} 
	else if(digitalRead(INC_BPM_PIN) == 0 && digitalRead(DEC_BPM_PIN) == 1)
	{
		increment_bpm();  
	}
	else if( digitalRead(DEC_BPM_PIN) == 0 && digitalRead(INC_BPM_PIN) == 1 )
	{  
		decrement_bpm();
	}  
 
	PCICR  |= bit (digitalPinToPCICRbit(START_STOP_PIN)); // enable interrupt for the group
	PCIFR  |= bit (digitalPinToPCICRbit(START_STOP_PIN)); // clear any outstanding interrupt   		
}

//-------------------------------------------------------------------------------------------------
//***************************
//Display Stuff
//***************************
//-------------------------------------------------------------------------------------------------

//////////////////////////
//
/////////////////////////
void initDisplay()
{
    lcd.begin(20,4);  // initialize the lcd as 20x4 (16,2 for 16x2)  
    lcd.setBacklight(1); // switch on the backlight
    
    pinMode(INDICATOR_PIN,OUTPUT);  // LED	
	digitalWrite(INDICATOR_PIN, 0);
}

//////////////////////////
//
/////////////////////////
void  updateBpmDisplay()
{		
	char bpm[15]={0};
	sprintf(bpm,"%d",current_bpm);

	lcd.setCursor(6,LINE_2);  // character 0 on line 2
	lcd.print("   ");//clear the line
	lcd.setCursor(6,LINE_2);  // character 0 on line 2
	lcd.print(bpm);		

}

/////////////////////////
//
/////////////////////////
void  updateSongDisplay()
{

//---------------------------------------------------------------------
//Split the title into separate words and store them in an array
//---------------------------------------------------------------------
	char* pText= NULL;
	char text_array[MAX_WORDS_LEN][MAX_WORD_LEN] = {0,0};
	uint8_t word_index = 0;
	const char delimiter[2] = " ";
	uint8_t word_count = 0;
	
	char song_text[MAX_SONG_TEXT_LEN] = {0};
	strncpy(song_text,current_song_text,MAX_SONG_TEXT_LEN);		
					
	pText = strtok (song_text,delimiter);
	stripNewLine(pText);					
	strncpy(text_array[word_index],pText,MAX_WORD_LEN);	
	word_index++;			
	
	while(pText != NULL)
	{
		pText =  strtok (NULL, delimiter);		
		if(pText)	
		{	
			strncpy(text_array[word_index],pText,MAX_WORD_LEN);			
			word_index++;			
		}
	}
	
	word_count = word_index;	
	
//---------------------------------------------------------------------
//print the first line
//---------------------------------------------------------------------
	char line1[25]={0};	
	uint8_t total_chars = 20;	
	const char pPadChar[] = " ";
							
	word_index = 0;		
	snprintf(line1 ,20 ,"Song: %s",text_array[word_index]) ;
	word_index++;	
		
	total_chars = total_chars - strlen((const char*)line1);	 
	
	if(word_count > 1 && total_chars>0)
	{	
		for(uint8_t i=1; i<word_count; i++)
		{							
			total_chars = total_chars - strlen( (const char*)text_array[word_index] ) ; 
								
			if(total_chars>0 )
			{									
				strcat(line1,pPadChar) ;											
				strcat(line1,text_array[word_index]) ;	
				word_index++;					
			}		
			else
			{
				break;//move to the next line
			}
		}		
	}	
	
	//pad the line
	total_chars = strlen((const char*)line1);	
	uint8_t pad_chars = 20-total_chars;		
	
	memset(&line1[total_chars],' ',pad_chars);
					
	lcd.setCursor(0,LINE_3);  // character 0 on line 2		
	lcd.print(line1);

//---------------------------------------------------------------------
//print the second line
//---------------------------------------------------------------------
	char line2[25]={0};					
	for(uint8_t i=word_index;i<word_count;i++)
	{					
		strcat(line2,text_array[i]) ;						
		strcat(line2,pPadChar) ;			
		word_index++;			
	}
	
	total_chars = strlen((const char*)line2);
	pad_chars = 20-total_chars;	
	memset(&line2[total_chars],' ',pad_chars);
	
	lcd.setCursor(6,LINE_4);  // character 0 on line 2
	lcd.print(line2);		
	
}
void stripNewLine(char* pString)
{
	for(int i=0;pString[i]!='\0';i++)
	{
		if(pString[i]=='\n' || pString[i]=='\r')
		{
			pString[i]=' ';
		}
	}
}


//////////////////////////
//
/////////////////////////
void  updateDisplay()
{   
	lcd.clear();
	lcd.home ();
	
	//add bpm label
	char bpm[15]={0};
	sprintf(bpm,"BPM: %d",current_bpm);
	lcd.setCursor(1,LINE_2);
	lcd.print(bpm);	
	
	updateBpmDisplay();
	updateSongDisplay();	
}

//////////////////////////
//
/////////////////////////
void printStatusText(char *pText)
{
	lcd.clear();
	lcd.home ();
	lcd.setCursor(0,LINE_2);  // character 0 on line 2
	lcd.print(pText);
}

//////////////////////////
//
/////////////////////////
void toggle_indicator()
{ 
	indicator_interupt = true;
}

//////////////////////////
//
/////////////////////////
void update_indicator()
{
	int val = digitalRead(INDICATOR_PIN);	
				
	lcd.setCursor(1,LINE_1);
	char buff[19]  = {0};
		
	if( (val ^ 1) )
	{
		memset(buff,0xff,18);	
		buff[0] = 0xdb;
		buff[17] = 0xdb;
	}	
	else
	{	
		memset(buff,' ',18);			
	}
				
	lcd.print(buff);	
	
	digitalWrite(INDICATOR_PIN, (val ^ 1));
}


//-------------------------------------------------------------------------------------------------
//***************************
//Button Press Stuff
//***************************
//-------------------------------------------------------------------------------------------------

//////////////////////////
//
/////////////////////////
void initButtonIoPins()
{
  int i;
  
  // set pullups, if necessary
  for (i=0; i<=12; i++)
  {   
    digitalWrite(i,HIGH);  // enable pullup resistor
  }

  for (i=A0; i<=A5; i++)
  {
      digitalWrite(i,HIGH);// enable pullup resistor
  }  
        
  // enable interrupt for pin...  
  pciSetup(NEXT_ENTRY_PIN);//next entry
  pciSetup(PREV_ENTRY_PIN);//prev entry
 
  pciSetup(INC_BPM_PIN);
  pciSetup(DEC_BPM_PIN);
  
  pciSetup(START_STOP_PIN);//start stop

}

//-------------------------------------------------------------------------------------------------
//***************************
//Disk Stuff
//***************************
//------------------------------------------------------------------------------------------------

//////////////////////////
//
/////////////////////////
void decrement_file_line_index()
{	
	 if(config_present)
	 {
		 if(current_file_line_index != 0 )
		 {
			 current_file_line_index--;
			 readConfigLine(current_file_line_index);
		 }
	 }  
}

//////////////////////////
//
/////////////////////////
void increment_file_line_index()
{				  
	if(config_present)
	{					
		if(current_file_line_index < (num_file_lines-1) )
		{ 
			current_file_line_index++;
			readConfigLine(current_file_line_index);			
		}	
	}
}


//////////////////////////
//
/////////////////////////
bool initSdCard()
{
	if( SD.begin(10) )
	{  
		#ifdef DEBUG_ON
			Serial.println("SD card initialized");	
		#endif		
		
		if(initConfigFileData())
		{       
			config_present = true;  	
		}
		else    
		{
			config_present = false;  			  
		}	
				
		return true;		
	}
	else
	{		
		return false;	       
	}  
}


//////////////////////////
//
/////////////////////////
bool initConfigFileData()
{  
	File myFile; 	
	uint8_t current_line = 0;
	uint8_t current_byte_count = 0;
	
	//if the file doesn't exist then return
	if ( !SD.exists( "config.met") )
	{ 
		Serial.println("CONFIG_FILE_NAME doesn't exist");		
		goto FAIL; 
	}

	printStatusText("Opening Song File...");
							
	myFile = SD.open( "config.met");     
	if (myFile)
	{
		//Serial.println("SD.open(CONFIG_FILE_NAME) opened");
	} 
	else
	{	  	
		printStatusText("Reading Song File Failed");
		goto FAIL;
	}
	
	printStatusText("Reading Song File...");
		
	//loop through file	    
	memset(config_data, 0, sizeof(config_data));    
    
	while( myFile.available() )
	{        
		//get a line  
		char new_char = 0x00;
		config_data[ current_line ].start_byte_index = myFile.position();   
      	  
		do
		{   
			new_char = myFile.read(); 
			current_byte_count++;  
	  
			if(new_char == 0x00 || new_char == 0x0A) 
			{
				//found the eol
				config_data[current_line].byte_count = (current_byte_count-NEW_LINE_CHAR_COUNT) ;  	
				current_line++; 	
				current_byte_count = 0;			
				break; 		 
			}       
	  
		}while( myFile.available() ) ; 
  	 	  
	}//end while
  
	num_file_lines = current_line; 
  
	// close the file:
	myFile.close();        
	
	printStatusText("Reading Song File...Done");	
	
	#ifdef DEBUG_ON
		//for (int j=0;j<num_file_lines;j++)
	//	{
		//	Serial.print("current_line: ");
		//	Serial.println(j);
			
		//	Serial.print("end_byte_index: ");
		//	Serial.println( config_data[ j ].end_byte_index );
	//	}
	#endif  
	
	//for(int i=0;i<num_file_lines;i++)
	//{
	//	readConfigLine(i);		
		
		//Serial.println( config_data[ i ].start_byte_index);
		//Serial.println( config_data[ i ].end_byte_index);
		
	//}
  
  return true;  
  
  FAIL:
	return false;    

}

//////////////////////////
//
/////////////////////////
bool readConfigLine(uint8_t linenum)
{
	File myFile;  
	bool statusOk = true;
	char buff[32]={0};

	char* pBpm = NULL;
	char* pDdisplay = NULL;	
	long timer_val = 0;
	const char delimiter[2] = "^";
	 
	//if the file doesn't exist then return
	if (!SD.exists("config.met"))
	{
		printStatusText("Can't read data file!");		
		goto FAIL;
	}	
	
	//try and open the file
	myFile = SD.open("config.met");
	if (myFile)
	{
		#ifdef DEBUG_INFO_ON
		//	Serial.println("readLine:: SD.open(CONFIG_FILE_NAME) opened");
		#endif  
	}
	else
	{
		#ifdef DEBUG_ON
		//	Serial.println("readLine::SD.open(CONFIG_FILE_NAME) failed");
		#endif  
		
		goto FAIL;			
	}
	
	//read the file data and store the indexes
	myFile.seek(config_data[ linenum ].start_byte_index);	
	myFile.read(&buff,config_data[ linenum ].byte_count);

	//close the file
	myFile.close();
	
	//get the song text
	pDdisplay = strtok (buff,delimiter);
				
	//set the current song name
	memset(current_song_text,0,MAX_SONG_TEXT_LEN);
	strncpy(current_song_text,pDdisplay,MAX_SONG_TEXT_LEN);
	
	//store the current bpm	
	pBpm =  strtok (NULL, delimiter);
	current_bpm = atoi(pBpm);
		
	//start the beat indicator timer
	timer_val =  calc_timer_val_from_bpm(current_bpm);
	Timer1.initialize(timer_val) ;
	
	song_interupt = true;
	bpm_interupt  = true;
	
	return true;
	
	FAIL:  	 
		config_present = false;
		memset(config_data, 0, sizeof(config_data));    
		
		memset(current_song_text,0,MAX_SONG_TEXT_LEN);
		strncpy(current_song_text,"None",MAX_SONG_TEXT_LEN);
	
		Serial.println("readConfigLine FAIL");		
		return false; 

}

