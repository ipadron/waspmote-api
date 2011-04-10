/*
 *  Copyright (C) 2009 Libelium Comunicaciones Distribuidas S.L.
 *  http://www.libelium.com
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 2.1 of the License, or
 *  (at your option) any later version.
   
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
  
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  Version:		0.6
 *  Design:		David Gascón
 *  Implementation:	Alberto Bielsa, David Cuartielles, Mikal Hart
 */
  

/******************************************************************************
 * Includes
 ******************************************************************************/

#ifndef __WPROGRAM_H__
  #include "WaspClasses.h"
#endif

/******************************************************************************
 * Definitions & Declarations
 ******************************************************************************/

/******************************************************************************
 * Constructors
 ******************************************************************************/

WaspGPS::WaspGPS()
{
  _baudRate = 4800;  	// by default we choose NMEA's speed for the port
  //_baudRate = 9600;  	// by default we choose NMEA's speed for the port
  _uart=1;

  // basic GPS configuration
  reboot=COLD;                  // reboot the system or init the system the first time
  flag = ACK;			// init the flag at ACK
  commMode = GPS_NMEA;		// communication mode: software or hardware serial
  pwrMode = GPS_ON;		// power on the GPS
  wakeMode = COLD;   		// wake up erasing the internal data
  clkOffset= (char*) "96000";
  timeOfWeek= (char*) "497260";
  weekNo= (char*) "921";
  channel= (char*) "12";
  resetCfg= (char*) "1";
  coordinateLat = (char*) "4140.8217"; // Zaragoza, Spain, coordinates for Libelium
  coordinateLon = (char*) "00053.1736"; // Zaragoza, Spain, coordinates for Libelium
  coordinateAl = (char*) "198"; // Zaragoza, Spain, coordinates for Libelium
  checksum=0;
}

/******************************************************************************
 * User API
 ******************************************************************************/


/******************************************************************************
 * PRIVATE FUNCTIONS                                                          *
 ******************************************************************************/
 
 
/*
 * extractDate (void) - private function getting the Date from the GPS
 *
 * makes a call to the GPRMC sentence type to extract the date from the GPS, it
 * separates the data using the Utils.inBuffer and breaking it into Utils.arguments
 *
 * Stores the final value in the dateGPS variable
 *
 * The system could time out, it could be good to double check the GPS.flag for
 * the value GPS_TIMEOUT when not being sure about data consistency
 */
void WaspGPS::extractDate(void)
{
  // store current state to return to it later
	uint16_t currentSentences = commMode;
	long previous=0;

	
  // get Date information
	serialFlush(1);
	previous=millis();
	while(!setCommMode(GPS_NMEA_RMC) && (millis()-previous)<3000);
	Utils.strExplode(Utils.inBuffer, ',');       // separates all the subarrays from Utils.inBuffer into the Utils.arguments array
	if( !strcmp(Utils.arguments[0],"$GPRMC") ) 
	{
		for(int i=0;i<MAX_ARGS;i++) dateGPS[i]=Utils.arguments[9][i];
	}
	else
	{
		flag |=GPS_INVALID;
	}

  // return to previous state
	previous=millis();
	if (currentSentences == GPS_BINARY_OFF ) while( !setCommMode(GPS_BINARY) && (millis()-previous)<3000);	
	delay(100);
	previous=millis();
	while(!setCommMode(currentSentences) && (millis()-previous)<3000);
}

/*
 * extractTime (void) - private function getting the Time from the GPS
 *
 * makes a call to the GPGGA sentence type to extract the time from the GPS, it
 * separates the data using the Utils.inBuffer and breaking it into Utils.arguments
 *
 * Stores the final value in the timeGPS variable
 *
 * The system could time out, it could be good to double check the GPS.flag for
 * the value GPS_TIMEOUT when not being sure about data consistency
 */
void WaspGPS::extractTime(void)
{
  	// store current state to return to it later
	uint16_t currentSentences = commMode;
	long previous=0;

  	// get Time
	serialFlush(1);
	previous=millis();
	while(!setCommMode(GPS_NMEA_GGA) && (millis()-previous)<3000);
	Utils.strExplode(Utils.inBuffer, ',');       // separates all the subarrays from Utils.inBuffer into the Utils.arguments array
	if( !strcmp(Utils.arguments[0],"$GPGGA") ) 
	{
		for(int i=0;i<MAX_ARGS;i++) timeGPS[i]=Utils.arguments[1][i];
	}
	else
	{
		flag |= GPS_INVALID;
	}

  // return to previous state
	previous=millis();
	if (currentSentences == GPS_BINARY_OFF ) while( !setCommMode(GPS_BINARY) && (millis()-previous)<3000);	
	delay(100);
	previous=millis();
	while(!setCommMode(currentSentences) && (millis()-previous)<3000);
}


/*
 * parse_decimal (str) - get a number out of a string
 *
 * It gets a number out of a string
 */
long WaspGPS::parse_decimal(char *str)
{
	bool isneg = *str == '-';
	if (isneg) *str++;
	unsigned long ret = 100UL * gpsatol(str);
	while (gpsisdigit(*str)) ++str;
	if (*str == '.')
	{
		if (gpsisdigit(str[1]))
		{
			ret += 10 * (str[1] - '0');
			if (gpsisdigit(str[2]))
				ret += str[2] - '0';
		}
	}
	return isneg ? -ret : ret;
}

/*
 * parse_degrees (str) - get a number out of a string
 *
 * It gets a number out of a string
 */
unsigned long WaspGPS::parse_degrees(char *str)
{
	unsigned long left = gpsatol(str);
	unsigned long tenk_minutes = (left % 100UL) * 10000UL;
	while (gpsisdigit(*str)) ++str;
	if (*str == '.')
	{
		unsigned long mult = 1000;
		while (gpsisdigit(*++str))
		{
			tenk_minutes += mult * (*str - '0');
			mult /= 10;
		}
	}
	return (left / 100) * 100000 + tenk_minutes / 6;
}

