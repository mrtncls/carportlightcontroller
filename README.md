# Carport LED controller
Arduino PWM PIR LED light controller

## Specs: 
 * 20M Ledstrips divided into 3 zones (front, back and shed)
 * Ledstrips are inside anodised alu-U profiles casted with epoxy resin to make the lightsource softer, eye-friendly and waterproof.
 * 3 MOSFETS used for LED dimming over PWM signal
 * 3 PIR sensors (modified chinese PIR sensors housing with Arduino compatible PIR board inside)
 * Analog light sensor
 * 2 pushbuttons (connected to interrupt inputs)
 * 2 120W 12V powersupplies (switched on/off using Omron solid state relays to reduce energy bill)
 * Arduino Duemilanove with regulated always-on 5V Sony USB phonecharger

## Pushbutton linking:
 * Button 1 (right): front LED
 * Button 2 (left): shed LED
 * Button 1+2 (right+left): rear LED

## Pushbutton usage:
 * Press once (on/off/dim): 
   * LED on when LED was off (Exited after 1 hour)
   * LED off for 5 seconds when LED was on. (PIR detection disabled during this period)
   * LED will switch softly on and off. If pressed during this fading, current brightness will be kept. This is dim mode (Exited after 1 hour)
 * Press 2x (glowing): Glow animation. Smoothly switches between different brightness levels (Exited after 120 minutes)
 * Press 3x (lightning): Lightning animation: random flashes at different brightness levels (Exited after 120 minutes)

## PIR usage:
 When dark enough and PIR detects object, LED is switched on for 1 minute. Only when auto mode active.

## Consumption:
Power | Mode | Description
------|------|------------
0,55W | idle | No zone on and PSU's off (only arduino and all sensors running): 0,0024A measured on 230V input
14,26W | PSU's | Arduino switched off. Optocoupler and PSU's switched on: 0,062A
96,14W | back | Back zone on: 0,418A
77,74W | shed | Shed zone on: 0,338A
38,64W | front | Front zone on: 0,168A
166,52W | back + shed | Back and shed zone on: 0,724A
121,67W | back + front | Back and front zone on: 0,529A
111,79W | front + shed | Front and shed zone on: 0,486A
182,62W | back + shed + front | All zones on: 0,794A
