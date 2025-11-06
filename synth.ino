#include <AMY-Arduino.h>
#include <ArduinoLog.h>

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

#define I2S_LRC 19
#define I2S_BCLK 14
#define I2S_DIN 27


#define N1 13
#define N2 26
#define N3 25
#define N4 33
#define N5 32
#define N6 35
#define N7 34
#define N8 21
#define N9 4
#define N10 16
#define N11 17
#define N12 5
#define N13 18

#define S1 15

float current_volume = 1.0;

int numVoices = 4;
int current_patch = 256;

#define FILTER_LEN 20
int AN_Pot1_Buffer[FILTER_LEN] = {0};
int AN_Pot1_i;

int sval = 0;

#define TOTAL 13
byte btns[TOTAL] = { N1, N2, N3, N4, N5, N6, N7, N8, N9, N10, N11, N12, N13 };
byte note_start = 60;
byte btn_states[TOTAL] = { HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH };

#define NORMAL 1
#define DRUM 10

int MODE = NORMAL;

struct pressure_sensor_reading {
  int pressure;
  long time;
};

struct pressure_sensor_reading reading;

// crashing patches: 60, 32

void setup() {
  pinMode(I2S_LRC, OUTPUT);
  pinMode(I2S_BCLK, OUTPUT);
  pinMode(I2S_DIN, OUTPUT);

  pinMode(S1, INPUT);
  for (int i = 0; i < TOTAL; i++) {
    pinMode(btns[i], INPUT);
  }


  Serial.begin(115200);

  Log.begin(LOG_LEVEL_INFO, &Serial);
  Log.info("enter the letter l followed by a number between 0 and 7 to change the logging from silent to verbose, for example l2 is error, l6 debug" CR);
  Log.info("enter the letter p followed by a number between 0 and 255 to choose a different preset patch to play, for example p1 or p32" CR);

  reading = {0, micros()};

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

void updatePatch(String value) {
  int newPatch = value.toInt();
  if(newPatch >= 0 && newPatch <= 256) {
    current_patch = newPatch;
    Log.info("Setting patch to %d:" CR, newPatch);
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
  if(level >= 0 && level <=7 ) {
    String levelNames[] = {"Silent", "Fatal", "Error", "Warning", "Info", "Notice", "Trace", "Verbose"};
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
    if (Serial.available()) { // if there is data comming
      String command = Serial.readStringUntil('\n'); // read string until newline character
      char direction = command.charAt(0);
      String subcommand = command.substring(1);
      switch(direction) {
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

void updateButtons() {

  readSerial();
  readSlider();

  for (byte i = 0; i < TOTAL; i++) {
    byte btn = btns[i];
    byte current_state = btn_states[i];
    byte note = note_start + i;

    byte new_state = digitalRead(btn);
    if (new_state != current_state) {
      btn_states[i] = new_state;
      amy_event e = amy_default_event();
      Log.trace("Read pin %d and value is %d with note %d" CR, btn, new_state, note);

      if(MODE == DRUM) {
        if(new_state == LOW) {
          e.osc = 0;
          e.velocity = 1;
          e.preset = i;
        } else {
          e.osc = 0;
          e.velocity = 0;
          e.preset = i;
        }

      } else {
        if (new_state == LOW) {
          // if(current_patch == 256) {
          //   e.volume = .5;
          // } else {
          //   e.volume = 1;
          // }
          e.velocity = 1;
          e.midi_note = note;
          e.synth = 1;
        } else {
          e.velocity = 0;
          e.midi_note = note;
          e.synth = 1;
        }
      }
      amy_add_event(&e);
    }
  }
}


void loop() {
  // Your loop() must contain this call to amy:
  updateButtons();
}


void readSlider() {

  int lp = map(analogRead(S1), 0, 4096, 0, 20);
  if(lp != reading.pressure) {
    struct pressure_sensor_reading old_reading = reading;
    reading = {lp, micros()};
    Log.info("Set pressure val %d" CR, lp);    
    int delta_time = reading.time - old_reading.time;
    int delta_pressure = reading.pressure - old_reading.pressure;
    Log.info("Delta Time %d pressure: %d" CR, delta_time, delta_pressure);    
  }

//   int avg = readADC_Avg(lp);
//   if(avg != sval){
//     sval = avg;

//     float volume = sval / 60.0;
// //    Log.info("Set volume to %F with sval %d" CR, volume, sval);    
//     // amy_event e = amy_default_event();
//     // //e.volume = volume;
//     // e.resonance = volume;
//     // amy_add_event(&e);



//  //   float pitch_bend = (svalFloat / 500.0000);
// //    Serial.println(pitch_bend, 4);
//     // amy_event e = amy_default_event();
//     // e.pitch_bend = pitch_bend;
//     // amy_add_event(&e);

//   }
}
int readADC_Avg(int ADC_Raw)
{
  int i = 0;
  int Sum = 0;
  
  AN_Pot1_Buffer[AN_Pot1_i++] = ADC_Raw;
  if(AN_Pot1_i == FILTER_LEN)
  {
    AN_Pot1_i = 0;
  }
  for(i=0; i<FILTER_LEN; i++)
  {
    Sum += AN_Pot1_Buffer[i];
  }
  return (Sum/FILTER_LEN);
}
