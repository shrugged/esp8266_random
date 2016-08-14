#include <elapsedMillis.h>

#define NUM_BTN_COLUMNS 16
#define NUM_BTN_ROWS 4

#define NUM_LEDS 16
#define NUM_BTNS 16
#define NUM_COLORS 3

// Uncomment this line to send debug messages to the serial monitor
#define ARDUINOME

#define MIDI_CHANNEL 1
#define POOL_TIME 10

// MIDI mapping taken from http://www.nortonmusic.com/midi_cc.html
#define MIDI_CC_MODULATION 0x01
#define MIDI_CC_BREATH 0x02
#define MIDI_CC_VOLUME 0x07
#define MIDI_CC_BALANCE 0x08
#define MIDI_CC_PAN 0x0A
#define MIDI_CC_EXPRESSION 0x0B
#define MIDI_CC_EFFECT1 0x0C
#define MIDI_CC_EFFECT2 0x0D

#define MIDI_CC_GENERAL1 0x0E
#define MIDI_CC_GENERAL2 0x0F
#define MIDI_CC_GENERAL3 0x10
#define MIDI_CC_GENERAL4 0x11
#define MIDI_CC_GENERAL5 0x12
#define MIDI_CC_GENERAL6 0x13
#define MIDI_CC_GENERAL7 0x14
#define MIDI_CC_GENERAL8 0x15
#define MIDI_CC_GENERAL9 0x16
#define MIDI_CC_GENERAL10 0x17
#define MIDI_CC_GENERAL11 0x18
#define MIDI_CC_GENERAL12 0x19
#define MIDI_CC_GENERAL13 0x1A
#define MIDI_CC_GENERAL14 0x1B
#define MIDI_CC_GENERAL15 0x1C
#define MIDI_CC_GENERAL16 0x1D
#define MIDI_CC_GENERAL17 0x1E
#define MIDI_CC_GENERAL18 0x1F

#define MIDI_CC_GENERAL1_FINE 0x2E
#define MIDI_CC_GENERAL2_FINE 0x2F
#define MIDI_CC_GENERAL3_FINE 0x30
#define MIDI_CC_GENERAL4_FINE 0x31
#define MIDI_CC_GENERAL5_FINE 0x32
#define MIDI_CC_GENERAL6_FINE 0x33
#define MIDI_CC_GENERAL7_FINE 0x34
#define MIDI_CC_GENERAL8_FINE 0x35
#define MIDI_CC_GENERAL9_FINE 0x36
#define MIDI_CC_GENERAL10_FINE 0x37
#define MIDI_CC_GENERAL11_FINE 0x38
#define MIDI_CC_GENERAL12_FINE 0x39
#define MIDI_CC_GENERAL13_FINE 0x3A
#define MIDI_CC_GENERAL14_FINE 0x3B
#define MIDI_CC_GENERAL15_FINE 0x3C
#define MIDI_CC_GENERAL16_FINE 0x3D
#define MIDI_CC_GENERAL17_FINE 0x3E
#define MIDI_CC_GENERAL18_FINE 0x3F

#define MIDI_CC_SUSTAIN 0x40
#define MIDI_CC_REVERB 0x5B
#define MIDI_CC_CHORUS 0x5D
#define MIDI_CC_CONTROL_OFF 0x79
#define MIDI_CC_NOTES_OFF 0x78

#define NOTE_C0 0x00 // 0
#define NOTE_C1 0x12 // 18
#define NOTE_C2 0x24 // 36

#define MIDI_CC MIDI_CC_GENERAL1

// Comment this line out to disable button debounce logic.
// See http://arduino.cc/en/Tutorial/Debounce on what debouncing is used for.
#define DEBOUNCE
// Debounce time length in milliseconds
#define DEBOUNCE_LENGTH 2

elapsedMillis pool_time = 0;

//Switch1, Switch2, Switch3, Switch4
static const uint8_t btnreadpins[NUM_BTN_ROWS] = {6, 7, 8, 9};

static const uint8_t dataPin = 2;
static const uint8_t clockPin = 1;
static const uint8_t latchPin = 0;

uint8_t note[NUM_BTN_ROWS][NUM_BTN_COLUMNS] = {
  {96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111},
  {80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95},
  {64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79},
  {48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63}
};

uint16_t bits[] = {0xfffe, 0xFFFD, 0xFFFB, 0xFFF7, 0xFFEF, 0xFFDF, 0xFFBF, 0xFF7F, 0xFEFF, 0xFDFF, 0xFBFF, 0xF7FF, 0xEFFF, 0xDFFF, 0xBFFF, 0x7FFF};

boolean WaitingCommand = true;
byte byte0, byte1, row = 0;
int address, command = 0 ;

#ifdef DEBUG
unsigned long loopTime = 0;
unsigned long serialSendTime = 0;
#endif

byte prev_buttons[NUM_BTN_COLUMNS][NUM_BTN_ROWS];

void setup() {
  Serial.begin(57600);

  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);

  for (uint16_t col = 0; col < NUM_BTN_COLUMNS; col++)
  {
    digitalWrite(latchPin, LOW);
    shiftOut(dataPin, clockPin, MSBFIRST, (0xffff) >> 8);
    shiftOut(dataPin, clockPin, MSBFIRST, 0xff);
    digitalWrite(latchPin, HIGH);

    for (uint16_t row = 0; row < NUM_BTN_ROWS; row++)
    {
      pinMode(btnreadpins[row], INPUT_PULLUP);
      prev_buttons[col][row] = HIGH;
    }
  };
}

