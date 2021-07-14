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

#if !defined(ARDUINO)
PinName cuPin = D2;
#elif defined(ARDUINO_ARCH_MBED)
PinName cuPin = p2;
REDIRECT_STDOUT_TO(Serial);
#elif defined(ARDUINO_ARCH_ESP8266)
int cuPin = D2;
#define fputc(c, file) Serial.write(char(c))
#define fputs(s, file) Serial.print(s)
#else
int cuPin = 2;
#define fputc(c, file) Serial.write(char(c))
#define fputs(s, file) Serial.print(s)
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
    int data = cu.read(100000);  // 100ms timeout
    if (data < 0) {
        out << "Timeout\r\n";
        cu.reset();
    } else if (CarreraProgrammingPacket prog = data) {
        out << uint16_t(data) << " PROG:"
            << " COMMAND=" << prog.command()
            << " VALUE=" << prog.value()
            << " ADDRESS=" << prog.address()
            << "\r\n";
    } else if (CarreraControllerPacket ctrl = data) {
        out << uint16_t(data) << " CTRL:"
            << " ADDRESS=" << ctrl.address()
            << " THROTTLE=" << ctrl.throttle()
            << " LANECHANGE=" << ctrl.laneChange()
            << " FUEL=" << ctrl.fuelMode()
            << "\r\n";
    } else if (CarreraPaceCarPacket pace = data) {
        out << uint16_t(data) << " PACE:"
            << " STOPPED=" << pace.stopped()
            << " RETURN=" << pace.returnToPit()
            << " ACTIVE=" << pace.active()
            << " FUEL=" << pace.fuelMode()
            << "\r\n";
    } else if (CarreraActivityPacket act = data) {
        out << uint16_t(data) << " ACT:"
            << " MASK=" << act.controllerMask()
            << " ANY=" << act.anyControllerActive()
            << "\r\n";
    } else if (CarreraAcknowledgePacket ack = data) {
        out << uint16_t(data) << " ACK:"
            << " MASK=" << ack.slotMask()
            << "\r\n";
    } else {
        out << uint16_t(data) << " [???]\r\n";
        cu.reset();  // probably lost sync
    }
}
