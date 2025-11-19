#include "MCPKeypad.h"
#include <Adafruit_MCP23X17.h>

#define _KEY_PRESSED_POS (1)
#define _KEY_PRESSED (1UL << _KEY_PRESSED_POS)

#define _JUST_PRESSED_POS (2)
#define _JUST_PRESSED (1UL << _JUST_PRESSED_POS)

#define _JUST_RELEASED_POS (3)
#define _JUST_RELEASED (1UL << _JUST_RELEASED_POS)

#define _KEYPAD_SETTLING_DELAY 20

/**************************************************************************/
/*!
    @brief  default constructor
    @param  userKeymap a multidimensional array of key characters
    @param  row an array of GPIO pins that are connected to each row of the
   keypad
    @param  col an array of GPIO pins that are connected to each column of the
   keypad
    @param  numRows the number of rows on the keypad
    @param  numCols the number of columns on the keypad
*/
/**************************************************************************/
MCPKeypad::MCPKeypad(byte *userKeymap, byte *row, byte *col,
                                 int numRows, int numCols, Adafruit_MCP23X17 *mcp) {
  _userKeymap = userKeymap;
  _row = row;
  _col = col;
  _numRows = numRows;
  _numCols = numCols;

  _mcp = mcp;

  _keystates = NULL;
}

/**************************************************************************/
/*!
    @brief  default destructor
*/
/**************************************************************************/
MCPKeypad::~MCPKeypad() {
  if (_keystates != NULL) {
    free((void *)_keystates);
  }
}

/**************************************************************************/
/*!
    @brief  get the state of a key with the given name
    @param  key the name of the key to be checked
*/
/**************************************************************************/
volatile byte *MCPKeypad::getKeyState(byte key) {
  for (int i = 0; i < _numRows * _numCols; i++) {
    if (_userKeymap[i] == key) {
      return _keystates + i;
    }
  }
  return NULL;
}

/**************************************************************************/
/*!
    @brief  read the array of switches and place any events in the buffer.
*/
/**************************************************************************/
void MCPKeypad::tick() {
  uint8_t evt;
  for (int i = 0; i < _numCols; i++) {
    _mcp->digitalWrite(_col[i], HIGH);
  }

  int i = 0;
  for (int c = 0; c < _numCols; c++) {
    _mcp->digitalWrite(_col[c], LOW);
    delayMicroseconds(_KEYPAD_SETTLING_DELAY);
    for (int r = 0; r < _numRows; r++) {
      i = r * _numCols + c;
      bool pressed = !_mcp->digitalRead(_row[r]);
      // Serial.print((int)pressed);
      volatile byte *state = _keystates + i;
      byte currentState = *state;
      if (pressed && !(currentState & _KEY_PRESSED)) {
        currentState |= (_JUST_PRESSED | _KEY_PRESSED);
        evt = KEY_JUST_PRESSED;
        _eventbuf.store_char(evt);
        _eventbuf.store_char(*(_userKeymap + i));
        _eventbuf.store_char(r);
        _eventbuf.store_char(c);
      } else if (!pressed && (currentState & _KEY_PRESSED)) {
        currentState |= _JUST_RELEASED;
        currentState &= ~(_KEY_PRESSED);
        evt = KEY_JUST_RELEASED;
        _eventbuf.store_char(evt);
        _eventbuf.store_char(*(_userKeymap + i));
        _eventbuf.store_char(r);
        _eventbuf.store_char(c);
      }
      *state = currentState;
    }
    // Serial.println("");
    _mcp->digitalWrite(_col[c], HIGH);
  }
}

/**************************************************************************/
/*!
    @brief  set all the pin modes and set up variables.
*/
/**************************************************************************/
void MCPKeypad::begin() {
  _keystates = (volatile byte *)malloc(_numRows * _numCols);
  memset((void *)_keystates, 0, _numRows * _numCols);

  for (int i = 0; i < _numCols; i++) {
    _mcp->pinMode(_col[i], OUTPUT);
    _mcp->digitalWrite(_col[i], HIGH);
  }

  for (int i = 0; i < _numRows; i++) {
    _mcp->pinMode(_row[i], INPUT_PULLUP);
  }
}

/**************************************************************************/
/*!
    @brief  check if the given key has just been pressed since the last tick.
    @param  key the name of the key to be checked
    @param  clear whether to reset the state (default yes) post-check
    @returns    true if it has been pressed, false otherwise.
*/
/**************************************************************************/
bool MCPKeypad::justPressed(byte key, bool clear) {
  volatile byte *state = getKeyState(key);
  bool val = (*state & _JUST_PRESSED) != 0;

  if (clear)
    *state &= ~(_JUST_PRESSED);

  return val;
}

/**************************************************************************/
/*!
    @brief  check if the given key has just been released since the last tick.
    @param  key the name of the key to be checked
    @returns    true if it has been released, false otherwise.
*/
/**************************************************************************/
bool MCPKeypad::justReleased(byte key) {
  volatile byte *state = getKeyState(key);
  bool val = (*state & _JUST_RELEASED) != 0;

  *state &= ~(_JUST_RELEASED);

  return val;
}

/**************************************************************************/
/*!
    @brief  check if the given key is currently pressed
    @param  key the name of the key to be checked
    @returns    true if it is currently pressed, false otherwise.
*/
/**************************************************************************/
bool MCPKeypad::isPressed(byte key) {
  return (*getKeyState(key) & _KEY_PRESSED) != 0;
}

/**************************************************************************/
/*!
    @brief  check if the given key is currently released
    @param  key the name of the key to be checked
    @returns    true if it is currently released, false otherwise.
*/
/**************************************************************************/
bool MCPKeypad::isReleased(byte key) {
  return (*getKeyState(key) & _KEY_PRESSED) == 0;
}

/**************************************************************************/
/*!
    @brief  check how many events are in the keypads buffer
    @returns    the number of events currently in the buffer
*/
/**************************************************************************/
int MCPKeypad::available() {
  return (_eventbuf.available() / sizeof(keypadEvent));
}

/**************************************************************************/
/*!
    @brief  pop the next event off of the FIFO
    @returns    the next event in the FIFO
*/
/**************************************************************************/
keypadEvent MCPKeypad::read() {
  keypadEvent k;
  k.bit.EVENT = _eventbuf.read_char();
  k.bit.KEY = _eventbuf.read_char();
  k.bit.ROW = _eventbuf.read_char();
  k.bit.COL = _eventbuf.read_char();

  return k;
}

/**************************************************************************/
/*!
    @brief Clear out the event buffer and all the key states
*/
/**************************************************************************/
void MCPKeypad::clear() {
  _eventbuf.clear();
  for (int i = 0; i < _numRows * _numCols; i++)
    *(_keystates + i) = 0;
}