/*
 * gpsatol (str) - get a number out of a string
 *
 * It gets a number out of a string 
 */
long WaspGPS::gpsatol(char *str)
{
	long ret = 0;
	while (gpsisdigit(*str))
		ret = 10 * ret + *str++ - '0';
	return ret;
}


/******************************************************************************
 * PUBLIC FUNCTIONS
 ******************************************************************************/

/* ON(void) - opens UART1 and powers the GPS module
 *
 * It opens UART1 and powers the GPS module
 *
 * Returns nothing
*/
void WaspGPS::ON()
{
	setMode(GPS_ON);
//	begin();
	init();
}

/* OFF(void) - closes UART1 and powers off the GPRS module
 *
 * This function closes UART1 and powers off the GPRS module
 *
 * Returns nothing
*/
void WaspGPS::OFF()
{
	close();
	setMode(GPS_OFF);
}

/*
 * init (void) - initialize the GPS on default parms
 *
 * this function initializes the GPS with the default time, date, and coordinates
 * taken from the constructor. It is possible to modify the corresponding public variables
 * and then call init() to reconfigure the
 * GPS with the new conditions
 *
 * The system is not answering anything, therefore it is not possible, at this
 * point to check whether the GPS is malfunctioning
 */
void WaspGPS::init()
{
	init(coordinateLat, coordinateLon, coordinateAl, clkOffset, timeOfWeek, weekNo, channel, resetCfg);
}

/*
 * init (void) - initialize the GPS using grouped parameters
 *
 * this function initializes the GPS with the specific parameters determined by
 * three strings which are equivalent to the public variables in the system.
 * This will try to reconfigure the GPS with the new conditions.
 *
 * The system is not answering anything, therefore it is not possible, at this
 * point to check whether the GPS is malfunctioning
 */
void WaspGPS::init(const char* _coordinateLat, const char* _coordinateLon, const char* _coordinateAl, const char* _clkOffset, const char* _timeOfWeek, const char* _weekNo, const char* _channel, const char* _resetCfg)
{
	long previous=0;
  // set up the initial coordinates, time and date
  if(reboot)
  {
      begin();
      delay(2000);
      USB.print('j');
  }
  sprintf(Utils.inBuffer,"$PSRF104,%s,%s,%s,%s,%s,%s,%s,%s*00",_coordinateLat,_coordinateLon,_coordinateAl,_clkOffset,_timeOfWeek,_weekNo,_channel,_resetCfg);
  
  setChecksum();
  serialFlush(1);
  printString(Utils.inBuffer,1);
  printByte('\r',1);
  printByte('\n',1);
  
  delay(1000);
//  int a;
//  while((a = serialRead(_uart))!= -1)
//  {
//	  USB.print(a, BYTE);
//	  delay(10);
//  }
  // Set GPS on binary mode without sending data
//  USB.print("BINARY");
  previous=millis();
  while( !setCommMode(GPS_BINARY) && (millis()-previous)<3000);
  if((millis()-previous)>=3000)
      USB.print('k');
//  USB.print("BINOFF");
  previous=millis();
//  while(!setCommMode(GPS_BINARY_OFF) && (millis()-previous)<3000);
  while(!setCommMode(GPS_BINARY_OFF) && !(flag & GPS_TIMEOUT))
  {
	  if ((millis() - previous) >= 3000)
	  {
		  flag |= GPS_TIMEOUT;
	      USB.print('l');
//		  return 0; // Perhaps a more specific code?
	  }
  }
  serialFlush(1);
  reboot=HOT;
}


/* setCommMode(mode) - set communication mode: Binary or NMEA
 *
 * It sets the communication mode. There are two possibilities : Binary Mode or NMEA Mode
 */
