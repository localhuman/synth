#include <ArduinoLog.h>
#include <CircularBuffer.hpp>

#define READING_COUNT 8

#define KEY_OFF 0
#define KEY_WILL_SOUND 1
#define KEY_SOUND 2
#define KEY_UNSET 3

#define PLAY_THRESHOLD 0.7

#define MAX_FORCE 20.0

#pragma once
class PianoKey {

public:
  PianoKey(byte channel_, byte note_, byte sharp_) {
    channel = channel_;
    note = note_;
    sharp = sharp_;
    state = KEY_OFF;

  }

  CircularBuffer<byte, READING_COUNT> pressures;
  CircularBuffer<int, READING_COUNT> times;

  long touch_elapsed() {
    long total= READING_COUNT;
    for(byte i =0; i< READING_COUNT; i++) {
      total+=times[i];
    }
    return total / READING_COUNT;
  }

  int pressure_delta() {
    if(pressures.isEmpty()) {
      return 0;
    }
    return pressures.first() - pressures.last();
  }

  float velocity() {
    return 50.0* pressure_delta() / touch_elapsed();
  }

  float force() {
    return velocity() * pressures.first();
  }
 
  void step(byte pressure, long time) {
      long elapsed = 0;
      if(last_time != 0) {
        elapsed = time - last_time;
      }
      last_pressure = pressure;
      last_time = time;
      pressures.unshift(pressure);
      times.unshift(elapsed);

      float current_velocity = velocity();
      float current_force = force();

      // Log.verbose("%d: new pressure for note %d :%d %l" CR, channel, note, last_pressure, elapsed);
      // Log.verbose("%d: velocity: %F %F" CR, channel, current_velocity, current_force);

      switch(state) {
        case KEY_OFF:
          if(current_force > PLAY_THRESHOLD && pressure > 2) {
            updateState(KEY_WILL_SOUND);
          }
          break;        

        case KEY_WILL_SOUND:
          if(current_force < last_force) {
            peak_force = last_force < MAX_FORCE ? last_force : MAX_FORCE;
            peak_pressure = pressure;
            updateState(KEY_SOUND);
          }
          break;
        
        case KEY_SOUND:
          updateState(KEY_UNSET);
          break;

        case KEY_UNSET:
          int oneThirdPressure = peak_pressure / 4;
          if(pressure <= oneThirdPressure) {
            updateState(KEY_OFF);
          }
          break;
      }

      last_velocity = current_velocity;
      last_force = current_force;


  }


  void updateState(byte newState) {
//    Log.info("%d Changed state from %d to %d" CR, note, state, newState);

    if(newState != state) {
      state = newState;

      // if(state == KEY_SOUND) {
      //   Log.info("%d Sound note with velocity: %F" CR, note, peak_force);
      // } else if(state == KEY_OFF) {

      // }
    }
  }

  void clear() {
    pressures.clear();
    times.clear();
    last_pressure = 0;
    last_time = 0;

    peak_force = 0;
    peak_pressure = 0;

    if(state != KEY_OFF) {
      updateState(KEY_OFF);
    }

  }

  byte last_pressure;
  
  long last_time;
  float last_velocity;
  float last_force;

  float peak_force;
  byte peak_pressure;


  byte channel;
  byte note;
  byte sharp;
  byte state;
};
