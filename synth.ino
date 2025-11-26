#include <AMY-Arduino.h>
#include <ArduinoLog.h>
#include <CD74HC4067.h>
#include <CircularBuffer.hpp>

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#include <Adafruit_MCP23X17.h>
#include <TM1637DisplayMCP.h>

#include <HT16K33Disp.h>


#include <MCPKeypad.h>
#include <SynthList.h>

#include "PianoKey.h"


// Simple AMY synth setup that just sets up the default MIDI synth.
// Plug a MIDI keyboard into the MIDI in port and play notes, send program changes, etc
// We've tested this with the following boards:
//
// esp32s3 [n32r8]
// rp2040 (Pi Pico) -- use 250Mhz and -O3 for 6 note juno polyphony and turn off serial debug if you have it on
// rp2350 (Pi Pico)
// teensy4
// Electrosmith Daisy
// Please see https://github.com/shorepine/amy/issues/354 for the full list

#define I2S_LRC 1
#define I2S_BCLK 2
#define I2S_DIN 42

#define SIG_PIN 4
#define MUX0_PIN 15
#define MUX1_PIN 7
#define MUX2_PIN 6
#define MUX3_PIN 5
CD74HC4067 mux_a(MUX0_PIN, MUX1_PIN, MUX2_PIN, MUX3_PIN);

#define SIGB_PIN 16
#define MUXB0_PIN 18
#define MUXB1_PIN 17
#define MUXB2_PIN 39
#define MUXB3_PIN 40
CD74HC4067 mux_b(MUXB0_PIN, MUXB1_PIN, MUXB2_PIN, MUXB3_PIN);

// #define SIGC_PIN 10
// #define MUXC0_PIN 17
// #define MUXC1_PIN 18
// #define MUXC2_PIN 8
// #define MUXC3_PIN 3
// CD74HC4067 mux_c(MUXC0_PIN, MUXC1_PIN, MUXC2_PIN, MUXC3_PIN);


Adafruit_MCP23X17 mcp;


const byte ROWS = 4;  // rows
const byte COLS = 4;  // columns
//define the symbols on the buttons of the keypads
char keys[ROWS][COLS] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { 'E', '0', '#', 'D' }
};
byte rowPins[ROWS] = { 3, 2, 1, 0 };  //connect to the row pinouts of the keypad
byte colPins[COLS] = { 7, 6, 5, 4 };  //connect to the column pinouts of the keypad

MCPKeypad keypad = MCPKeypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS, &mcp);

CircularBuffer<char, 3> keyPresses;

LiquidCrystal_I2C lcd(0x26, 16, 2);  // set the LCD address to 0x27 for a 16 chars and 2 line display

// TM1637DisplayMCP digitDisplay(8, 9, &mcp);

String currentFDText = "P256";
HT16K33Disp fdDisplay;


int numVoices = 6;
int current_patch = 256;

float max_velocity = 10.0;
float current_volume = 1.0;

byte keypadCounter = 0;



#define NORMAL 1
#define DRUM 10

int MODE = NORMAL;

long current_micros;


#define TOTAL_SENSORS 30

