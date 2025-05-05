#include "relay_module.h"
#include <Arduino.h>

const int relayPins[4] = {25, 27, 32, 26};
bool relayStates[4] = {false, false, false, false};
const int relayFeedbackLedPin = 33;

int testVariable = 5;

void setupRelays() {
    for (int i = 0; i < 4; i++) {
        pinMode(relayPins[i], OUTPUT);
        digitalWrite(relayPins[i], relayStates[i] ? HIGH : LOW);
    }
    pinMode(relayFeedbackLedPin, OUTPUT);
    digitalWrite(relayFeedbackLedPin, LOW);
}

void toggleRelay(int index) {
    if (index >= 0 && index < 4) {
        relayStates[index] = !relayStates[index];
        digitalWrite(relayPins[index], relayStates[index] ? HIGH : LOW);
        blinkRelayFeedback();
    }
}

void setRelay(int index, bool state) {
    if (index >= 0 && index < 4) {
        relayStates[index] = state;
        digitalWrite(relayPins[index], state ? HIGH : LOW);
        blinkRelayFeedback();
    }
}

void blinkRelayFeedback() {
    digitalWrite(relayFeedbackLedPin, HIGH);
    delay(100);
    digitalWrite(relayFeedbackLedPin, LOW);
}
