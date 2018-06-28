/**
 * Carport LED controller
 * 
 * Specs: 
 * 20M Ledstrips divided into 3 zones (front, back and shed)
 * Ledstrips are inside anodised alu-U profiles casted with epoxy resin to make the lightsource softer, eye-friendly and waterproof.
 * 3 MOSFETS used for LED dimming over PWM signal
 * 3 PIR sensors (modified chinese PIR sensors housing with Arduino compatible PIR board inside)
 * 1 analog light sensor
 * 2 pushbuttons (connected to interrupt inputs)
 * 2 120W 12V powersupplies (switched on/off using Omron solid state relays to reduce energy bill)
 * 1 Arduino Leonardo with regulated always-on 5V Sony USB phonecharger
 * 
 * Pushbutton linking:
 * Button 1 (right): front LED
 * Button 2 (left): shed LED
 * Button 1+2 (right+left): rear LED
 * 
 * Pushbutton usage:
 *   --> Press once (on/off/dim): - LED on when LED was off (Exited after 1 hour)
 *                                - LED off for 5 seconds when LED was on. (PIR detection disabled during this period)
 *                                - LED will switch softly on and off. If pressed during this fading, current brightness will be kept. This is dim mode (Exited after 1 hour)
 *   --> Press 2x (glowing): Glow animation. Smoothly switches between different brightness levels (Exited after 120 minutes)
 *   --> Press 3x (lightning): Lightning animation: random flashes at different brightness levels (Exited after 120 minutes)
 *                            
 * PIR usage:
 *   --> When dark enough and PIR detects object, LED is switched on for 1 minute. Only when auto mode active.
 *   
 * Consumption:
 *     0,55W idle                 --> No zone on and PSU's off (only arduino and all sensors running): 0,0024A measured on 230V input
 *    14,26W PSU's                --> Arduino switched off. Optocoupler and PSU's switched on: 0,062A
 *    96,14W back                 --> Back zone on: 0,418A
 *    77,74W shed                 --> Shed zone on: 0,338A
 *    38,64W front                --> Front zone on: 0,168A
 *   166,52W back + shed          --> Back and shed zone on: 0,724A
 *   121,67W back + front         --> Back and front zone on: 0,529A
 *   111,79W front + shed         --> Front and shed zone on: 0,486A
 *   182,62W back + shed + front  --> All zones on: 0,794A
 */


const int MODE_AUTO = 1; // Use PIR sensor and light sensor to switch LEDs on for a period of time
const int MODE_AUTO_ON = 2;  // LEDs on after PIR triggered
const int MODE_ON = 3;  // Switch LEDs on for a period of time
const int MODE_OFF = 4; // Switch LEDs off for a period of time
const int MODE_DIM = 5; // Keep LEDs dimmed for a period of time
const int MODE_GLOW = 6; // Glow animation
const int MODE_LIGHTNING = 7; // Lightning animation

const long MODE_AUTO_ON_TIMEOUT = 60000; // Time spend in auto-on mode. Switch to auto mode when elapsed. [ms]
const long MODE_ON_TIMEOUT = 3600000; // Max time spend in on mode. Switch to auto mode when elapsed. [ms]
const long MODE_OFF_TIMEOUT = 5000; // Max time spend in off mode. Switch to auto mode when elapsed. [ms]
const long MODE_DIM_TIMEOUT = 3600000; // Max time spend in dim mode. Switch to auto mode when elapsed. [ms]
const long MODE_GLOW_TIMEOUT = 3600000; // Max time spend in glow mode. Switch to auto mode when elapsed. [ms]
const long MODE_LIGHTNING_TIMEOUT = 3600000; // Max time spend in lightning mode. Switch to auto mode when elapsed. [ms]

const int PIN_LED_FRONT = 9;
const int PIN_LED_SHED = 10;
const int PIN_LED_BACK = 11;

const int PIN_PSU_TOP = 12;
const int PIN_PSU_BOTTOM = 13;

const int PIN_BTN_RIGHT = 2;
const int PIN_BTN_LEFT = 3;

const int PIN_PIR_FRONT = 4;
const int PIN_PIR_SHED = 7;
const int PIN_PIR_BACK = 8;

const int PIN_LIGHT_SENSOR = 0;
const int LIGHT_LOGGING_INTERVAL = 5000; // [ms]
const int LIGHT_THRESHOLD = 230; // Allows LED's on in idle mode when above this threshold. Range: 0 - 1023

const int BUTTON_REPEAT_TIME = 0; //250; No repeats double/triple click modes at the moment - set to low value.
const int BUTTON_MIN_PRESS_TIME = 50; // Pushbutton pulse needs to be longer than this. Filters unwanted bouncing pulses.

