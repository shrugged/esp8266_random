#define NOTE_D5  587

long noteDuration = 500;
long frequency = NOTE_D5;

void playNote(int targetPin, long frequency, long length) {
  long delay = 1000000 / frequency / 2;
  long cycles = frequency * length / 1000;
  for (long i = 0; i < cycles; i++) {
    digitalWrite(targetPin, HIGH);
    delayMicroseconds(delay);
    digitalWrite(targetPin, LOW);
    delayMicroseconds(delay);
  }
}
