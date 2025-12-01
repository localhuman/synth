
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
#include <Recorder.h>

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

#define I2S_LRC 29
#define I2S_BCLK 30
#define I2S_DIN 31

#define SIG_PIN 20
#define MUX0_PIN 2
#define MUX1_PIN 3
#define MUX2_PIN 4
#define MUX3_PIN 5
CD74HC4067 mux_a(MUX0_PIN, MUX1_PIN, MUX2_PIN, MUX3_PIN);

#define SIGB_PIN 52
#define MUXB0_PIN 49
#define MUXB1_PIN 50
#define MUXB2_PIN 28
#define MUXB3_PIN 51
CD74HC4067 mux_b(MUXB0_PIN, MUXB1_PIN, MUXB2_PIN, MUXB3_PIN);

#define SIGC_PIN 21
#define MUXC0_PIN 27
#define MUXC1_PIN 26
#define MUXC2_PIN 23
#define MUXC3_PIN 22
CD74HC4067 mux_c(MUXC0_PIN, MUXC1_PIN, MUXC2_PIN, MUXC3_PIN);


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

CircularBuffer<char, 3> keysPressed;
CircularBuffer<char, 3> keyPresses;

LiquidCrystal_I2C lcd(0x26, 16, 2);  // set the LCD address to 0x27 for a 16 chars and 2 line display

// TM1637DisplayMCP digitDisplay(8, 9, &mcp);

String currentFDText = "P256";
HT16K33Disp fdDisplay;


int numVoices = 10;
int current_patch = 256;

float max_velocity = 10.0;

#define POTREAD_COUNT 20
#define POTREAD_DELAY 10

float current_volume = 1.0;
CircularBuffer<int, POTREAD_COUNT> vol_avg;

float eq_l = 0.0;
float eq_m = 0.0;
float eq_h = 0.0;
CircularBuffer<int, POTREAD_COUNT> eql_avg;
CircularBuffer<int, POTREAD_COUNT> eqm_avg;
CircularBuffer<int, POTREAD_COUNT> eqh_avg;

int octave = 0;
CircularBuffer<int, POTREAD_COUNT> oct_avg;

byte keypadCounter = 0;



#define NORMAL 1
#define DRUM 10

int MODE = NORMAL;

long current_micros;




#define TOTAL_SENSORS 30