uint8_t WaspGPS::setCommMode(uint16_t mode)
{
  uint8_t tempBuffer[32] =
  {0xA0,0xA2,0x00,0x18,0x81,0x02,0x01,0x01,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x01,
  0x00,0x01,0x00,0x01,0x00,0x01,0x12,0xC0,0x01,0x60,0xB0,0xB3};
  uint8_t tempBuffer2[16] ={0xA0,0xA2,0x00,0x08,0xA6,0x02,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0xAA,0xB0,0xB3};
  uint8_t byteIN[10];
  commMode = mode;
  uint8_t valid=0;
    
  switch(commMode)
  {
	  case GPS_BINARY:	sprintf(Utils.inBuffer,"$PSRF100,0,%u,8,1,0*0F",_baudRate);
				serialFlush(_uart);
				printString(Utils.inBuffer,_uart);
				printByte('\r',_uart);
				printByte('\n',_uart);
				delay(10);
				serialFlush(_uart);
				delay(10);
				if(serialAvailable(_uart)){
					byteIN[0]=serialRead(_uart);
					if( (byteIN[0]=='$') || ( (byteIN[0]>'0') && (byteIN[0]<'Z') ) ) valid=0;
					else valid=1;
				}
				break;
	  case GPS_BINARY_OFF:	serialFlush(_uart);
				for(int a=0;a<16;a++)
				{
					printByte(tempBuffer2[a],_uart);
					//if(a==14) serialFlush(_uart);
				}
				delay(100);
				if(serialAvailable(_uart))
				{
					for(int a=0;a<10;a++)
					{
						byteIN[a]=serialRead(_uart);
					}
				}
				if(byteIN[5]==0xA6) valid=1;
				else valid=0;
		  		break;
	  case GPS_NMEA:	tempBuffer[8]=0x01;
	  			tempBuffer[10]=0x01;
				tempBuffer[12]=0x01;
				tempBuffer[14]=0x01;
				tempBuffer[16]=0x01;
				tempBuffer[29]=0x65;
				for(int b=0;b<32;b++)
				{
					printByte(tempBuffer[b],1);
				}
				delay(10);
				getRaw(100);
				Utils.strExplode(Utils.inBuffer, ',');
				if( strcmp(Utils.arguments[0],"$GPGGA") ) 
				{
					valid=0;
				}
				else valid=1;
		  		break;
	  case GPS_NMEA_GGA:	for(int c=0;c<32;c++)
				{
					printByte(tempBuffer[c],1);
				}
				delay(10);
				getRaw(100);
				Utils.strExplode(Utils.inBuffer, ',');
				if( strcmp(Utils.arguments[0],"$GPGGA") ) 
				{
					valid=0;
				}
				else valid=1;
		  		break;
	  case GPS_NMEA_GLL:	tempBuffer[6]=0x00;
	  			tempBuffer[8]=0x01;
				for(int d=0;d<32;d++)
				{
					printByte(tempBuffer[d],1);
				}
				getRaw(100);
				Utils.strExplode(Utils.inBuffer, ',');
				if( strcmp(Utils.arguments[0],"$GPGLL") ) 
				{
					valid=0;
				}
				else valid=1;
		  		break;
	  case GPS_NMEA_GSA:	tempBuffer[6]=0x00;
	  			tempBuffer[10]=0x01;
				for(int e=0;e<32;e++)
				{
					printByte(tempBuffer[e],1);
				}
				getRaw(100);
				Utils.strExplode(Utils.inBuffer, ',');
				if( strcmp(Utils.arguments[0],"$GPGSA") ) 
				{
					valid=0;
				}
				else valid=1;
		  		break;
	  case GPS_NMEA_GSV:	tempBuffer[6]=0x00;
	  			tempBuffer[12]=0x01;
				for(int f=0;f<32;f++)
				{
					printByte(tempBuffer[f],1);
				}
				getRaw(100);
				Utils.strExplode(Utils.inBuffer, ',');
				if( strcmp(Utils.arguments[0],"$GPGSV") ) 
				{
					valid=0;
				}
				else valid=1;
		  		break;
	  case GPS_NMEA_RMC:	tempBuffer[6]=0x00;
	  			tempBuffer[14]=0x01;
				for(int g=0;g<32;g++)
				{
					printByte(tempBuffer[g],1);
				}
				getRaw(100);
				Utils.strExplode(Utils.inBuffer, ',');
				if( strcmp(Utils.arguments[0],"$GPRMC") ) 
				{
					valid=0;
				}
				else valid=1;
		  		break;
	  case GPS_NMEA_VTG:	tempBuffer[6]=0x00;
	  			tempBuffer[16]=0x01;
				for(int h=0;h<32;h++)
				{
					printByte(tempBuffer[h],1);
				}
				getRaw(100);
				Utils.strExplode(Utils.inBuffer, ',');
				if( strcmp(Utils.arguments[0],"$GPVTG") ) 
				{
					valid=0;
				}
				else valid=1;
		  		break;
  }
  return valid;
}

/* getCommMode() - get the communication mode
 *
 * It gets the communication mode in use.
 */	
uint8_t WaspGPS::getCommMode(void)
{
  return commMode;
}


/*
 * setMode (void) - sets the current internal Power Mode on the GPS
 *
 * GPS has three different power modes: ON, OFF, STANDBY, we map it
 * to four different software ones: ON (ON), OFF (OFF), SLEEP (STANDBY), 
 * HIBERNATE (STANDBY). It is done this way to keep consistency with the
 * rest of the design
 *
 * The function will set up the GPS.pwrMode public variable to one of the
 * four values, but also send the serial command to the GPS module
 */
void WaspGPS::setMode(uint8_t mode)
{
	pwrMode = mode;
	pinMode(GPS_PW,OUTPUT);
	
  // set the GPS in the defined power mode
	switch (pwrMode)
	{
		case GPS_ON:
			digitalWrite(GPS_PW,HIGH);
			break;

		case GPS_OFF:
			digitalWrite(GPS_PW,LOW);
			break;
	}
}

/*
 * getMode (void) - answers the current internal Power Mode on the GPS
 *
 * this GPS has three different power modes: ON, OFF, STANDBY, we map it
 * to four different software ones: ON (ON), OFF (OFF), SLEEP (STANDBY), 
 * HIBERNATE (STANDBY). It is done this way to keep consistency with the
 * rest of the design
 */
uint8_t WaspGPS::getMode(void)
{
	return pwrMode;
}


/* check() - get if receiver is connected to some satellite
 *
 * It gets if receiver is connected to some satellite
 *
 * It returns '1' if connected, '0' if not
 */
bool WaspGPS::check()
{
	uint16_t currentSentences = commMode;
	bool connection=0;
	long previous=0;
		
	serialFlush(1);
	while(!setCommMode(GPS_NMEA_GGA) && (millis()-previous)<3000);
  			
	// separates all the subarrays from Utils.inBuffer into the Utils.arguments array         
	Utils.strExplode(Utils.inBuffer, ',');
		
  	// the data is valid only if the GPGGA position 7 is 1 or bigger
	if( !strcmp(Utils.arguments[0],"$GPGGA") ) 
	{
		connection = (Utils.arguments[6][0] - '0');
	}
	else connection=0;

  	// return to previous state
	previous=millis();
	if (currentSentences == GPS_BINARY_OFF ) while( !setCommMode(GPS_BINARY) && (millis()-previous)<3000);	
	delay(100);
	previous=millis();
	while(!setCommMode(currentSentences) && (millis()-previous)<3000);
	
	return connection;
}