const int SOFTON_STEP = 1;
const int SOFTON_INTERVAL = 10; // [ms]
const int SOFTOFF_STEP = 1;
const int SOFTOFF_INTERVAL = 20; // [ms]

const int PSU_TOP_STARTUPTIME = 1000; // PSU start takes some time. This delay is required to get a smooth fade (SoftOn function). [ms]
const int PSU_BOTTOM_STARTUPTIME = 1000; // PSU start takes some time. This delay is required to get a smooth fade (SoftOn function). [ms]

struct Zone
{
  char Name[20];
  
  int Brightness; // LED brightness. Range: 0 - 255
  
  int Mode;
  long ModeBeginTS; // Timestamp of modestart

  long SoftOnOff_LastSet; // Soft on/off helper

  bool PIR_RTRIG; // PIR trigger rising edge (high for 1 cycle)
  bool LastPIR; // RTRIG helper

  long ButtonLastDown;
  int ButtonRepeats;
  bool ButtonLastState;
  bool ButtonHandled;

  bool PSU_Top; // Required top PSU to be on
  bool PSU_Bottom; // Required bottom PSU to be on
};

struct Button
{
  long LastUp;
  bool Down;
};

struct EnvLight
{
  int Last; // Last value with all LED's switched off
  long LastTS;
  
  long LoggedTS;
};

struct PSU
{
  bool Last;
  long SwitchOnTS;
  bool LoggedLastOn;
};

Zone Front = {"front", 0, MODE_AUTO, 0, 0, false, false, 0, 0, false, false, true, false};
Zone Shed = {"shed", 0, MODE_AUTO, 0, 0, false, false, 0, 0, false, false, false, true};
Zone Back = {"back", 0, MODE_AUTO, 0, 0, false, false, 0, 0, false, false, true, true};
Button LeftBtn = {0, false};
Button RightBtn = {0, false};
Button BothBtn = {0, false};
EnvLight Light = {0, 0, 0};
PSU PSUTop = {false, 0, false};
PSU PSUBottom = {false, 0, false};

void setup() {
  pinMode( PIN_LED_FRONT, OUTPUT);
  pinMode( PIN_LED_SHED, OUTPUT);
  pinMode( PIN_LED_BACK, OUTPUT);

  pinMode( PIN_PSU_TOP, OUTPUT);
  pinMode( PIN_PSU_BOTTOM, OUTPUT);

  pinMode( PIN_BTN_RIGHT, INPUT);
  pinMode( PIN_BTN_LEFT, INPUT);

  pinMode( PIN_PIR_FRONT, INPUT);
  pinMode( PIN_PIR_SHED, INPUT);
  pinMode( PIN_PIR_BACK, INPUT);

  Serial.begin(9600);
}