PianoKey *pianoKeys[TOTAL_SENSORS] = {
  new PianoKey(0, 59, 0, 0, 350.0, 1),
  new PianoKey(1, 60, 1, 0, 600.0, 1),
  new PianoKey(2, 61, 0, 0, 200.0, 4),
  new PianoKey(3, 62, 0, 0, 450.0, 1),
  new PianoKey(4, 63, 1, 0, 450.0, 1),
  new PianoKey(5, 64, 0, 0, 350.0, 1),
  new PianoKey(6, 65, 0, 0, 800.0, 1),
  new PianoKey(7, 66, 0, 0, 200.0, 1),
  new PianoKey(8, 67, 1, 0, 300.0, 1),
  new PianoKey(9, 68, 0, 0, 350.0, 1),
  new PianoKey(10, 69, 0, 0, 350.0, 1),
  new PianoKey(11, 70, 0, 0, 300.0, 5),
  new PianoKey(12, 71, 1, 0, 400.0, 1),
  new PianoKey(13, 72, 0, 0, 500.0, 1),
  new PianoKey(14, 73, 0, 0, 400.0, 2),
  new PianoKey(15, 74, 0, 0, 200.0, 2),

  new PianoKey(0, 58, 0, 0, 300.0, 1),
  new PianoKey(1, 57, 1, 0, 600.0, 1),
  new PianoKey(2, 56, 0, 0, 400.0, 1),
  new PianoKey(3, 55, 0, 0, 500.0, 2),
  new PianoKey(4, 54, 1, 0, 600.0, 3),
  new PianoKey(5, 53, 0, 0, 300.0, 1),
  new PianoKey(6, 52, 0, 0, 350.0, 1),
  new PianoKey(7, 51, 0, 0, 300.0, 1),
  new PianoKey(8, 50, 1, 0, 300.0, 1),
  new PianoKey(9, 49, 0, 0, 500.0, 5),
  new PianoKey(10, 48, 0, 0, 300.0, 1),
  new PianoKey(11, 47, 0, 0, 500.0, 5),
  new PianoKey(12, 46, 1, 0, 400.0, 1),
  new PianoKey(13, 45, 0, 0, 350.0, 4),

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

  pinMode(MUXC0_PIN, OUTPUT);
  pinMode(MUXC1_PIN, OUTPUT);
  pinMode(MUXC2_PIN, OUTPUT);
  pinMode(MUXC3_PIN, OUTPUT);
  pinMode(SIGC_PIN, INPUT);


  if (!Wire.begin(7, 8)) {
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
  lcd.print("Hello");

  updateLCD();

  current_micros = micros();


  keypad.begin();

  amy_config_t amy_config = amy_default_config();
  amy_config.features.startup_bleep = 1;
  amy_config.features.default_synths = 0;
  amy_config.i2s_bclk = I2S_BCLK;
  amy_config.i2s_lrc = I2S_LRC;
  amy_config.i2s_dout = I2S_DIN;
  amy_config.max_oscs = 512;

  amy_start(amy_config);
  amy_live_start();

  initialize_notes();
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

  // amy_event e = amy_default_event();
  // e.patch_number = 0;
  // e.synth = 4;
  // e.num_voices = SAMPLE_VOICES;
  // amy_add_event(&e);

  // e = amy_default_event();
  // e.patch_number = 0;
  // e.synth = 2;
  // e.num_voices = SAMPLE_VOICES;
  // amy_add_event(&e);

  // e = amy_default_event();
  // e.patch_number = 0;
  // e.synth = 3;
  // e.num_voices = SAMPLE_VOICES;
  // amy_add_event(&e);

  amy_event e = amy_default_event();
  e.patch_number = current_patch;
  e.synth = 1;
  e.num_voices = numVoices;
  amy_add_event(&e);


}

void updateLCD() {


  lcd.setCursor(0, 0);
  String space = "   ";
  String cp = String("Patch: ");
  String pn = String(current_patch);
  cp.concat(pn);
  cp.concat(space);
  lcd.print(cp);

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
    char k = (char)e.bit.KEY;
    int ki = String(k).toInt();
    if(e.bit.EVENT == KEY_JUST_PRESSED){
      keysPressed.unshift(k);
      if(keysPressedContains('A', '1')) startRecording(1);
      if(keysPressedContains('A', '2')) startRecording(2);
      if(keysPressedContains('A', '3')) startRecording(3);

      if(keysPressedContains('B', '1')) startSample(1);
      if(keysPressedContains('B', '2')) startSample(2);
      if(keysPressedContains('B', '3')) startSample(3);


    }
    if (e.bit.EVENT == KEY_JUST_RELEASED) {

 
      Log.info("Switch on key released: %c" CR, k);

      switch (k) {
        case 'E':
          updatePatchFromKeys();
          break;
        case 'D':
          updateKeyDebug();
          break;
        case '#':
          keyPresses.push(k);
          stopRecording();
          break;
        default:
          keyPresses.push(k);
          break;
      }
      keysPressed.clear();
    }
    updateLCD();
  }
}

void updateButtons() {
  readPiano();

  if (keypadCounter++ > POTREAD_DELAY) {
    readKeyboard();
    readPots();

    keypadCounter = 0;
  }
  loopSample();

}

void startRecording(int slot){
  if(isRecording){
    return;
  }
  isRecording = true;
  Log.info("Started Recording" CR);
  record = {millis()};
  record.noteCount = 0;
  record.isComplete = false;
  record.patch = current_patch;
  record.slot = slot;
}

void stopRecording() {
  if(!isRecording){
    return;
  }
  isRecording = false;
  Log.info("Stopped Recording" CR);
  record.msEnd = millis();
  record.length = record.msEnd - record.msStart;
  record.isComplete = true;
  copyToSlot(&record, record.slot);
}

void loopSample(){
  long now = millis();
  for(int i = 0; i <NUM_SAMPLES; i++) {
    Recording *sample = getSample(i+1); 
    if(sample->isPlaying) {
      if(now > sample->playbackStart + sample->length) {
        Log.info("Ending sample playback %d " CR, sample->slot);
        sample->isPlaying = false;

//        startSample(sample->slot);
      }
    }    
  }
}

void startSample(int slot) {

  Recording *sample = getSample(slot);

  if(sample->isPlaying) return;
  Log.info("Will start sample with slot: %d %d" CR, slot, sample->length);

  amy_event e = amy_default_event();
  e.patch_number = sample->patch;
  e.synth = sample->slot+1;
  e.num_voices = SAMPLE_VOICES;
  amy_add_event(&e);

  long now = amy_sysclock();

  sample->isPlaying = true;
  sample->playbackStart = millis();

  for(int i = 0; i < sample->noteCount; i++) {
    RecordedNote n = sample->notes[i];
    amy_event e = amy_default_event();
    int noteWithOctaveChange = noteFromOctave(n.baseNote, n.octave);
    //e.velocity = play_velocity;
    e.velocity = n.velocity;
    e.volume = current_volume;
    e.midi_note = noteWithOctaveChange;
    e.synth = sample->slot+1;
    e.eq_l = eq_l;
    e.eq_m = eq_m;
    e.eq_h = eq_h;
    e.time = now + n.msStart;
    amy_add_event(&e);

    amy_event eEnd = amy_default_event();
    eEnd.velocity = 0;
    eEnd.midi_note = noteWithOctaveChange;
    eEnd.synth = sample->slot+1;
    eEnd.time = now + n.msStart + n.duration;
    amy_add_event(&eEnd);
  }


}