/*
 * getTime (void) - answers the current time on the GPS
 *
 * gets the time from the GPS and returns it in the format "065600.0000" - hhmmss.mmmm
 *
 * The system could time out, it could be good to double check the GPS.flag for
 * the value GPS_TIMEOUT when not being sure about data consistency
 */
char* WaspGPS::getTime(void)
{
  extractTime();
  return timeGPS;
}


/*
 * getDate (void) - answers the current date on the GPS
 *
 * gets the date from the GPS and returns it in the format "180509" - ddmmyy
 *
 * The system could time out, it could be good to double check the GPS.flag for
 * the value GPS_TIMEOUT when not being sure about data consistency
 */
char* WaspGPS::getDate(void)
{
  extractDate();
  return dateGPS;
}


/*
 * getLatitude (void) - gets the latitude from the GPS
 *
 * forces getLocation and responds the current value of the latitude 
 * variable (in degrees) as a string
 *
 * The system could time out, it could be good to double check the GPS.flag for
 * the value GPS_TIMEOUT when not being sure about data consistency
 */
char* WaspGPS::getLatitude(void)
{	
	flag &= ~(GPS_INVALID);

  // store current state to return to it later
	uint16_t currentSentences = commMode;
	long previous=0;

  // get latitude, longitude, altitude, but NOT time
	while(!setCommMode(GPS_NMEA_GGA) && (millis()-previous)<3000);

  // separates all the subarrays from Utils.inBuffer into the Utils.arguments array         
	Utils.strExplode(Utils.inBuffer, ',');       

	if( !strcmp(Utils.arguments[0],"$GPGGA") ) 
	{
		for(int i=0;i<MAX_ARGS;i++) latitude[i]=Utils.arguments[2][i];
	}
	else
	{
		flag |=GPS_INVALID;
	}
	
	
  // the data is valid only if the GPGGA position 7 is 1 or bigger
	fixValid = (Utils.arguments[6][0] - '0');  
	if (!fixValid) flag |= GPS_INVALID;

  // return to previous state
	previous=millis();
	if (currentSentences == GPS_BINARY_OFF ) while( !setCommMode(GPS_BINARY) && (millis()-previous)<3000);	
	delay(100);
	previous=millis();
	while(!setCommMode(currentSentences) && (millis()-previous)<3000);

  	return latitude;
}

/*
 * getLongitude (void) - gets the longitude the GPS
 *
 * forces getLocation and responds the current value of the longitude
 * variable (in degrees) as a string
 *
 * The system could time out, it could be good to double check the GPS.flag for
 * the value GPS_TIMEOUT when not being sure about data consistency
 */
char* WaspGPS::getLongitude(void)
{	
	flag &= ~(GPS_INVALID);

  // store current state to return to it later
	uint16_t currentSentences = commMode;
	long previous=0;

  // get latitude, longitude, altitude, but NOT time
	while(!setCommMode(GPS_NMEA_GGA) && (millis()-previous)<3000);

  // separates all the subarrays from Utils.inBuffer into the Utils.arguments array         
	Utils.strExplode(Utils.inBuffer, ',');       

	if( !strcmp(Utils.arguments[0],"$GPGGA") ) 
	{
		for(int i=0;i<MAX_ARGS;i++) longitude[i]=Utils.arguments[4][i];
	}
	else
	{
		flag |=GPS_INVALID;
	}
	
  // the data is valid only if the GPGGA position 7 is 1 or bigger
	fixValid = (Utils.arguments[6][0] - '0');  
	if (!fixValid) flag |= GPS_INVALID;

  // return to previous state
	previous=millis();
	if (currentSentences == GPS_BINARY_OFF ) while( !setCommMode(GPS_BINARY) && (millis()-previous)<3000);	
	delay(100);
	previous=millis();
	while(!setCommMode(currentSentences) && (millis()-previous)<3000);
  	
	return longitude;
}

/*
 * getSpeed (void) - gets the speed from the GPS
 *
 * makes a call to the GPVTG sentence type to extract the data from the GPS, it
 * separates the data using the Utils.inBuffer and breaking it into Utils.arguments
 *
 * Stores the final value in the variable speed as string.
 * It does not update the fixValid variable to show whether the data from the
 * GPS is valid or not
 *
 * Returns the speed in Km/h
 *
 * The system could time out, it could be good to double check the GPS.flag for
 * the value GPS_TIMEOUT when not being sure about data consistency
 */
char* WaspGPS::getSpeed(void)
{
  // store current state to return to it later
  uint16_t currentSentences = commMode;
  long previous=0;

  // get speed and course
  while(!setCommMode(GPS_NMEA_VTG) && (millis()-previous)<3000);

  // separates all the subarrays from Utils.inBuffer into the Utils.arguments array         
  Utils.strExplode(Utils.inBuffer, ',');  

  if( !strcmp(Utils.arguments[0],"$GPVTG") ) 
  {
	  for(int i=0;i<MAX_ARGS;i++) speed[i]=Utils.arguments[7][i];
  }
  else
  {
	  flag |=GPS_INVALID;
  }

  // return to previous state
  previous=millis();
  if (currentSentences == GPS_BINARY_OFF ) while( !setCommMode(GPS_BINARY) && (millis()-previous)<3000);	
  delay(100);
  previous=millis();
  while(!setCommMode(currentSentences) && (millis()-previous)<3000);

  return speed;
}

/*
 * getAltitude (void) - gets the altitude from the GPS
 *
 * forces getLocation and responds the current value of the altitude 
 * variable (in meters) as a string
 *
 * The system could time out, it could be good to double check the GPS.flag for
 * the value GPS_TIMEOUT when not being sure about data consistency
 */
