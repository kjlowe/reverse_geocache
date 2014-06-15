/*

  reverseGeocache.ino - Software for a box that only opens one place on earth. 
  
  Created: August 23, 2011
  Author: Kevin Lowe
  
  This file is based on code from Arduiniana written by Mikal Hart.
  "Reverse Geocache" is a trademark of Mikal Hart.
  
  Decription: This code is written for a reverse geocaching box. The box is 
  activated by pressing a button. If the box is in the correct location it will
  unlock. If the user passes a certain number of attempts, the box will never open.

  Written for this hardware:
  - Arduino UNO
  - Modified Sparkfun GPS Shield with Pololu Switch and headers
  - EM-406A SiRF III GPS Receiver
  - LK202-25-WB - 2x20 LCD from Matrix Orbital
  - Parallax Servo
  
*/

#include <PWMServo.h>
#include <SoftwareSerial.h>
#include <TinyGPS.h>
#include <EEPROM.h>

/** Pin assignments */
static const int GPSrx = 2, GPStx = 3;
static const int LCDrx = 5, LCDtx = 6;
static const int offPin = 8;

/** Constants */
static const int CLOSED_ANGLE = 29; // degrees
static const int OPEN_ANGLE = 60; // degrees
static const float DEST_LATITUDE = -1.944086;
static const float DEST_LONGITUDE = 30.061922; 
static const int RADIUS = 100; // meters
static const int MAX_ATTEMPTS = 50;
static const int HINT_ON_ATTEMPT = 46;
static const int EEPROM_POSITION_ATTEMPTS = 14;
static const int EEPROM_POSITION_SOLVED = 15;
static const int LCD_CHARS_PER_LINE = 20;

/** Objects */
TinyGPS tinyGPS;
PWMServo servo;
int attemptCounter;
int attemptsLeft;
SoftwareSerial gps(GPSrx, GPStx);
SoftwareSerial lcd(LCDrx, LCDtx);

void setup()
{
  // Attach the servo lock
  servo.attach(SERVO_PIN_B);
  servo.write(CLOSED_ANGLE);
  delay(500);
  servo.write(CLOSED_ANGLE+1);
  delay(500);
  servo.write(CLOSED_ANGLE);
  
  // Serial for debug session for PC
  Serial.begin(115200);

  // Setup the LCD Serial Port
  lcd.begin(19200);
  lcd.write(12);

  // Setup the GPS Serial Port
  // SoftwareSerial can only listen on one port.
  // We initialize this port last so that it defaults
  // to listening for the GPS, not the LCD.
  gps.begin(4800);  
  
  // Setup Off Pin
  pinMode(offPin, OUTPUT);
  digitalWrite(offPin, LOW);
  
  // Are we already solved?
  if(EEPROM.read(EEPROM_POSITION_SOLVED) == 1)
  {
    writeToLCD("  Already solved!!", "", 2000);
    delay(1000);
    servo.write(OPEN_ANGLE);
    delay(3000);   
    powerOff();
  }

  // How many attempts have we done?
  attemptCounter = EEPROM.read(EEPROM_POSITION_ATTEMPTS);
  if (attemptCounter == 0xFF) // brand new EEPROM?
    attemptCounter = 0;
    
  // This is one more attempt.
  ++attemptCounter;
  
  // Turn off if we've used all our attempts.
  if (attemptCounter > MAX_ATTEMPTS)
  {
    writeToLCD(" I'm sealed forever", "", 2000);
    powerOff();
  }
  
  // Introduction
  writeToLCD("There is only", "1 place on earth...", 3000);
  writeToLCD("..where I will open", "", 3000);
  clearLCD();
  delay(1500);

  /* Save the new attempt counter */
  EEPROM.write(EEPROM_POSITION_ATTEMPTS, attemptCounter);

  // Attempt count
  attemptsLeft = MAX_ATTEMPTS - attemptCounter;
  clearLCD();
  lcd.print("This is attempt #");
  lcd.print(attemptCounter);
  lcd.println();
  delay(2000); 
  lcd.print("Attempts left: ");
  delay(1500); 
  lcd.print(attemptsLeft);
  delay(2000);   
  
  writeToLCD("    Finding your", "      location", 1000);
}