void playNote(byte note, float velocity) {
  float play_velocity = (velocity / MAX_PIANO_KEY_FORCE) * max_velocity;
  int noteWithOctaveChange = noteFromOctave(note, octave);
  //Log.info("Playing note %d %d %F" CR, note, noteWithOctaveChange, play_velocity);
  amy_event e = amy_default_event();

  //e.velocity = play_velocity;
  e.velocity = 1;
  e.volume = current_volume;
  e.midi_note = noteWithOctaveChange;
  e.synth = 1;
  e.eq_l = eq_l;
  e.eq_m = eq_m;
  e.eq_h = eq_h;
  amy_add_event(&e);

  if(isRecording) {
    long playOffset = millis() - record.msStart;
    RecordedNote n = { playOffset ,NULL,1.0, note, octave, false};
    record.notes[record.noteCount] = n;
    record.noteCount++;
    Log.info("Recording note %d at index %d" CR, note, record.noteCount);
  }

}

void stopNote(byte note) {
  int noteWithOctaveChange = noteFromOctave(note, octave);

  //Log.info("Stopping note %d %d" CR, note, noteWithOctaveChange);
  amy_event e = amy_default_event();
  e.velocity = 0;
  e.midi_note = noteWithOctaveChange;
  e.synth = 1;
  amy_add_event(&e);


  if(isRecording) {
    bool foundNoteToStop = false;
    for(int i= 0; i < record.noteCount; i++) {
      RecordedNote n = record.notes[i];
      if(!n.complete && n.baseNote == note) {
        foundNoteToStop = true;
        n.duration = millis() - n.msStart - record.msStart;
        n.complete = true;
        Log.info("Ended note %d at index %d and duration %l and note started at %l" CR, note, i, n.duration, n.msStart);
        record.notes[i] = n;
      }
    }
    if(!foundNoteToStop) {
      Log.info("Did not find note to stop: %d" CR, note);
    }
  }
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

    pressure = map(analogRead(pinToRead), 0, 3300, 0, 20);

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

#define MAX_33V 3400

void readPots() {
  mux_c.channel(0);

  oct_avg.push(analogRead(SIGC_PIN));
  int oavg = getAvg(oct_avg);
  int oct = 2 - map(oavg, 0, MAX_33V, 0, 5);
  if (oct != octave) {
    octave = oct;
    Log.info("Changing octave to %d %d" CR, octave, oavg);
  }


  mux_c.channel(1);
  vol_avg.push(analogRead(SIGC_PIN));
  float vavg = float(getAvg(vol_avg));

  float v = 4.0 - (4.0 * (vavg / float(MAX_33V)));
  if (v != current_volume) {
    current_volume = v;
  //  Log.info("Changing current Volume to %F" CR, current_volume);
  }

  mux_c.channel(2);
  eql_avg.push(analogRead(SIGC_PIN));
  float eqla = float(getAvg(eql_avg));
  float eqaR = 15.0 - (30.0 * (eqla / float(MAX_33V)));

  if(eqaR != eq_l) {
    eq_l = eqaR;
//    Log.info("Changing current eq to %F" CR, eq_l);
  }


  mux_c.channel(3);
  eqm_avg.push(analogRead(SIGC_PIN));
  float eqma = float(getAvg(eqm_avg));
  float eqmR = 15.0 - (30.0 * (eqma / float(MAX_33V)));

  if(eqmR != eq_m) {
    eq_m = eqmR;
//    Log.info("Changing current eq to %F" CR, eq_l);
  }

  mux_c.channel(4);
  eqh_avg.push(analogRead(SIGC_PIN));
  float eqha = float(getAvg(eqh_avg));
  float eqhR = 15.0 - (30.0 * (eqha / float(MAX_33V)));

  if(eqhR != eq_h) {
    eq_h = eqhR;
//    Log.info("Changing current eq to %F" CR, eq_l);
  }

}


float getAvg(CircularBuffer<int, POTREAD_COUNT>& buffer) {
  int avg = 0;
  for(int i=0; i < POTREAD_COUNT; i++) {
    avg+= buffer[i] / POTREAD_COUNT;
  }
  return avg;
}

bool keysPressedContains(char charA, char charB) {
  bool foundA = false;
  bool foundB = false;

  for(int m=0; m < keysPressed.size(); m++) {
    char n = keysPressed[m];
    if(n ==  charA) {
      foundA = true;
    }
    if(n == charB) {
      foundB = true;
    }
  }
  return foundA && foundB;
}

void loop() {
  updateButtons();
}