char* WaspGPS::getAltitude(void)
{
	flag &= ~(GPS_INVALID);

  // store current state to return to it later
	uint16_t currentSentences = commMode;
	long previous=0;

  // get latitude, longitude, altitude, but NOT time
	while(!setCommMode(GPS_NMEA_GGA) && (millis()-previous)<3000);

  // separates all the subarrays from Utils.inBuffer into the Utils.arguments array         
	Utils.strExplode(Utils.inBuffer, ',');       

	if( !strcmp(Utils.arguments[0],"$GPGGA") ) 
	{
		for(int i=0;i<MAX_ARGS;i++) altitude[i]=Utils.arguments[9][i];
	}
	else
	{
		flag |=GPS_INVALID;
	}
	
  // the data is valid only if the GPGGA position 7 is 1 or bigger
	fixValid = (Utils.arguments[6][0] - '0');  
	if (!fixValid) flag |= GPS_INVALID;

  // return to previous state
	previous=millis();
	if (currentSentences == GPS_BINARY_OFF ) while( !setCommMode(GPS_BINARY) && (millis()-previous)<3000);	
	delay(100);
	previous=millis();
	while(!setCommMode(currentSentences) && (millis()-previous)<3000);

	  return altitude;
}


/*
 * getCourse (void) - gets the course from the GPS
 *
 * makes a call to the GPVTG sentence type to extract the data from the GPS, it
 * separates the data using the Utils.inBuffer and breaking it into Utils.arguments
 *
 * Stores the final value in the variable course as string. 
 * It does not update the fixValid variable to show whether the data from the GPS is valid or not
 *
 *
 * The system could time out, it could be good to double check the GPS.flag for
 * the value GPS_TIMEOUT when not being sure about data consistency
 */
char* WaspGPS::getCourse(void)
{
  // store current state to return to it later
	uint16_t currentSentences = commMode;
	long previous=0;

  // get speed and course
	while(!setCommMode(GPS_NMEA_VTG) && (millis()-previous)<3000);

  // separates all the subarrays from Utils.inBuffer into the Utils.arguments array         
	Utils.strExplode(Utils.inBuffer, ',');  

	if( !strcmp(Utils.arguments[0],"$GPVTG") ) 
	{
		for(int i=0;i<MAX_ARGS;i++) course[i]=Utils.arguments[1][i];
	}
	else
	{
		flag |=GPS_INVALID;
	}

  // return to previous state
	previous=millis();
	if (currentSentences == GPS_BINARY_OFF ) while( !setCommMode(GPS_BINARY) && (millis()-previous)<3000);	
	delay(100);
	previous=millis();
	while(!setCommMode(currentSentences) && (millis()-previous)<3000);

	return course;
}


/*
 * getRaw (int) - gets a data string from the GPS
 *
 * store the next consisten NMEA sentence in the internal buffer Utils.inBuffer
 * if the input is 0, continue until the end of the GPS sentence or up to
 * the max buffer size (GPS_BUFFER_SIZE)
 *
 * The system could time out, it will set the GPS.flag with the
 * value GPS_TIMEOUT when the GPS is not answering after 1000 milliseconds
 *
 * It will return the string and not only update it in the Utils.inBuffer. In case
 * of error it will write GPS_TIMEOUT_em to the buffer
 */
char* WaspGPS::getRaw(int byteAmount)
{
  flag &= ~(GPS_TIMEOUT);

  uint8_t byteGPS = 0;		// the last byte coming through the port
  int i = 0;			// count the amount of bytes read
  uint32_t timeout = 1000;	// millis to wait before declaring timeout

  if (byteAmount == 0) byteAmount = GPS_BUFFER_SIZE;
  
  serialFlush(1);	// empty the port
  Utils.clearBuffer();
  // wait until arrival of a byte
  while(!serialAvailable(_uart) && timeout > 0) { delay(1); timeout--; }; 	

  if (timeout <= 0)
  {
    flag |= GPS_TIMEOUT;
    return GPS_TIMEOUT_em;
  }

  byteGPS = serialRead(_uart);	// read the first byte coming through the port         

  while(byteGPS != '$')
  { 
    if(serialAvailable(_uart) > 0) 
      byteGPS = serialRead(_uart); // flush incomplete sentences
  }
  Utils.inBuffer[i]=byteGPS;  
  i++;
  while(i <= 1 || (byteGPS != '*' && byteGPS != '$' && i < byteAmount)){ // read the GPS sentence
//  while(!i || (byteGPS != '*' && byteGPS != '$' && i < byteAmount)){ // read the GPS sentence
	  if(serialAvailable(_uart) > 0)
    {
      byteGPS = serialRead(_uart);
      if (byteGPS != '*' && byteGPS != '$' && i < byteAmount) Utils.inBuffer[i]=byteGPS;               
/*      if (byteGPS != '$' && i < byteAmount) Utils.inBuffer[i]=byteGPS;               */
      i++;
    }  
   }
//   // add the checksum
//   if (byteGPS == '*' && i < byteAmount) 
//   {
//     timeout = 1000;
//     while(!serialAvailable(1) && timeout > 0) { delay(1); timeout--; };
//     if (timeout <= 0)
//     {
//       flag |= GPS_TIMEOUT;
//       return GPS_TIMEOUT_em;
//     }
//     byteGPS = serialRead(_uart); 	
//     Utils.inBuffer[i]=byteGPS;
//     i++;
//     if (i < byteAmount) 
//     {
//       timeout = 1000;
//       while(!serialAvailable(1) && timeout > 0) { delay(1); timeout--; };
//       if (timeout <= 0)
//       {
//         flag |= GPS_TIMEOUT;
//         return GPS_TIMEOUT_em;
//       }
//       byteGPS = serialRead(_uart); 	
//       Utils.inBuffer[i]=byteGPS;
//       i++;
//     }
//   }

  if (byteGPS == '\n' || i == byteAmount) Utils.inBuffer[i-1] = '\0';  
  else Utils.inBuffer[i] = '\0';
  
  return Utils.inBuffer; 
}


