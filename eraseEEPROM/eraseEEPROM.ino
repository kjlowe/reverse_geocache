#include <EEPROM.h>

void setup()
{
  EEPROM.write(0, 0);
  EEPROM.write(1, 0);
}

void loop()
{
}