PianoKey *pianoKeys[TOTAL_SENSORS] = {
  new PianoKey(0, 59, 0, 0, 350.0, 1),
  new PianoKey(1, 60, 1, 0, 600.0, 2),
  new PianoKey(2, 61, 0, 0, 500.0, 7),
  new PianoKey(3, 62, 0, 0, 450.0, 1),
  new PianoKey(4, 63, 1, 0, 450.0, 1),
  new PianoKey(5, 64, 0, 0, 350.0, 1),
  new PianoKey(6, 65, 0, 0, 800.0, 3),
  new PianoKey(7, 66, 0, 0, 200.0, 1),
  new PianoKey(8, 67, 1, 0, 300.0, 1),
  new PianoKey(9, 68, 0, 0, 350.0, 1),
  new PianoKey(10, 69, 0, 0, 350.0, 1),
  new PianoKey(11, 70, 0, 0, 300.0, 5),
  new PianoKey(12, 71, 1, 0, 400.0, 1),
  new PianoKey(13, 72, 0, 0, 500.0, 1),
  new PianoKey(14, 73, 0, 0, 400.0, 2),
  new PianoKey(15, 74, 0, 0, 900.0, 3),

  new PianoKey(0, 58, 0, 0, 300.0, 1),
  new PianoKey(1, 57, 1, 0, 600.0, 1),
  new PianoKey(2, 56, 0, 0, 400.0, 1),
  new PianoKey(3, 55, 0, 0, 500.0, 2),
  new PianoKey(4, 54, 1, 0, 600.0, 3),
  new PianoKey(5, 53, 0, 0, 300.0, 1),
  new PianoKey(6, 52, 0, 0, 350.0, 1),
  new PianoKey(7, 51, 0, 0, 300.0, 1),
  new PianoKey(8, 50, 1, 0, 300.0, 1),
  new PianoKey(9, 49, 0, 0, 500.0, 8),
  new PianoKey(10, 48, 0, 0, 300.0, 1),
  new PianoKey(11, 47, 0, 0, 500.0, 8),
  new PianoKey(12, 46, 1, 0, 400.0, 1),
  new PianoKey(13, 45, 0, 0, 350.0, 6),

};

// crashing patches: 60, 32

void setup() {
  Serial.begin(115200);

  Log.begin(LOG_LEVEL_INFO, &Serial);
  Log.info("enter the letter l followed by a number between 0 and 7 to change the logging from silent to verbose, for example l2 is error, l6 debug" CR);
  Log.info("enter the letter p followed by a number between 0 and 255 to choose a different preset patch to play, for example p1 or p32" CR);

  pinMode(I2S_LRC, OUTPUT);
  pinMode(I2S_BCLK, OUTPUT);
  pinMode(I2S_DIN, OUTPUT);

  pinMode(MUX0_PIN, OUTPUT);
  pinMode(MUX1_PIN, OUTPUT);
  pinMode(MUX2_PIN, OUTPUT);
  pinMode(MUX3_PIN, OUTPUT);
  pinMode(SIG_PIN, INPUT);

  pinMode(MUXB0_PIN, OUTPUT);
  pinMode(MUXB1_PIN, OUTPUT);
  pinMode(MUXB2_PIN, OUTPUT);
  pinMode(MUXB3_PIN, OUTPUT);
  pinMode(SIGB_PIN, INPUT);


  if (!Wire.begin(14, 13)) {
    Log.info("Initializing Wire library error....." CR);
    while (1)

      ;
  } else {
    Log.info("Initialized Wire Library" CR);
  }
  delay(1000);  // Give some time for the I2C bus to stabilize

  // // Initialize the first MCP23017 using I2C with address 0x20
  Log.info("Initializing MCP23017 at address 0x27..." CR);
  if (!mcp.begin_I2C(0x27, &Wire)) {
    Log.error("Error initializing MCP23017 at address 0x27." CR);
    while (1)
      ;
  } else {
    Log.info("Successfully initialized MCP23017 at address 0x27." CR);
  }
  delay(1000);

  lcd.init();
  lcd.backlight();

  // // digitDisplay.init();
  // // digitDisplay.setBrightness(5);

  updateLCD();

  fdDisplay.Init(0x70, 9);
  fdDisplay.Text(0x70, "P256");

  current_micros = micros();


  keypad.begin();

  amy_config_t amy_config = amy_default_config();
  amy_config.features.startup_bleep = 1;
  amy_config.features.default_synths = 0;
  amy_config.i2s_bclk = I2S_BCLK;
  amy_config.i2s_lrc = I2S_LRC;
  amy_config.i2s_dout = I2S_DIN;
  amy_config.features.echo = 0;
  amy_config.features.reverb = 1;
  amy_config.features.chorus = 1;

  amy_start(amy_config);
  amy_live_start();

  initialize_notes();
  scanI2C();
  Log.info("Initialized!");
}