/* getChecksum(buffer) - calculate checksum for a secuence given as a parameter
 *
 * It calculates the corresponding checksum for a secuence given as a parameter
 *
 * It stores in 'checkSUM' variable the result
 */
void WaspGPS::getChecksum(uint8_t* buffer)
{
	int a=4;
	int check=0;
	uint8_t aux=0, aux2=0;
	while( (buffer[aux]!=0xB0) || (buffer[aux+1]!=0xB3) )
	{
		aux++;
	}
	buffer[aux-1]=0x00;
	buffer[aux-2]=0x00;
	aux=0;
	while( (buffer[a]!=0xB0) || (buffer[a+1]!=0xB3) )
	{
		check+=buffer[a];
		check &= 0x7FFF;
		a++;
	}
	if(check>255)
	{
		aux=check/256;
		aux2=check-(aux*256);
		checkSUM[0]=aux;
		checkSUM[1]=aux2;
	}
	else
	{
		checkSUM[0]=0x00;
		checkSUM[1]=check;
	}
}

/* saveEphems() - save ephemeris into SD
 *
 * It saves ephemeris into SD. It creates a file named 'FILE_EPHEMERIS' and stores ephemeris into it.
 *
 * It returns '2' when no ephemeris are returned by GPS receiver, '1' when error on writing and '0' on succesful.
 */
uint8_t WaspGPS::saveEphems()
{
	return saveEphems(FILE_EPHEMERIS);
}


/* saveEphems(filename) - save ephemeris into SD
 *
 * It saves ephemeris into SD. It creates a file named 'filename' and stores ephemeris into it.
 *
 * It returns '2' when no ephemeris are returned by GPS receiver, '0' when error on writing and '1' on succesful.
 */
uint8_t WaspGPS::saveEphems(const char* filename)
{
	uint8_t tempBuffer[11] ={0xA0,0xA2,0x00,0x03,0x93,0x00,0x00,0x00,0x00,0xB0,0xB3};
	uint8_t* ByteIN = (uint8_t*) calloc(110,sizeof(uint8_t));
	uint8_t* tempData = (uint8_t*) calloc(92,sizeof(uint8_t));
	uint8_t endFile[7] ={0xAA,0xBB,0xCC,0xCC,0xBB,0xAA,0xAA};
	uint8_t counter3=0;
	uint8_t end=0;
	uint16_t interval=1000;
	long previous=millis();
	int8_t error=2;
//	uint8_t est=1;
	// initialize the flags
	flag = 0; SD.flag = 0;
	uint8_t aux=0;
	
	if (SD.isFile(filename)) SD.del(filename);
	SD.create(filename);
	
	previous=millis();
	while(!setCommMode(GPS_BINARY_OFF) && (millis()-previous)<3000);	
	delay(100);
	while(serialAvailable(_uart)>0)
	{
		serialRead(_uart);
	}
	// Iterates asking the GPS about available ephemeris (0..32)
	for(int a=1;a<33;a++)
	{
		tempBuffer[5]=a; // set SV ID
		getChecksum(tempBuffer);
		tempBuffer[7]=checkSUM[0];
		tempBuffer[8]=checkSUM[1];
		for(int b=0;b<11;b++)
		{
			printByte(tempBuffer[b],_uart);
		}
		// read ephemeris data and store into ByteIN
		while(end==0)
		{
			if(serialAvailable(_uart)>0)
			{
				ByteIN[counter3]=serialRead(_uart);
				counter3++;
				previous=millis();
			}
			if( (millis()-previous) > interval )
			{
				end=1;
				serialFlush(_uart);
			} 
		}
		if( counter3>100 ) // ephemeris available
		{
			counter3=6;
			if( (ByteIN[0]!=0xA0) || (ByteIN[1]!=0xA2) ) break;
			while( counter3<96 )
			{
				tempData[counter3-6]=ByteIN[counter3];
				counter3++;
			}
			tempData[counter3-6]=0xAA;
			tempData[counter3-5]=0xAA;
//			tempData[counter3]=0xAA;
//			tempData[counter3+1]=0xAA;
			if(SD.writeSD(filename,tempData,aux*90)) error=1;
			else error=0;
			USB.print(SD.getFileSize(filename),10);
			aux++;
		}
		counter3=0;
		end=0;
		previous=millis();
	}
	USB.print("aux:");USB.print(aux,10);
//?	if (error==1) if(SD.writeSD(filename,endFile,aux*90)) error=1;
	if (error==1)
	{
		USB.print("no error ");
		if(!SD.writeSD(filename,"prue",aux*90))
		{
			error=0;
			USB.println("error");
		}
		if(!SD.writeSD(filename,endFile,aux*90))
		{
			error=0;
			USB.println("error");
		}
		USB.print(SD.getFileSize(filename),10);
	}
	free(ByteIN);
	free(tempData);
	ByteIN=NULL;
	tempData=NULL;
	USB.print(SD.getFileSize(filename),10);
	return error;
}

/* loadEphems() - load ephemeris from SD to GPS receiver
 *
 * It loads ephemeris from SD to GPS receiver.
 *
 * It returns '1' on success and '0' on error.
 */
uint8_t WaspGPS::loadEphems()
{
	return loadEphems(FILE_EPHEMERIS);
}


/* loadEphems(filename) - load ephemeris from SD file 'filename' to GPS receiver
 *
 * It loads ephemeris from SD to GPS receiver.
 *
 * It returns '1' on success and '0' on error.
 */
