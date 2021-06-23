/*
   Copyright 2017, 2021 Thomas Kemmer

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
#include "CarreraDigitalControlUnit.h"

// use digital pin 2 as input - make sure it does not deliver more
// than 5V or 3.3V, depending on platform!
#if defined(ARDUINO_ARCH_MBED)
PinName cuPin = p2;
REDIRECT_STDOUT_TO(Serial);
#elif defined(ARDUINO_ARCH_ESP8266)
int cuPin = D2;
#define fputc(c, file) Serial.write(char(c))
#define fputs(s, file) Serial.print(s)
#elif defined(ARDUINO)
int cuPin = 2;
#define fputc(c, file) Serial.write(char(c))
#define fputs(s, file) Serial.print(s)
#elif defined(TARGET_NRF51_MICROBIT)
PinName cuPin = P2;
#else
PinName cuPin = D2;
#endif

CarreraDigitalControlUnit cu(cuPin);

// printf() is fine for debugging, but we need something more
// efficient here...
class DataLogger {
public:
    DataLogger() {}
    
    DataLogger& operator<<(const char* s) {
        fputs(s, stdout);
        return *this;
    }

    DataLogger& operator<<(bool f) {
        fputc(f ? '1' : '0', stdout);
        return *this;
    }

    DataLogger& operator<<(uint8_t v) {
        if (v >= 100) {
            fputc('0' + v / 100, stdout);
            v %= 100;
        }
        if (v >= 10) {
            fputc('0' + v / 10, stdout);
            v %= 10;
        }
        fputc('0' + v, stdout);
        return *this;
    }

    DataLogger& operator<<(uint16_t v) {
        static const char hex[] = "0123456789abcdef";
        fputc(hex[(v >> 12) & 0xf], stdout);
        fputc(hex[(v >> 8) & 0xf], stdout);
        fputc(hex[(v >> 4) & 0xf], stdout);
        fputc(hex[v & 0xf], stdout);
        fputc('h', stdout);  // hex
        return *this;
    }
};

DataLogger out;

void setup() {
#ifdef ARDUINO
    Serial.begin(115200);
#endif
    cu.start();
}

void loop() {
    uint8_t values[4];  // maximum number of components
    int data = cu.read(100000);  // 100ms timeout
    if (data < 0) {
        out << "Timeout\r\n";
        cu.reset();
    } else if (cu.parse_prog(data, values)) {
        // prog := { command, value, address }
        out << uint16_t(data) << " PROG:"
            << " COMMAND=" << values[0]
            << " VALUE=" << values[1]
            << " ADDRESS=" << values[2]
            << "\r\n";
    } else if (cu.parse_ctrl(data, values)) {
        // ctrl := { address, speed, button, fuel }
        out << uint16_t(data) << " CTRL:"
            << " ADDRESS=" << values[0]
            << " SPEED=" << values[1]
            << " BUTTON=" << values[2]
            << " FUEL=" << values[3]
            << "\r\n";
    } else if (cu.parse_pace(data, values)) {
        // pace := { stop, return, active, fuel }
        out << uint16_t(data) << " PACE:"
            << " STOP=" << values[0]
            << " RETURN=" << values[1]
            << " ACTIVE=" << values[2]
            << " FUEL=" << values[3]
            << "\r\n";
    } else if (cu.parse_act(data, values)) {
        // act := { mask, any }
        out << uint16_t(data) << " ACT:"
            << " MASK=" << values[0]
            << " ANY=" << values[1]
            << "\r\n";
    } else if (cu.parse_ack(data, values)) {
        // ack := { mask }
        out << uint16_t(data) << " ACK:"
            << " MASK=" << values[0]
            << "\r\n";
    } else {
        out << uint16_t(data) << " [???]\r\n";
        cu.reset();  // probably lost sync
    }
}
