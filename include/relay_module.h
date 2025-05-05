#ifndef RELAY_MODULE_H
#define RELAY_MODULE_H
#include <Arduino.h>

void setupRelays();
void toggleRelay(int index);
void setRelay(int index, bool state);
extern const int relayPins[4];
extern bool relayStates[4];
void blinkRelayFeedback();

#endif // RELAY_MODULE_H
