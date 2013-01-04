#include <NewSoftSerial.h>
#include <TinyGPS.h>
#include <EEPROM.h>
#include <Servo2.h> 

/** Pin assignments */
static const int GPSrx = 2, GPStx = 3;
static const int LCDrx = 6, LCDtx = 5;
static const int offPin = 7;
static const int servoPin = 9;

/** Constants */
static const int CLOSED_ANGLE = 105; // degrees
static const int OPEN_ANGLE = 160; // degrees
static const float DEST_LATITUDE = 26.439147;
static const float DEST_LONGITUDE = 127.714104; 
static const int RADIUS = 2000; // meters
static const int DEF_ATTEMPT_MAX = 50;
static const int DEF_ATTEMPT_HINT = 43;
static const int EEPROM_OFFSET = 100;

/** Objects */
TinyGPS tinyGPS;
Servo servo;
int attemptCounter;
int attemptsLeft;
NewSoftSerial gps(GPSrx, GPStx);
NewSoftSerial lcd(LCDrx, LCDtx);

void setup()
{
  // Attach the servo lock
  servo.attach(servoPin);
  servo.write(CLOSED_ANGLE);
  delay(1000);
  
  // Serial for debug session for PC
  Serial.begin(115200);
  
  // Setup the GPS Serial Port
  pinMode(GPSrx, INPUT);
  pinMode(GPStx, OUTPUT);  
  gps.begin(4800);  
  
  // Setup the LCD Serial Port
  pinMode(LCDrx, INPUT);
  pinMode(LCDtx, OUTPUT);  
  lcd.begin(9600);
  
  // Setup Off Pin
  pinMode(offPin, OUTPUT);
  digitalWrite(offPin, LOW);

  // Are we already solved?
  int solved = EEPROM.read(EEPROM_OFFSET + 1);
  if(solved == 1)
  {
    Msg("  Already solved!!", "", 2000);
    delay(1000);        
    servo.write(OPEN_ANGLE);
    delay(3000);   
    PowerOff();
  }

  // How many attempts have we done?
  attemptCounter = EEPROM.read(EEPROM_OFFSET);
  if (attemptCounter == 0xFF) // brand new EEPROM?
    attemptCounter = 0;
    
  // This is one more attempt.
  ++attemptCounter;
  
  // Turn off if we've used all our attempts.
  if (attemptCounter > DEF_ATTEMPT_MAX)
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
  EEPROM.write(EEPROM_OFFSET, attemptCounter);

  // Attempt count
  attemptsLeft = DEF_ATTEMPT_MAX - attemptCounter;
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
      // Calculate distance to destination
      float distance_meters = TinyGPS::distance_between(lat, lon, DEST_LATITUDE, DEST_LONGITUDE);
      clearLCD();
      delay(1000);
  
      // NEARBY: Open up, the puzzle is solved!
      if (distance_meters <= RADIUS)
      {
        Msg(" Welcome to Okinawa", "", 2000);
        EEPROM.write(EEPROM_OFFSET + 1, 1);
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
        if (attemptCounter == DEF_ATTEMPT_MAX)
        {
          Msg(" I'm sealed forever", "", 2000);
          PowerOff();
        }        
        
        // Calculate Direction and display on top line
        float course_to = TinyGPS::course_to(lat, lon, DEST_LATITUDE, DEST_LONGITUDE);
        Serial.println(course_to);  
        clearLCD();    
        if (course_to >= 22.5 && course_to < 67.5)          lcd.print("  Travel Northeast");
        else if (course_to >= 67.5 && course_to < 112.5)    lcd.print("    Travel East");
        else if (course_to >= 112.5 && course_to < 157.5)   lcd.print("  Travel Southeast");
        else if (course_to >= 157.5 || course_to < -157.5)   lcd.print("    Travel South");
        else if (course_to >= -157.5 && course_to < -112.5)   lcd.print("  Travel Southwest");
        else if (course_to >= -112.5 && course_to < -67.5)   lcd.print("    Travel West");
        else if (course_to >= -67.5 && course_to < -22.5)   lcd.print("  Travel Northwest");
        else if (course_to >= -22.5 || course_to < 22.5)    lcd.print("    Travel North");
        else lcd.print("???");
        lcd.println();

        // Display distance on bottom line
        int distance_km = (int)(distance_meters / 1000);
        for (int i = 0; i < (int)(-1*(log(1)/log(10)-4)/2); ++i)
          lcd.print(" ");

        lcd.print("    by ");
        lcd.print(distance_km);
        lcd.print("km");
        
        // Display the major hint within the last few attempts.
        if(attemptCounter >= DEF_ATTEMPT_HINT)
        {
          delay(2000);
          clearLCD();  
          lcd.print("   To Cape Zanpa.");
        }
        
        delay(4000);
      }

      PowerOff();
    }
  }

  /* Turn off after 4 minutes */
  if (millis() >= 240000)
  {
    Msg("   No GPS signal", "Face lid up, outside", 30000);
    PowerOff();
  }
}

/** Called to shut off the system using the Pololu switch */
void PowerOff()
{
  digitalWrite(offPin, HIGH);
  delay(3000);
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
  lcd.print(12, BYTE);  
}