uint8_t WaspGPS::loadEphems(const char* filename)
{
	uint8_t* tempData = (uint8_t*) calloc(99,sizeof(uint8_t));
	uint8_t* answer = (uint8_t*) calloc(10,sizeof(uint8_t));
	uint8_t endFile=0;
	uint16_t offset=0;
	uint8_t counter3=0;
	uint8_t end=0;
	uint16_t interval=1000;
	long previous=millis();
	int8_t error=0;
	SD.flag = 0;
	
	/*** Disable All Binary Messages ***/
	while(!setCommMode(GPS_BINARY_OFF) && (millis()-previous)<3000);
	delay(100);
	while(serialAvailable(_uart)>0)
	{
		serialRead(_uart);
//		USB.print('x');
	}
	USB.print('y');
	while( !endFile )
	{
//		USB.print(SD.bufferBin[0],16);USB.print(SD.bufferBin[1],16);USB.print(SD.bufferBin[2],16);
//				USB.print(SD.bufferBin[3],16);USB.print(SD.bufferBin[4],16);
		SD.catBin(filename,offset,5);
		USB.print(SD.bufferBin[0],16);USB.print(SD.bufferBin[1],16);USB.print(SD.bufferBin[2],16);
		USB.print(SD.bufferBin[3],16);USB.print(SD.bufferBin[4],16);
		if( (SD.bufferBin[0]==0xAA) && (SD.bufferBin[1]==0xBB) && (SD.bufferBin[2]==0xCC) &&
				   (SD.bufferBin[3]==0xCC) && (SD.bufferBin[4]==0xBB) ) endFile=1;
//?
		if((SD.flag & FILE_OPEN_ERROR) || (SD.flag & SEEK_FILE_ERROR))
			endFile=1;
//		USB.println(SD.buffer);
//		USB.print('z');

		if (!endFile)
		{
//			USB.print('a');
			for(int a=0;a<5;a++) // Copy first 5 already read bytes
			{
				tempData[a+5]=SD.bufferBin[a];
			}
			offset+=5;
			SD.catBin(filename,offset,85);
			if(SD.flag & SEEK_FILE_ERROR)
				endFile=1;
			
			tempData[0]=0xA0;
			tempData[1]=0xA2;
			tempData[2]=0x00;
			tempData[3]=0x5B;
			tempData[4]=0x95;
			for(int b=10;b<95;b++)
			{
				tempData[b]=SD.bufferBin[b-10];
			}
			tempData[95]=0x00;
			tempData[96]=0x00;
			tempData[97]=0xB0;
			tempData[98]=0xB3;
			getChecksum(tempData);
			tempData[95]=checkSUM[0];
			tempData[96]=checkSUM[1];
			for(int c=0;c<99;c++)
			{
				printByte(tempData[c],_uart);
			}
			delay(100);
			while(end==0)
			{
				if(serialAvailable(_uart)>0)
				{
					answer[counter3]=serialRead(_uart);
					counter3++;
					previous=millis();
				}
				if( (millis()-previous) > interval )
				{
					end=1;
					serialFlush(_uart);
				}
//				USB.print('a');
			}
			counter3=0;
			end=0;
			previous=millis();
			if( (answer[0]==0xA0) && (answer[1]==0xA2) && (answer[2]==0x00) && (answer[3]==0x02) &&
						  (answer[4]==0x0B) && (answer[5]==0x95) && (answer[6]==0x00) &&
						  (answer[7]==0xA0) && (answer[8]==0xB0) && (answer[9]==0xB3) ) error=1;
			else error=0;
			offset+=85;
		}
	}
	USB.print('b');
	free(answer);
	free(tempData);
	answer=NULL;
	tempData=NULL;
	return error;
	
}


/* getPosition() - gets the latitude, longitude, altitude, speed, course, time and date
 *
 * It gets the latitude, longitude, altitude, speed, course, time and date
 *
 * It returns '1' on success and '0' on error.
 */
uint8_t WaspGPS::getPosition()
{	
	flag &= ~(GPS_INVALID);
		
	uint16_t currentSentences = commMode;
	long previous=0;
	uint8_t complete = 0;
	
	uint8_t byteGPS = 0;		// the last byte coming through the port
	int i = 0;			// count the amount of bytes read
	uint32_t timeout = 1000;	// millis to wait before declaring timeout
		
  	// get all NMEA sentences
	while(!setCommMode(GPS_NMEA) && (millis()-previous)<3000);
	
	// separates all the subarrays from Utils.inBuffer into the Utils.arguments array         
	Utils.strExplode(Utils.inBuffer, ',');       
		
	
	previous = millis();
	while( complete<3 && (millis()-previous)<5000 )
	{
		if( !strcmp(Utils.arguments[0],"$GPGGA") ) 
		{
			for(int i=0;i<MAX_ARGS;i++) timeGPS[i]=Utils.arguments[1][i];
			for(int i=0;i<MAX_ARGS;i++) latitude[i]=Utils.arguments[2][i];
			for(int i=0;i<MAX_ARGS;i++) longitude[i]=Utils.arguments[4][i];
			for(int i=0;i<MAX_ARGS;i++) altitude[i]=Utils.arguments[9][i];	
			complete++;
		}
		else if( !strcmp(Utils.arguments[0],"$GPRMC") )
		{
			for(int i=0;i<MAX_ARGS;i++) dateGPS[i]=Utils.arguments[9][i];
			for(int i=0;i<MAX_ARGS;i++) timeGPS[i]=Utils.arguments[1][i];
			complete++;
		}
		else if( !strcmp(Utils.arguments[0],"$GPVTG") )
		{
			for(int i=0;i<MAX_ARGS;i++) speed[i]=Utils.arguments[7][i];
			for(int i=0;i<MAX_ARGS;i++) course[i]=Utils.arguments[1][i];
			complete++;
		}
		
		while(!serialAvailable(_uart) && timeout > 0) { delay(1); timeout--; }; 	

		if (timeout <= 0)
		{
			flag |= GPS_TIMEOUT;
			return 1;
		}

		byteGPS = serialRead(_uart);	// read the first byte coming through the port         
				
		while(byteGPS != '$')
		{ 
			if(serialAvailable(_uart) > 0){ 
				byteGPS = serialRead(_uart); // flush incomplete sentences
			}
		}
		Utils.inBuffer[i]=byteGPS;  
		i++;
		while(i <= 1 || (byteGPS != '*' && byteGPS != '$' && i < 100)){ // read the GPS sentence
			if(serialAvailable(_uart) > 0)
			{
				byteGPS = serialRead(_uart);
				if (byteGPS != '*' && byteGPS != '$' && i < 100) Utils.inBuffer[i]=byteGPS;
				i++;
			}  
		}
		
		if (byteGPS == '\n' || i == 100) Utils.inBuffer[i-1] = '\0';  
		else Utils.inBuffer[i] = '\0';
		
		i=0;
		
		Utils.strExplode(Utils.inBuffer, ',');       
	}
	
	  // return to previous state
	previous=millis();
	if (currentSentences == GPS_BINARY_OFF ) while( !setCommMode(GPS_BINARY) && (millis()-previous)<3000);	
	delay(100);
	previous=millis();
	while(!setCommMode(currentSentences) && (millis()-previous)<3000);
		
	if( flag & GPS_INVALID ) return 0;
	else return 1;
}


