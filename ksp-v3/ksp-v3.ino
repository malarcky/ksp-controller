#include <Encoder.h>
#include <Bounce2.h>

// Pin definitions
// Analog joystick axes (already wired)
const int joystickPins[6] = {38, 39, 40, 41, 42, 43}; // 6 analog axes

// Joystick buttons (already wired)
const int joystickButtonPins[2] = {0, 1}; // 2 buttons from joysticks

// Mechanical switches (already wired)
const int mechanicalSwitchPins[16] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17};

// Rotary encoders with push buttons (to be wired)
// For each encoder, we need 2 pins for rotation and 1 for the button
const int encoderPins[2][2] = {
  {18, 19}, // First encoder rotation pins
  {20, 21}  // Second encoder rotation pins
};
const int encoderButtonPins[2] = {22, 23}; // Push buttons for encoders

// Three-position switches (to be wired)
const int threePosSwitch1Pins[2] = {24, 25}; // First three-position switch (needs 2 pins)
const int threePosSwitch2Pins[2] = {26, 27}; // Second three-position switch (needs 2 pins)

// Create encoder objects
Encoder encoder1(encoderPins[0][0], encoderPins[0][1]);
Encoder encoder2(encoderPins[1][0], encoderPins[1][1]);

// Create debounce objects for all buttons
Bounce2::Button joystickButton1 = Bounce2::Button();
Bounce2::Button joystickButton2 = Bounce2::Button();
Bounce2::Button encoderButton1 = Bounce2::Button();
Bounce2::Button encoderButton2 = Bounce2::Button();
Bounce2::Button mechanicalSwitches[16];

// Variables to track encoder positions
long encoder1Pos = 0;
long encoder2Pos = 0;
long oldEncoder1Pos = 0;
long oldEncoder2Pos = 0;

// Variables para manejo avanzado de encoders
unsigned long encoder1PulseStartTime = 0;
unsigned long encoder2PulseStartTime = 0;
const unsigned long encoderPulseDuration = 50; // Duración de cada pulso en ms

// Flags para controlar el estado de los pulsos
bool encoder1CWPulseActive = false;
bool encoder1CCWPulseActive = false;
bool encoder2CWPulseActive = false;
bool encoder2CCWPulseActive = false;

// Variables to track three-position switches
int threePosSwitch1State = 0; // 0 = middle, 1 = up, 2 = down
int threePosSwitch2State = 0;

// For debugging
unsigned long lastDebugTime = 0;
const unsigned long debugInterval = 2000; // 2 seconds

void setup() {
  // Start Serial for debugging
  Serial.begin(9600);
  
  // Initialize USB Joystick
  Joystick.useManualSend(true);
  
  // Configure analog pins
  for (int i = 0; i < 6; i++) {
    pinMode(joystickPins[i], INPUT);
  }
  
  // Configure joystick button pins
  joystickButton1.attach(joystickButtonPins[0], INPUT_PULLUP);
  joystickButton1.interval(10);
  joystickButton1.setPressedState(LOW);  // Since we're using INPUT_PULLUP
  
  joystickButton2.attach(joystickButtonPins[1], INPUT_PULLUP);
  joystickButton2.interval(10);
  joystickButton2.setPressedState(LOW);  // Since we're using INPUT_PULLUP
  
  // Configure mechanical switch pins
  for (int i = 0; i < 16; i++) {
    mechanicalSwitches[i].attach(mechanicalSwitchPins[i], INPUT_PULLUP);
    mechanicalSwitches[i].interval(10);
    mechanicalSwitches[i].setPressedState(LOW);  // Since we're using INPUT_PULLUP
  }
  
  // Configure encoder button pins
  encoderButton1.attach(encoderButtonPins[0], INPUT_PULLUP);
  encoderButton1.interval(10);
  encoderButton1.setPressedState(LOW);  // Since we're using INPUT_PULLUP
  
  encoderButton2.attach(encoderButtonPins[1], INPUT_PULLUP);
  encoderButton2.interval(10);
  encoderButton2.setPressedState(LOW);  // Since we're using INPUT_PULLUP
  
  // Configure three-position switch pins
  for (int i = 0; i < 2; i++) {
    pinMode(threePosSwitch1Pins[i], INPUT_PULLUP);
    pinMode(threePosSwitch2Pins[i], INPUT_PULLUP);
  }
  
  // Give some time for everything to initialize
  delay(1000);
  Serial.println("Joystick controller initialized");
}