void scanI2C() {
  byte error, address;
  int nDevices;

  Serial.println("Scanning...");

  nDevices = 0;
  for (address = 1; address < 127; address++) {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.print(address, HEX);
      Serial.println("  !");

      nDevices++;
    } else if (error == 4) {
      Serial.print("Unknown error at address 0x");
      if (address < 16)
        Serial.print("0");
      Serial.println(address, HEX);
    }
  }
  if (nDevices == 0)
    Serial.println("No I2C devices found\n");
  else
    Serial.println("done\n");
}

void initialize_notes() {
  amy_event e = amy_default_event();
  e.patch_number = current_patch;
  e.synth = 1;
  e.num_voices = numVoices;
  amy_add_event(&e);
}

void updateLCD() {

  // int switchVal = mcp.digitalRead(10);
  // Log.info("Switch Value is: %d " CR, switchVal);

  lcd.setCursor(0, 0);
  String space = "   ";
  String cp = String("Patch: ");
  String pn = String(current_patch);
  cp.concat(pn);
  cp.concat(space);
  lcd.print(cp);

  // lcd.setCursor(0, 1);
  // String kpStr = String();
  // for(int i = 0; i < keyPresses.size(); i++) {
  //   kpStr.concat(keyPresses[i]);
  // }
  // kpStr.concat(space);
  // lcd.print(kpStr);


//  digitDisplay.showNumberDec(current_patch, false);

  String fdS = "P";
  fdS.concat(current_patch);

  if (fdS != currentFDText) {
    currentFDText = fdS;
    fdDisplay.Clear(0x70);
    fdDisplay.Text(0x70, fdS);

    String currentSynthName = synthList[current_patch];
    Log.info("Current patch name: %s" CR, currentSynthName.c_str());
    lcd.setCursor(0, 1);
    lcd.print(currentSynthName);

  }
}

void updatePatchFromKeys() {
  String kpStr = String();
  while (keyPresses.size()) {
    if (keyPresses.size() > 3) {
      keyPresses.shift();
    } else {
      kpStr.concat(keyPresses.shift());
    }
  }
  Log.info("Kepyressess: %s " CR, kpStr);
  updatePatch(kpStr);
}

void updatePatch(String value) {
  int newPatch = value.toInt();
  if (newPatch >= 0 && newPatch <= 256) {
    current_patch = newPatch;
    Log.info("Setting patch to %d" CR, newPatch);
    amy_event e = amy_default_event();
    e.patch_number = current_patch;
    e.synth = 1;
    e.num_voices = numVoices;
    amy_add_event(&e);
    updateLCD();
  } else {
    Log.warning("Invalid patch %s" CR, value);
  }
}


void updateLogLevel(String value) {
  int level = value.toInt();
  if (level >= 0 && level <= 7) {
    String levelNames[] = { "Silent", "Fatal", "Error", "Warning", "Info", "Notice", "Trace", "Verbose" };
    String levelName = levelNames[level];
    Log.setLevel(LOG_LEVEL_TRACE);
    Log.trace("Setting log level to %d : %s" CR, level, levelName);
    Log.setLevel(level);
  } else {
    int current_log_level = Log.getLevel();
    Log.setLevel(LOG_LEVEL_ERROR);
    Log.error("Log Level option %d not found.  Use an integer between 0 and 7" CR, level);
    Log.setLevel(current_log_level);
  }
}

