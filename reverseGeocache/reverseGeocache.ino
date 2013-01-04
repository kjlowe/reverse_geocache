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
    Msg("  Already solved!!", "", 2000);
    delay(1000);
    servo.write(OPEN_ANGLE);
    delay(3000);   
    PowerOff();
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
    Msg(" I'm sealed forever", "", 2000);
    PowerOff();
  }
  
  // Introduction
  Msg("There is only", "1 place on earth...", 3000);
  Msg("..where I will open", "", 3000);
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
  
  Msg("    Finding your", "      location", 1000);
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
        Msg("     Welcome to", "  Downtown Kigali", 2000);
        EEPROM.write(EEPROM_POSITION_SOLVED, true);
        delay(1000);
        servo.write(OPEN_ANGLE);
        delay(3000);
      }
      
      // FAR AWAY: Don't open. Direction and distance.
      else
      {
        Msg("   Access Denied!", "", 2000);
        Msg("", "", 1000);
        
        // Turn off if we've used all our attempts.
        if (attemptCounter == MAX_ATTEMPTS)
        {
          Msg(" I'm sealed forever", "", 2000);
          PowerOff();
        }        
        
        // Calculate Direction and display on top line
        float courseto = TinyGPS::course_to(lat, lon, DEST_LATITUDE, DEST_LONGITUDE);
        clearLCD();
        if (courseto >= 22.5 && courseto < 67.5)          lcd.print("  Travel Northeast");
        else if (courseto >= 67.5 && courseto < 112.5)    lcd.print("    Travel East");
        else if (courseto >= 112.5 && courseto < 157.5)   lcd.print("  Travel Southeast");
        else if (courseto >= 157.5 && courseto < 202.5)   lcd.print("    Travel South");
        else if (courseto >= 202.5 && courseto < 247.5)   lcd.print("  Travel Southwest");
        else if (courseto >= 247.5 && courseto < 292.5)   lcd.print("    Travel West");
        else if (courseto >= 292.5 && courseto < 337.5)   lcd.print("  Travel Northwest");
        else if (courseto >= 337.5 || courseto < 22.5)    lcd.print("    Travel North");
        else lcd.print("???");
        lcd.println();

        // Display distance on bottom line
        DisplayDistanceString(distance_meters);
        
        // Display the major hint within the last few attempts.
        if(attemptCounter >= HINT_ON_ATTEMPT)
        {
          delay(2000);
          clearLCD();  
          Msg("       To The", " Kigali Roundabout", 0);
        }
        
        delay(4000);
      }

      PowerOff();
    }
  }

  /* Turn off after 4 minutes */
  if (millis() >= 180000)
  {
    Msg("   No GPS signal", "Face lid up, outside", 60000);
    PowerOff();
  }
}

/** Called to shut off the system using the Pololu switch */
void PowerOff()
{
  digitalWrite(offPin, HIGH);
  delay(3000);
}

/** Displaces in the distance, centered on a 20 character LCD line. */
void DisplayDistanceString(float distance_meters)
{
  if (distance_meters < 1000)
  {
    for (int i = 0; i < 7 - (int)(log10(distance_meters)/2); ++i)
      lcd.print(" ");
  
    lcd.print("by ");
    lcd.print(distance_meters, 0);
    lcd.print("m");
  }
  else if (distance_meters < 10000)
  {
    lcd.print("      by ");
    lcd.print(distance_meters / 1000, 1);
    lcd.print("km");
  }
  else
  {
    unsigned int distance_km = distance_meters / 1000;
    for (int i = 0; i < 7 - (int)(log10(distance_km)/2); ++i)
      lcd.print(" ");
  
    lcd.print("by ");
    lcd.print(distance_km);
    lcd.print("km");
  }
}

/** Prints a message to the screen and waits a delay */
void Msg(const char *top, const char *bottom, unsigned long del)
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