void loop()
{
  // Has a valid NMEA sentence been parsed?
  if (gps.available() && tinyGPS.encode(gps.read()))
  {
    float lat, lon;
    unsigned long fix_age;

    // Have we established our location?
    tinyGPS.f_get_position(&lat, &lon, &fix_age);
    if (fix_age != TinyGPS::GPS_INVALID_AGE)
    {
      Serial.println("1\2");
      // Calculate distance to destination
      float distance_meters = TinyGPS::distance_between(lat, lon, DEST_LATITUDE, DEST_LONGITUDE);
      clearLCD();
      delay(1000);
  
      // NEARBY: Open up, the puzzle is solved!
      if (distance_meters <= RADIUS)
      {
        writeToLCD("     Welcome to", "  Downtown Kigali", 2000);
        EEPROM.write(EEPROM_POSITION_SOLVED, true);
        delay(1000);
        servo.write(OPEN_ANGLE);
        delay(3000);
      }
      
      // FAR AWAY: Don't open. Direction and distance.
      else
      {
        writeToLCD("   Access Denied!", "", 2000);
        writeToLCD("", "", 1000);
        
        // Turn off if we've used all our attempts.
        if (attemptCounter == MAX_ATTEMPTS)
        {
          writeToLCD(" I'm sealed forever", "", 2000);
          powerOff();
        }        
        
        // Calculate Direction and display on top line
        float courseto = TinyGPS::course_to(lat, lon, DEST_LATITUDE, DEST_LONGITUDE);

        clearLCD();
        lcd.print(centerText("Travel " + degreesToCompassPoint(courseto)));
        lcd.println();

        // Display distance on bottom line
        lcd.print(centerText("by " + distanceToString(distance_meters)));
        
        // Display the major hint within the last few attempts.
        if(attemptCounter >= HINT_ON_ATTEMPT)
        {
          delay(2000);
          clearLCD();  
          writeToLCD("       To The", " Kigali Roundabout", 0);
        }
        
        delay(4000);
      }

      powerOff();
    }
  }

  /* Turn off after 4 minutes */
  if (millis() >= 180000)
  {
    writeToLCD("   No GPS signal", "Face lid up, outside", 60000);
    powerOff();
  }
}

/** Called to shut off the system using the Pololu switch */
void powerOff()
{
  digitalWrite(offPin, HIGH);
  delay(3000);
}

/** Converts a distance into a meter or kilometer string */
String distanceToString(float distance_meters)
{
  if (distance_meters < 1000)
  {
    return (String)(int)distance_meters + "m"; 
  }
  else
  {
    byte decimals = 1;
    char kmBuffer[10];
    dtostrf(distance_meters / 1000, 0, decimals, kmBuffer);
    return (String)kmBuffer + "km"; 
  }
}

/** Converts the degrees to the nearest cardinal/intercardinal point on a compass */
String degreesToCompassPoint(float courseto)
{
  if      (courseto >= 22.5  && courseto < 67.5)    return "Northeast";
  else if (courseto >= 67.5  && courseto < 112.5)   return "East";
  else if (courseto >= 112.5 && courseto < 157.5)   return "Southeast";
  else if (courseto >= 157.5 && courseto < 202.5)   return "South";
  else if (courseto >= 202.5 && courseto < 247.5)   return "Southwest";
  else if (courseto >= 247.5 && courseto < 292.5)   return "West";
  else if (courseto >= 292.5 && courseto < 337.5)   return "Northwest";
  else if (courseto >= 337.5 || courseto < 22.5)    return "North";
  else                                              return "???";  
}

/** Adds spaces such that the provided string will be centered on the LCD */
String centerText(String text)
{
  if (text.length() > LCD_CHARS_PER_LINE)
    return text;
  
  int add_spaces = (LCD_CHARS_PER_LINE - text.length()) / 2;
  
  String centered_string;
  for (int i = 0; i < add_spaces; ++i)
    centered_string += " ";
    
  return (centered_string + text);
}


/** Prints a message to the screen and waits a delay */
void writeToLCD(String top, String bottom, unsigned long del)
{
  clearLCD();
  lcd.print(top);
  lcd.println(); 
  lcd.print(bottom);
  delay(del);
}

/** Clears the LCD and homes the cursor */
void clearLCD()
{
  lcd.write(12);  
}