void readSerial() {
  if (Serial.available()) {                         // if there is data comming
    String command = Serial.readStringUntil('\n');  // read string until newline character
    char direction = command.charAt(0);
    String subcommand = command.substring(1);
    switch (direction) {
      case 108:
      case 76:
        // 'l' or 'L'
        updateLogLevel(subcommand);
        break;
      case 112:
      case 80:
        // 'p' or 'P'
        updatePatch(subcommand);
        break;
      case 100:
      case 68:
        MODE = DRUM;
        Log.info("Updated mode to drum" CR);
        break;
      case 110:
      case 78:
        MODE = NORMAL;
        Log.info("Updated mode to normal" CR);
        break;
    }
    //      int directionInt = direc;
    Log.verbose("Read serial direction: %d command: %s" CR, direction, subcommand);
  }
}


void updateKeyDebug() {
  String kpStr = String();
  while (keyPresses.size()) {
    if (keyPresses.size() > 3) {
      keyPresses.shift();
    } else {
      kpStr.concat(keyPresses.shift());
    }
  }
  int keyNum = kpStr.toInt();
  if (keyNum > -1 && keyNum < TOTAL_SENSORS) {
    Log.info("Will toggle debug for key %d" CR, keyNum);
    PianoKey *key = pianoKeys[keyNum];
    if (key->debug > 0) key->debug = 0;
    else key->debug = 1;
  }
}

void readKeyboard() {
  keypad.tick();
  while (keypad.available()) {
    keypadEvent e = keypad.read();
    //    Serial.print((char)e.bit.KEY);
    //    if(e.bit.EVENT == KEY_JUST_PRESSED) Serial.println(" pressed");
    //    else if(e.bit.EVENT == KEY_JUST_RELEASED) Serial.println(" released");
    if (e.bit.EVENT == KEY_JUST_RELEASED) {

      char k = (char)e.bit.KEY;
      Log.info("Switch on key pressed: %c" CR, k);
      switch (k) {
        case 'E':
          updatePatchFromKeys();
          break;
        case 'D':
          updateKeyDebug();
          break;
        default:
          keyPresses.push(k);
          break;
      }
    }
    updateLCD();
  }
}

void updateButtons() {
  readPiano();

  if (keypadCounter++ > 50) {
    readSerial();
    readKeyboard();
//    readPots();
    keypadCounter = 0;
  }
}

void playNote(byte note, float velocity) {
  float play_velocity = (velocity / MAX_PIANO_KEY_FORCE) * max_velocity;
  Log.info("Playing note %d %F" CR, note, play_velocity);
  amy_event e = amy_default_event();
  e.velocity = play_velocity;
  e.volume = current_volume;
  e.midi_note = note;
  e.synth = 1;
  amy_add_event(&e);
}

void stopNote(byte note) {
  Log.info("Stopping note %d" CR, note);
  amy_event e = amy_default_event();
  e.velocity = 0;
  e.midi_note = note;
  e.synth = 1;
  amy_add_event(&e);
}

void readPiano() {

  long now = micros();
  long elapsed;
  int pressure = 0;

  int pinToRead = SIG_PIN;

  for (int i = 0; i < TOTAL_SENSORS; i++) {
    PianoKey *key = pianoKeys[i];

    if (i < 16) {
      mux_a.channel(key->channel);
      pinToRead = SIG_PIN;
    } else {
      pinToRead = SIGB_PIN;
      mux_b.channel(key->channel);
    }

    pressure = map(analogRead(pinToRead), 0, 4095, 0, 20);

    if (pressure != key->last_pressure) {
      byte old_state = key->state;

      if (pressure == 0) {
        key->clear();
      }

      key->step(pressure, now);

      byte new_state = key->state;
      if (new_state != old_state) {
        if (new_state == KEY_SOUND) {
          playNote(key->note, key->peak_force);
        } else if (new_state == KEY_OFF) {
          stopNote(key->note);
        }
      }
    }
  }
}

int current_octave = 4;
void readPots() {
  // mux_c.channel(0);
  // int octave = map(analogRead(SIGC_PIN), 0, 4095, 0, 7);
  // if(octave != current_octave) {
  //   current_octave = octave;
  //   Log.info("Changing octave to %d " CR, octave);
  // }
}

void loop() {
  updateButtons();
}
