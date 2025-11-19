#ifndef _MCPKEYPAD_H_
#define _MCPKEYPAD_H_

#include "MCPKeypad_Ringbuffer.h"
#include "Arduino.h"
#include <Adafruit_MCP23X17.h>
#include <string.h>

#define makeKeymap(x) ((byte *)x) ///< cast the passed key characters to bytes

#define KEY_JUST_RELEASED (0) ///< key has been released
#define KEY_JUST_PRESSED (1)  ///< key has been pressed

/**************************************************************************/
/*!
    @brief  key event structure
*/
/**************************************************************************/
union keypadEvent {
  struct {
    uint8_t KEY : 8;   ///< the keycode
    uint8_t EVENT : 8; ///< the edge
    uint8_t ROW : 8;   ///< the row number
    uint8_t COL : 8;   ///< the col number
  } bit;               ///< bitfield format
  uint32_t reg;        ///< register format
};

/**************************************************************************/
/*!
    @brief  Class for interfacing GPIO with a diode-multiplexed keypad
*/
/**************************************************************************/
class MCPKeypad {
public:
  MCPKeypad(byte *userKeymap, byte *row, byte *col, int numRows,
                  int numCols, Adafruit_MCP23X17 *mcp);
  ~MCPKeypad();
  void begin();

  void tick();

  bool justPressed(byte key, bool clear = true);
  bool justReleased(byte key);
  bool isPressed(byte key);
  bool isReleased(byte key);
  int available();
  keypadEvent read();
  void clear();

private:
  byte *_userKeymap;
  byte *_row;
  byte *_col;
  volatile byte *_keystates;
  MCPKeypad_Ringbuffer _eventbuf;

  int _numRows;
  int _numCols;

  Adafruit_MCP23X17 *_mcp;

  volatile byte *getKeyState(byte key);
};

#endif