void loop() 
{
  bool allLightsOff = 
    Front.Brightness == 0 &&
    Shed.Brightness == 0 &&
    Back.Brightness == 0;

  /**
   * Environmental light
   */
  bool autoLightActive = checkLight(analogRead(PIN_LIGHT_SENSOR), &Light, allLightsOff);

  /**
   * PIR sensors
   */
  checkPIR(digitalRead(PIN_PIR_FRONT), &Front);
  checkPIR(digitalRead(PIN_PIR_SHED), &Shed);
  checkPIR(digitalRead(PIN_PIR_BACK), &Back);

  /**
   * Buttons
   */  
  bool rightDown = digitalRead(PIN_BTN_RIGHT);
  bool leftDown = digitalRead(PIN_BTN_LEFT);
  bool bothDown = rightDown && leftDown;
  if (bothDown)
  {
    rightDown = false;
    leftDown = false;
  }

  if (!RightBtn.Down && RightBtn.LastUp + BUTTON_MIN_PRESS_TIME < millis())
  {
    //Serial.println("Right down");
    RightBtn.Down = true;
  }
  if (!rightDown) 
  {
    RightBtn.LastUp = millis();
    if (RightBtn.Down)
    {
      RightBtn.Down = false;
      //Serial.println("Right up");
    }      
  }

  if (!LeftBtn.Down && LeftBtn.LastUp + BUTTON_MIN_PRESS_TIME < millis())
  {
    //Serial.println("Left down");
    LeftBtn.Down = true;
  }
  if (!leftDown) 
  {
    LeftBtn.LastUp = millis();
    if (LeftBtn.Down)
    {
      LeftBtn.Down = false;
      //Serial.println("Left up");        
    }      
  }

  if (!BothBtn.Down && BothBtn.LastUp + BUTTON_MIN_PRESS_TIME < millis())
  {
    //Serial.println("Both down");
    BothBtn.Down = true;
  }
  if (!bothDown) 
  {
    BothBtn.LastUp = millis();
    if (BothBtn.Down)
    {
      BothBtn.Down = false;
      //Serial.println("Both up");        
    }      
  }

  checkButton(&Front, RightBtn.Down);
  checkButton(&Shed, LeftBtn.Down);
  checkButton(&Back, BothBtn.Down);

  /**
   * Calc target LED brightness
   */
  calcBrightness(&Front, autoLightActive);
  calcBrightness(&Shed, autoLightActive);
  calcBrightness(&Back, autoLightActive);

  /**
   * LED power supplies
   */
  bool needTopPSU = PSUTop.SwitchOnTS + 500 > millis() || // Leave PSU for some time after startup (let calc brightness do his thing)
                    (Back.PSU_Top && Back.Brightness > 0) || 
                    (Front.PSU_Top && Front.Brightness > 0) || 
                    (Shed.PSU_Top && Shed.Brightness > 0);
  bool needBottomPSU = PSUBottom.SwitchOnTS + 500 > millis() || // Leave PSU for some time after startup (let calc brightness do his thing)
                       (Back.PSU_Bottom && Back.Brightness > 0) || 
                       (Front.PSU_Bottom && Front.Brightness > 0) || 
                       (Shed.PSU_Bottom && Shed.Brightness > 0);
                       
  if (needTopPSU && needTopPSU != PSUTop.Last)
  {
    Serial.print(millis(), DEC);
    Serial.println(" - Top PSU starting...");
    
    digitalWrite(PIN_PSU_TOP, LOW); // Inverted signal
    PSUTop.SwitchOnTS = millis() + PSU_TOP_STARTUPTIME;
    PSUTop.LoggedLastOn = false;
  }
  PSUTop.Last = needTopPSU;
  if (PSUTop.SwitchOnTS > millis())
  {
    // Delay on
    if (Back.PSU_Top) Back.Brightness = 0;    
    if (Front.PSU_Top) Front.Brightness = 0;    
    if (Shed.PSU_Top) Shed.Brightness = 0;    
  }
  else
  {
    if (needTopPSU && !PSUTop.LoggedLastOn)
    {
      Serial.print(millis(), DEC);
      Serial.println(" - Top PSU ready");
      PSUTop.LoggedLastOn = true;
    }
    
    digitalWrite(PIN_PSU_TOP, !needTopPSU); // Inverted signal
  }

  if (needBottomPSU && needBottomPSU != PSUBottom.Last) 
  {
    Serial.print(millis(), DEC);
    Serial.println(" - Bottom PSU starting...");

    digitalWrite(PIN_PSU_BOTTOM, LOW); // Inverted signal
    PSUBottom.SwitchOnTS = millis() + PSU_BOTTOM_STARTUPTIME;
    PSUBottom.LoggedLastOn = false;
  }
  PSUBottom.Last = needBottomPSU;
  if (PSUBottom.SwitchOnTS > millis())
  {
    // Delay on
    if (Back.PSU_Bottom) Back.Brightness = 0;    
    if (Front.PSU_Bottom) Front.Brightness = 0;    
    if (Shed.PSU_Bottom) Shed.Brightness = 0;    
  }
  else
  {
    if (needBottomPSU && !PSUBottom.LoggedLastOn)
    {
      Serial.print(millis(), DEC);
      Serial.println(" - Bottom PSU ready");
      PSUBottom.LoggedLastOn = true;
    }

    digitalWrite(PIN_PSU_BOTTOM, !needBottomPSU); // Inverted signal
  }


  /**
   * Set LED brightness
   */
  analogWrite(PIN_LED_FRONT, Front.Brightness);
  analogWrite(PIN_LED_SHED, Shed.Brightness);
  analogWrite(PIN_LED_BACK, Back.Brightness);
}

bool checkLight(int input, EnvLight* el, bool allLightsOff)
{
  if (allLightsOff)
  {
    el->Last = input;
    el->LastTS = millis();  
  }

  bool autoLightActive = el->Last > LIGHT_THRESHOLD;
  
  if (el->LoggedTS + LIGHT_LOGGING_INTERVAL < millis())
  {
    Serial.print("Environment light changed to ");
    Serial.print(input, DEC);   
    if (autoLightActive) Serial.print(" (auto light switching active)");
    Serial.println();

    el->LoggedTS = millis();
  }

  return autoLightActive;
}

void checkPIR(bool input, Zone* z)
{
  if (input && !z->LastPIR)
  {
    z->PIR_RTRIG = true;
    Serial.print("PIR trigger in zone '");    
    Serial.print(z->Name);
    Serial.println("'");    
  }
  else
  {
    z->PIR_RTRIG = false;
  }
  z->LastPIR = input;
}