void checkSerial()
{
  do
  {
    if (Serial.available())
    {
      if (WaitingCommand)        // command is the first byte of the two byte command
      {
        byte0 = Serial.read();
        command = byte0 >> 4;
        WaitingCommand = false;

        switch (command) // do one byte commands
        {
          case 9: // clear command
            WaitingCommand = true; // next byte is a new command
            break;
        }

        if ((command > 9) || (command < 2))  WaitingCommand = true; // command outside range so must be an error
      }

      else
      {
        WaitingCommand = true;    // the next byte is the first byte of the next command
        byte1 = Serial.read();    // read the second byte of this command

        switch (command)
        {

          case 2:  // LED command
            break;
          case 3:   // led intensity command
            break;
          case 4:  // led test command
            break;
          case 5:  // enable ADC command - but we don't do this
            break;
          case 6: // shutdown command - not sure what the monome is expected to do here
            break;
          case 7:  // led row command
            break;
          case 8:  // coloum command
            break;
          default:
            break;

        } // end switch(command)
      } // end of else in if(WaitingCommand)
    } // end of if(Serial.available();
  } // end of do
  while (Serial.available() > 8);  // keep on going if we have a lot of commands stacked up
}

void read_buttons()
{
  for (uint16_t i = 0; i < NUM_BTN_COLUMNS; i++)
  {
    digitalWrite(latchPin, LOW);
    shiftOut(dataPin, clockPin, MSBFIRST, (bits[i]) >> 8);
    shiftOut(dataPin, clockPin, MSBFIRST, bits[i]);
    digitalWrite(latchPin, HIGH);

    for (uint16_t j = 0; j < NUM_BTN_ROWS; j++)
    {
      byte val = digitalRead(btnreadpins[j]);
      if (prev_buttons[i][j] != val)
      {
        // low is Pressed
        if (val == LOW )
        {
#if defined(ARDUINOME)
          Serial.write((uint8_t)((0 << 4) | (0 & 15)));
          Serial.write((uint8_t)((j << 4) | (i & 15)));
#else
          noteOff(MIDI_CHANNEL, note[j][i]);
#endif
        }
        // hight is Down
        else
        {
#if defined(ARDUINOME)
          Serial.write((0 << 4) | (1 & 15));
          Serial.write((j << 4) | (i & 15));
#else
          noteOff(MIDI_CHANNEL, note[j][i]);
#endif

        }
        prev_buttons[i][j] = val;
      }
    }
    digitalWrite(latchPin, LOW);
    shiftOut(dataPin, clockPin, MSBFIRST, (0xffff) >> 8);
    shiftOut(dataPin, clockPin, MSBFIRST, 0xff);
    digitalWrite(latchPin, HIGH);
  }
}

void loop()
{
  if (pool_time  >= POOL_TIME)
  { // 20ms = min Trellis poll
    read_buttons();
    pool_time = 0;
  }
}

// Send a MIDI note on message
void noteOn(byte channel, byte pitch, byte velocity)
{
  // 0x90 is the first of 16 note on channels. Subtract one to go from MIDI's 1-16 channels to 0-15
  channel += 0x90 - 1;

  // Ensure we're between channels 1 and 16 for a note on message
  if (channel >= 0x90 && channel <= 0x9F)
  {
#ifdef DEBUG
    Serial.print("Button pressed: ");
    Serial.println(pitch);
#elif defined(TEENSY_PLUS_PLUS) || defined(TEENSY_2) || defined(TEENSY_PLUS_PLUS_2)
    usbMIDI.sendNoteOn(pitch, velocity, channel);
#else
    Serial.write(channel);
    Serial.write(pitch);
    Serial.write(velocity);
#endif
  }
}

// Send a MIDI note off message
void noteOff(byte channel, byte pitch)
{
  // 0x80 is the first of 16 note off channels. Subtract one to go from MIDI's 1-16 channels to 0-15
  channel += 0x80 - 1;

  // Ensure we're between channels 1 and 16 for a note off message
  if (channel >= 0x80 && channel <= 0x8F)
  {
#ifdef DEBUG
    Serial.print("Button released: ");
    Serial.println(pitch);
#elif defined(TEENSY_PLUS_PLUS) || defined(TEENSY_2) || defined(TEENSY_PLUS_PLUS_2)
    usbMIDI.sendNoteOff(pitch, 0x00, channel);
#elif MIDI_CONTROLLER
    Serial.write(channel);
    Serial.write(pitch);
    Serial.write((byte)0x00);
#endif
  }
}

// Send a MIDI control change message
void controlChange(byte channel, byte control, byte value)
{
  // 0xB0 is the first of 16 control change channels. Subtract one to go from MIDI's 1-16 channels to 0-15
  channel += 0xB0 - 1;

  // Ensure we're between channels 1 and 16 for a CC message
  if (channel >= 0xB0 && channel <= 0xBF)
  {
#ifdef DEBUG
    Serial.print(control - MIDI_CC);
    Serial.print(": ");
    Serial.println(value);
#elif defined(TEENSY_PLUS_PLUS) || defined(TEENSY_2) || defined(TEENSY_PLUS_PLUS_2)
    usbMIDI.sendControlChange(control, value, channel);
#else
    Serial.write(channel);
    Serial.write(control);
    Serial.write(value);
#endif
  }
}
