#include <AMY-Arduino.h>
#include <ArduinoLog.h>
#include <CD74HC4067.h>
#include <CircularBuffer.hpp>

#include <Wire.h>
#include <Adafruit_MCP23X17.h>

#include <MCPKeypad.h>
#include <LiquidCrystal.h>

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

#define I2S_LRC 17
#define I2S_BCLK 4
#define I2S_DIN 16

#define SIG_PIN 12
#define MUX0_PIN 25
#define MUX1_PIN 26
#define MUX2_PIN 27
#define MUX3_PIN 14
#define SET_PIN 33

CD74HC4067 mux_a(MUX0_PIN, MUX1_PIN, MUX2_PIN, MUX3_PIN);

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
CircularBuffer<char, 5> keyPresses;

const int rs = 9, en = 10, d4 = 11, d5 = 12, d6 = 13, d7 = 14;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7, &mcp);

float current_volume = 1.0;

int numVoices = 4;
int current_patch = 256;

int sval = 0;


int last_val = 0;
int note_on = 0;

#define NORMAL 1
#define DRUM 10

int MODE = NORMAL;

#define NOTE_ON 3
#define NOTE_ASCENDING 2
#define NOTE_DESCENDING 1
#define NOTE_OFF 0


long current_micros;


#define TOTAL_SENSORS 16

PianoKey *pianoKeys[TOTAL_SENSORS] = {
  new PianoKey(0, 59, 0),
  new PianoKey(1, 60, 1),
  new PianoKey(2, 61, 0),
  new PianoKey(3, 62, 0),
  new PianoKey(4, 63, 1),
  new PianoKey(5, 64, 0),
  new PianoKey(6, 65, 0),
  new PianoKey(7, 66, 0),
  new PianoKey(8, 67, 1),
  new PianoKey(9, 68, 0),
  new PianoKey(10, 69, 0),
  new PianoKey(11, 70, 0),
  new PianoKey(12, 71, 1),
  new PianoKey(13, 72, 0),
  new PianoKey(14, 73, 0),
  new PianoKey(15, 74, 0),
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

  pinMode(SET_PIN, OUTPUT);
  digitalWrite(SET_PIN, LOW);

  // pinMode(S1, INPUT);
  // for (int i = 0; i < TOTAL; i++) {
  //   pinMode(btns[i], INPUT);
  // }
  delay(1000);  // Give some time for the I2C bus to stabilize

  if (!Wire.begin(21, 22)) {
    Log.info("Initializing Wire library error....." CR);
    while (1)
      ;
  } else {
    Log.info("Initialized Wire Library" CR);
  }
  delay(1000);  // Give some time for the I2C bus to stabilize

  // Initialize the first MCP23017 using I2C with address 0x20
  Log.info("Initializing MCP23017 at address 0x27..." CR);
  if (!mcp.begin_I2C(0x27, &Wire)) {
    Log.error("Error initializing MCP23017 at address 0x27." CR);
    while (1)
      ;
  } else {
    Log.info("Successfully initialized MCP23017 at address 0x27." CR);
  }

  // Configure all pins of both MCP23017s as outputs and turn off all relays initially
  // for (uint8_t pin = 0; pin < 16; pin++) {
  //   mcp.pinMode(pin, OUTPUT);
  //   mcp.digitalWrite(pin, HIGH);  // Ensure relays are off initially (HIGH for low-level trigger relays)
  // }
  delay(1000);
  current_micros = micros();


  keypad.begin();

  // lcd.begin(16, 2);
  // lcd.print("Hello World");

  amy_config_t amy_config = amy_default_config();
  amy_config.features.startup_bleep = 1;
  amy_config.features.default_synths = 1;
  amy_config.i2s_bclk = I2S_BCLK;
  amy_config.i2s_lrc = I2S_LRC;
  amy_config.i2s_dout = I2S_DIN;
  amy_config.features.chorus = 2;
  amy_config.features.reverb = 3;
  amy_start(amy_config);
  amy_live_start();

  initialize_notes();
}


void initialize_notes() {
  //  Serial.println("Amy Test Sequencer");

  amy_event e = amy_default_event();
  e.patch_number = current_patch;
  e.synth = NORMAL;
  e.num_voices = numVoices;
  amy_add_event(&e);
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
  if (newPatch > 0 && newPatch <= 256) {
    current_patch = newPatch;
    Log.info("Setting patch to %d" CR, newPatch);
    amy_event e = amy_default_event();
    e.patch_number = current_patch;
    e.synth = 1;
    e.num_voices = numVoices;
    amy_add_event(&e);

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
        default:
          keyPresses.push(k);
          break;
      }
    }
  }
}

void updateButtons() {

  readSerial();
  readKeyboard();
  readPiano();
}

void playNote(byte note, float velocity) {
  float play_velocity = velocity / 2.0;
  Log.info("Playing note %d %F" CR, note, play_velocity);
  amy_event e = amy_default_event();
  e.velocity = play_velocity;
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

  for (int i = 0; i < TOTAL_SENSORS; i++) {
    PianoKey *key = pianoKeys[i];

    mux_a.channel(key->channel);
    pressure = map(analogRead(SIG_PIN), 0, 4095, 0, 20);
    byte old_state = key->state;

    if (pressure == 0) {
      key->clear();
    }

    if (pressure != key->last_pressure) {
      key->step(pressure, now);
    }

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


void loop() {
  updateButtons();
}