void loop() {
  // Read joystick axes and map them to proper range for Joystick library
  // For Teensy 2.0 joystick library, we use these methods:
  Joystick.X(map(analogRead(joystickPins[0]), 0, 1023, 0, 1023));     // X-axis for first joystick
  Joystick.Y(map(analogRead(joystickPins[1]), 0, 1023, 0, 1023));     // Y-axis for first joystick
  Joystick.Z(map(analogRead(joystickPins[2]), 0, 1023, 0, 1023));     // Z-axis for first joystick
  Joystick.Zrotate(map(analogRead(joystickPins[3]), 0, 1023, 0, 1023)); // X-axis for second joystick
  Joystick.sliderLeft(map(analogRead(joystickPins[4]), 0, 1023, 0, 1023));  // Y-axis for second joystick
  Joystick.sliderRight(map(analogRead(joystickPins[5]), 0, 1023, 0, 1023)); // Z-axis for second joystick
  
  // Read joystick buttons
  joystickButton1.update();
  joystickButton2.update();
  Joystick.button(1, joystickButton1.isPressed());
  Joystick.button(2, joystickButton2.isPressed());
  
  // Read mechanical switches (buttons 3-18)
  for (int i = 0; i < 16; i++) {
    mechanicalSwitches[i].update();
    Joystick.button(i + 3, mechanicalSwitches[i].isPressed());
  }
  
  // Read encoder buttons (buttons 19-20)
  encoderButton1.update();
  encoderButton2.update();
  
  bool encoder1Pressed = encoderButton1.isPressed();
  bool encoder2Pressed = encoderButton2.isPressed();
  
  Joystick.button(19, encoder1Pressed);
  Joystick.button(20, encoder2Pressed);
  
  // Leer posiciones actuales de los encoders
  encoder1Pos = encoder1.read() / 4;
  encoder2Pos = encoder2.read() / 4;
  
  // ENCODER 1 - Sistema avanzado de control de pulsos
  // Detectar cambio de posición para Encoder 1
  if (encoder1Pos != oldEncoder1Pos) {
    int diff = encoder1Pos - oldEncoder1Pos;
    
    // Actualizar posición anterior
    oldEncoder1Pos = encoder1Pos;
    
    // Si estaba activo un pulso en dirección contraria, desactivarlo
    if (diff > 0 && encoder1CCWPulseActive) {
      Joystick.button(22, 0);
      encoder1CCWPulseActive = false;
    } else if (diff < 0 && encoder1CWPulseActive) {
      Joystick.button(21, 0);
      encoder1CWPulseActive = false;
    }
    
    // Iniciar nuevo pulso solo si no hay uno activo en esa dirección
    if (diff > 0 && !encoder1CWPulseActive) {
      Joystick.button(21, 1);
      Joystick.button(22, 0);
      encoder1CWPulseActive = true;
      encoder1PulseStartTime = millis();
    } else if (diff < 0 && !encoder1CCWPulseActive) {
      Joystick.button(22, 1);
      Joystick.button(21, 0);
      encoder1CCWPulseActive = true;
      encoder1PulseStartTime = millis();
    }
  }
  
  // Comprobar si es hora de finalizar los pulsos activos del Encoder 1
  unsigned long currentTime = millis();
  if ((encoder1CWPulseActive || encoder1CCWPulseActive) && 
      (currentTime - encoder1PulseStartTime >= encoderPulseDuration)) {
    Joystick.button(21, 0);
    Joystick.button(22, 0);
    encoder1CWPulseActive = false;
    encoder1CCWPulseActive = false;
  }
  
  // ENCODER 2 - Sistema avanzado de control de pulsos
  // Detectar cambio de posición para Encoder 2
  if (encoder2Pos != oldEncoder2Pos) {
    int diff = encoder2Pos - oldEncoder2Pos;
    
    // Actualizar posición anterior
    oldEncoder2Pos = encoder2Pos;
    
    // Si estaba activo un pulso en dirección contraria, desactivarlo
    if (diff > 0 && encoder2CCWPulseActive) {
      Joystick.button(24, 0);
      encoder2CCWPulseActive = false;
    } else if (diff < 0 && encoder2CWPulseActive) {
      Joystick.button(23, 0);
      encoder2CWPulseActive = false;
    }
    
    // Iniciar nuevo pulso solo si no hay uno activo en esa dirección
    if (diff > 0 && !encoder2CWPulseActive) {
      Joystick.button(23, 1);
      Joystick.button(24, 0);
      encoder2CWPulseActive = true;
      encoder2PulseStartTime = millis();
    } else if (diff < 0 && !encoder2CCWPulseActive) {
      Joystick.button(24, 1);
      Joystick.button(23, 0);
      encoder2CCWPulseActive = true;
      encoder2PulseStartTime = millis();
    }
  }
  
  // Comprobar si es hora de finalizar los pulsos activos del Encoder 2
  if ((encoder2CWPulseActive || encoder2CCWPulseActive) && 
      (currentTime - encoder2PulseStartTime >= encoderPulseDuration)) {
    Joystick.button(23, 0);
    Joystick.button(24, 0);
    encoder2CWPulseActive = false;
    encoder2CCWPulseActive = false;
  }
  
  // Read three-position switches
  // For a three-position switch using 2 pins, you can detect 3 states:
  // Both HIGH (center), pin1 LOW (up), pin2 LOW (down)
  int threePosSwitch1Pin1 = digitalRead(threePosSwitch1Pins[0]);
  int threePosSwitch1Pin2 = digitalRead(threePosSwitch1Pins[1]);
  
  if (threePosSwitch1Pin1 == LOW && threePosSwitch1Pin2 == HIGH) {
    threePosSwitch1State = 1; // Up position
  } else if (threePosSwitch1Pin1 == HIGH && threePosSwitch1Pin2 == LOW) {
    threePosSwitch1State = 2; // Down position
  } else {
    threePosSwitch1State = 0; // Center position
  }
  
  int threePosSwitch2Pin1 = digitalRead(threePosSwitch2Pins[0]);
  int threePosSwitch2Pin2 = digitalRead(threePosSwitch2Pins[1]);
  
  if (threePosSwitch2Pin1 == LOW && threePosSwitch2Pin2 == HIGH) {
    threePosSwitch2State = 1; // Up position
  } else if (threePosSwitch2Pin1 == HIGH && threePosSwitch2Pin2 == LOW) {
    threePosSwitch2State = 2; // Down position
  } else {
    threePosSwitch2State = 0; // Center position
  }
  
  // Send the state of three-position switches as buttons
  // First three-position switch uses buttons 25-27
  Joystick.button(25, threePosSwitch1State == 1); // Up
  Joystick.button(40, threePosSwitch1State == 0); // Center
  Joystick.button(27, threePosSwitch1State == 2); // Down
  
  // Second three-position switch uses buttons 28-30
  Joystick.button(28, threePosSwitch2State == 1); // Up
  Joystick.button(40, threePosSwitch2State == 0); // Center
  Joystick.button(30, threePosSwitch2State == 2); // Down
  
  // Send all joystick data to the computer
  Joystick.send_now();
  
  // Debug output every few seconds
  if (millis() - lastDebugTime > debugInterval) {
    lastDebugTime = millis();
    
    Serial.println("Encoder Status:");
    Serial.print("Encoder1 pos: "); Serial.print(encoder1Pos);
    Serial.print(", Button: "); Serial.println(encoder1Pressed ? "Pressed" : "Released");
    Serial.print(", CW Pulse: "); Serial.print(encoder1CWPulseActive ? "Active" : "Inactive");
    Serial.print(", CCW Pulse: "); Serial.println(encoder1CCWPulseActive ? "Active" : "Inactive");
    
    Serial.print("Encoder2 pos: "); Serial.print(encoder2Pos);
    Serial.print(", Button: "); Serial.println(encoder2Pressed ? "Pressed" : "Released");
    Serial.print(", CW Pulse: "); Serial.print(encoder2CWPulseActive ? "Active" : "Inactive");
    Serial.print(", CCW Pulse: "); Serial.println(encoder2CCWPulseActive ? "Active" : "Inactive");
    
    Serial.print("3-pos switch 1: "); Serial.println(threePosSwitch1State);
    Serial.print("3-pos switch 2: "); Serial.println(threePosSwitch2State);
    
    Serial.println("-------------------");
  }
  
  // Small delay to prevent too frequent updates
  delay(5);
}