void checkButton(Zone* z, bool down)
{
  if ((z->Mode == MODE_AUTO_ON && z->ModeBeginTS + MODE_AUTO_ON_TIMEOUT < millis()) ||
      (z->Mode == MODE_ON && z->ModeBeginTS + MODE_ON_TIMEOUT < millis()) ||
      (z->Mode == MODE_OFF && z->ModeBeginTS + MODE_OFF_TIMEOUT < millis()) ||
      (z->Mode == MODE_DIM && z->ModeBeginTS + MODE_DIM_TIMEOUT < millis()) ||
      (z->Mode == MODE_GLOW && z->ModeBeginTS + MODE_GLOW_TIMEOUT < millis()) ||
      (z->Mode == MODE_LIGHTNING && z->ModeBeginTS + MODE_LIGHTNING_TIMEOUT < millis()))      
  {
    z->Mode = MODE_AUTO;
    z->ModeBeginTS = millis();

    Serial.print("Zone '");
    Serial.print(z->Name);
    Serial.println("' auto mode. Previous mode timed out.");
  }
  
  if (down && z->ButtonLastState != down)
  {
    if (z->ButtonLastDown + BUTTON_REPEAT_TIME > millis())
    {
      z->ButtonRepeats++;
    }
    else
    {
      z->ButtonRepeats = 1;
    }

    z->ButtonHandled = false;
    z->ButtonLastDown = millis();

    Serial.print("Zone '");
    Serial.print(z->Name);
    Serial.println("' button down");
  }
  z->ButtonLastState = down;

  // Handle when repeat time is over
  if (z->ButtonHandled || z->ButtonLastDown + BUTTON_REPEAT_TIME > millis()) return;

  z->ButtonHandled = true;

  switch(z->ButtonRepeats)
  {
    case 1:

      if (//(z->Brightness != 255 && z->Mode == MODE_ON) || Only goto dim mode when going off
          (z->Brightness != 0 && z->Mode == MODE_OFF))
      {
        // Dim
        z->Mode = MODE_DIM;
        z->ModeBeginTS = millis();

        Serial.print("Zone '");
        Serial.print(z->Name);
        Serial.println("' dim mode");
      }
      else if (z->Brightness == 0 || 
               z->Mode == MODE_OFF) // Allows to SoftOn when SoftOff running
      {          
        // On
        z->Mode = MODE_ON;
        z->ModeBeginTS = millis();

        Serial.print("Zone '");
        Serial.print(z->Name);
        Serial.println("' on mode");
      }
      else
      {
        // Off
        z->Mode = MODE_OFF;
        z->ModeBeginTS = millis();

        Serial.print("Zone '");
        Serial.print(z->Name);
        Serial.println("' off mode");
      }
    
      break;
/**    case 2: // Glow
      z->Mode = MODE_GLOW;
      z->ModeBeginTS = millis();

      Serial.print("Zone '");
      Serial.print(z->Name);
      Serial.println("' glow mode");
      break;
    case 3: // Lightning
      z->Mode = MODE_LIGHTNING;
      z->ModeBeginTS = millis();

      Serial.print("Zone '");
      Serial.print(z->Name);
      Serial.println("' lightning mode");
      break;*/
  }
}

void calcBrightness(Zone* z, bool autoLightActive)
{
  switch(z->Mode)
  {
    case MODE_AUTO:    
      if (autoLightActive && z->PIR_RTRIG)
      {
        z->Mode = MODE_AUTO_ON;
        z->ModeBeginTS = millis();
      }
      SoftOff(z);
      break;
    case MODE_ON:
    case MODE_AUTO_ON:
      SoftOn(z);
      break;
    case MODE_OFF:
      SoftOff(z);
      break;
    case MODE_DIM:
      // Do nothing. Keep brightness at current level.      
      break;
  }
}

void SoftOn(Zone* z)
{
  if (z->Brightness < 255)
  {
    if (z->SoftOnOff_LastSet + SOFTON_INTERVAL < millis())
    {
      z->Brightness += SOFTON_STEP;
      z->SoftOnOff_LastSet = millis();
    }
  }    
  else  
    z->Brightness = 255;
}

void SoftOff(Zone* z)
{
  if (z->Brightness > 0)
  {
    if (z->SoftOnOff_LastSet + SOFTOFF_INTERVAL < millis())
    {
      z->Brightness -= SOFTOFF_STEP;
      z->SoftOnOff_LastSet = millis();
    }
  }
  else
    z->Brightness = 0;
}
