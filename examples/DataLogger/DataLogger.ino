/*
   Copyright 2017 Thomas Kemmer

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

// printf() is fine for debugging, but we need something more
// efficient here...
class DataLogger {
    RawSerial _serial;

public:
    template<class TX, class RX>
    DataLogger(TX tx, RX rx, long baud = 115200) : _serial(tx, rx, baud) {}

    DataLogger& operator<<(const char* s) {
        _serial.puts(s);
        return *this;
    }

    DataLogger& operator<<(bool f) {
        _serial.putc(f ? '1' : '0');
        return *this;
    }

    DataLogger& operator<<(uint8_t v) {
        if (v >= 100) {
            _serial.putc('0' + v / 100);
            v %= 100;
        }
        if (v >= 10) {
            _serial.putc('0' + v / 10);
            v %= 10;
        }
        _serial.putc('0' + v);
        return *this;
    }

    DataLogger& operator<<(uint16_t v) {
        static const char hex[] = "0123456789abcdef";
        _serial.putc(hex[v >> 12]);
        _serial.putc(hex[(v >> 8) & 0xf]);
        _serial.putc(hex[(v >> 4) & 0xf]);
        _serial.putc(hex[v & 0xf]);
        return *this;
    }
};

// TODO: more efficient implementation?
inline uint8_t lsb3(uint8_t v) {
    return ((v & 0x1) << 2) | (v & 0x2) | ((v & 0x4) >> 2);
}

inline uint8_t lsb4(uint8_t v) {
    return ((v & 0x1) << 3) | (v & 0x2) << 1 | (v & 0x4) >> 1 | ((v & 0x8) >> 3);
}

inline uint8_t lsb5(uint8_t v) {
    return ((v & 0x1) << 4) | (v & 0x2) << 2 | (v & 0x4) | (v & 0x8) >> 2 | ((v & 0x10) >> 4);
}

inline uint8_t msb3(uint8_t v) {
    return v & 0x7;
}

inline uint8_t msb4(uint8_t v) {
    return v & 0xf;
}

inline uint8_t msb5(uint8_t v) {
    return v & 0xf;
}

DataLogger out(USBTX, USBRX);

CarreraDigitalControlUnit cu(D2);

void setup()
{
    // TODO: start
}

void loop()
{
    uint16_t data = cu.read();
    if (data & ~0x1fff) {
        out << "???:  " << data << "\r\n";
    } else if (data & 0x1000) {
        out << "PROG: " << data
            << " B=" << lsb5(data >> 3)
            << " W=" << lsb4(data >> 8)
            << " R=" << lsb3(data)
            << "\r\n";
    } else if (data & 0xc00) {
        out << "???:  " << data << "\r\n";
    } else if ((data & 0x3c0) == 0x3c0) {
        out << "PACE: " << data
            << " KFR=" << !!(data & 0x20)
            << " TK=" << !!(data & 0x10)
            << " FR=" << !!(data & 0x8)
            << " NH=" << !!(data & 0x4)
            << " PC=" << !!(data & 0x2)
            << " TA=" << !!(data & 0x1)
            << "\r\n";
    } else if (data & 0x200) {
        out << "CTRL: " << data
            << " R=" << msb3(data >> 6)
            << " SW=" << !!(data & 0x20)
            << " G=" << msb4(data >> 1)
            << " TA=" << !!(data & 0x1)
            << "\r\n";
    } else if (data & 0x100) {
        out << "ACK:  " << data << "\r\n";
    } else if (data & 0x80) {
        out << "ACT:  " << data
            << " R=" << uint16_t((data >> 1) & 0x3f)
            << " IE=" << !!(data & 0x1)
            << "\r\n";
    } else {
        out << "???:  " << data << "\r\n";
    }
}

#ifndef ARDUINO
int main() {
    setup();
    for (;;) {
        loop();
    }
}
#endif