/******************************************************************************
 * Serial communication functions
 ******************************************************************************/

/*
 * begin (void) - power up the GPS, open the serial port
 *
 * powers up the GPS and opens the serial port at 4800. You should be careful and
 * always add a delay of approximately 3 seconds after calling this command. 
 * Otherwise you risk that the GPS module won't be ready to accept data and your
 * commands to it wouldn't be heard
 *
 * An example on the use of this on the code is:
 *
 *   GPS.begin();
 *   delay(3000);
 *   GPS.init();
 *
 * This will leave time for the GPS to warm up and open -internally- the 
 * communication port to Wasp. According to the Tyco GPS datasheet, it
 * should be possible to connect at faster speeds than 4800bps, and you
 * could change this internally in the _baudRate variable at the WaspGPS 
 * constructor, or at any poing in the code before calling GPS.begin(). This
 * has not been tested, though 
 *
 * On Wasp, calling GPS.begin() means initializing the internal UART drivers
 * inside the ATMEGA1281 processor. In order to push the power consumption to
 * the minimum you should remember calling GPS.close() after working with the
 * GPS
 */
void WaspGPS::begin(void)
{
  Utils.setMuxGPS();
  beginSerial(_baudRate,_uart);

}

/*
 * close (void) - close the serial port
 *
 * closes the serial port
 *
 * An example on the use of this on the code is:
 *
 *   GPS.close();
 *
 * On Wasp, calling GPS.close() means disconnecting the internal UART drivers
 * inside the ATMEGA1281 processor. This will push the power consumption to
 * the minimum, since the UART is making a massive use of resources 
 */
void WaspGPS::close()
{
  closeSerial(_uart);
  Utils.setMux(MUX_TO_LOW,MUX_TO_LOW);
  reboot=COLD;
}


/*
 * checkSum (gpsString) - calculate the NMEA checkSum, leave out $, *, and the checkSum bytes
 *
 * returns 1 if the checksum is right, 0 if error
 *
 */
uint8_t WaspGPS::checkSum(const char* gpsString) 
{
  // clear the checksum flag
  flag &= ~(GPS_BAD_CHECKSUM);

  // return error if there is no asterisk at the end of the string
  if (gpsString[Utils.sizeOf(gpsString)-3] != '*') return 0;

  char check = 0;
  // iterate over the string, XOR each byte with the total sum:
  for (int c = 1; c < Utils.sizeOf(gpsString) - 3; c++) {
    check = char(check ^ gpsString[c]);
  }

  // get the checksum character for the string itself
  char stringCheckSum = (gpsString[Utils.sizeOf(gpsString)-2] - '0') << 4 | (gpsString[Utils.sizeOf(gpsString)-1] - '0');
 
  uint8_t result = (check == stringCheckSum);

  if (!result) flag |= GPS_BAD_CHECKSUM;

  // return the result
  return result;
}


/* getChecksum (gpsString) - calculate the NMEA checkSum, leave out $, *, and the checkSum bytes
 *
 * returns 1 if the checksum is right, 0 if error
 *
 * credit: Tom Igoe
 */
uint8_t WaspGPS::getChecksum(const char* gpsString)
{
  char check = 0;
  // iterate over the string, XOR each byte with the total sum:
  for (int c = 1; c < Utils.sizeOf(gpsString) - 3; c++) {
    check = char(check ^ gpsString[c]);
  }
  return check;
}


/* setChecksum(_checksum) - set checksum to the end of Utils.inBuffer
 *
 * it sets checksum to the end of Utils.inBuffer
 */
void WaspGPS::setChecksum()
{
	checksum= getChecksum(Utils.inBuffer);
	int aux=Utils.sizeOf(Utils.inBuffer);
	float aux2 = checksum%16;
	if(aux2==0.0)
	{
		Utils.inBuffer[aux-1]=48;
		Utils.inBuffer[aux-2]=(checksum/16)+48;
	}
	else
	{
		Utils.inBuffer[aux-2]=(int) (checksum/16) + 48;
		aux2 = checksum/16;
		Utils.inBuffer[aux-1]= (checksum-aux2*16)+48;
		
	}
}

// Preinstantiate Objects //////////////////////////////////////////////////////

WaspGPS GPS = WaspGPS();

