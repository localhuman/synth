#include <ArduinoLog.h>

struct RecordedNote {
  long msStart;
  long duration;
  float velocity;
  int baseNote;
  int octave;
  bool complete;
};

struct Recording {
  long msStart;
  long msEnd;
  long length;
  int noteCount;
  int patch;
  bool isComplete = false;
  bool isPlaying = false;
  long playbackStart;
  RecordedNote notes[1024];
  int slot;
};


bool isRecording = false;
Recording record;

#define NUM_SAMPLES 3

#define SAMPLE_VOICES 6

Recording samples[NUM_SAMPLES];

Recording* getSample(int slot) {
  return &samples[slot-1];
}

void copyToSlot(Recording *s, int slot) {
  samples[slot-1] = *s;
  for(int i = 0; i < s->noteCount; i++) {
    samples[slot-1].notes[i] = s->notes[i];
  }  
}




