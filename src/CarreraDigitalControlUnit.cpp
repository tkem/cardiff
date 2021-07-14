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

#ifdef ARDUINO_ARCH_MBED
#include <pinDefinitions.h>
#endif

#ifdef ARDUINO_ARCH_ESP8266
#define CU_IRQ_HANDLER IRAM_ATTR
#else
#define CU_IRQ_HANDLER
#endif

#if defined(__MBED__)

#include "platform/mbed_critical.h"

static inline void critical_section_enter()
{
    core_util_critical_section_enter();
}

static inline void critical_section_exit()
{
    core_util_critical_section_exit();
}

static inline bool compare_and_swap(volatile uint16_t* ptr, uint16_t expected, uint16_t desired)
{
    return core_util_atomic_cas_u16(ptr, &expected, desired);
}

#elif defined(ARDUINO)

static unsigned critical_section_depth = 0;

static inline void critical_section_enter()
{
    noInterrupts();
    ++critical_section_depth;
}

static inline void critical_section_exit()
{
    if (--critical_section_depth == 0) {
        interrupts();
    }
}

static bool compare_and_swap(volatile uint16_t* ptr, uint16_t expected, uint16_t desired)
{
    bool result = false;
    critical_section_enter();
    if (*ptr == expected) {
        *ptr = desired;
        result = true;
    }
    critical_section_exit();
    return result;
}

#else

#error "This library works only on mbed and Arduino platforms."

#endif

static const uint16_t emit_mask[10] = {
    1 << 12,
    1 << 9,
    1 << 8,  // or 7 if no command was sent to CU
    1 << 9,
    1 << 9,
    1 << 9,
    1 << 9,
    1 << 9,
    1 << 7,
    1 << 9
};

static uint8_t rev8(uint8_t data)
{
    data = ((data & 0x55) << 1) | ((data >> 1) & 0x55);
    data = ((data & 0x33) << 2) | ((data >> 2) & 0x33);
    data = ((data & 0xf0) >> 4) | ((data & 0x0f) << 4);
    return data;
}

static unsigned rev12(unsigned data)
{
    data = ((data & 0x555) << 1) | ((data >> 1) & 0x555);
    data = ((data & 0x333) << 2) | ((data >> 2) & 0x333);
    data = ((data & 0xf00) >> 8) | (data & 0x0f0) | ((data & 0x00f) << 8);
    return data;
}

#ifdef ARDUINO

static CarreraDigitalControlUnit* instance = 0;

CarreraDigitalControlUnit::CarreraDigitalControlUnit(int pin, bool inverted)
#ifdef ARDUINO_ARCH_MBED
    : _irq(digitalPinToPinName(pin)), _data(0), _inverted(inverted), _running(false)
#else
    : _pin(pin), _data(0), _inverted(inverted), _running(false)
#endif
{
#ifdef __AVR
    _ireg = portInputRegister(digitalPinToPort(pin));
    _mask = digitalPinToBitMask(pin);
#endif
    instance = this;
}

CarreraDigitalControlUnit::CarreraDigitalControlUnit(int pin, int mode, bool inverted)
#ifdef ARDUINO_ARCH_MBED
    : _irq(digitalPinToPinName(pin)), _data(0), _inverted(inverted), _running(false)
#else
    : _pin(pin), _data(0), _inverted(inverted), _running(false)
#endif
{
#ifdef __AVR
    _ireg = portInputRegister(digitalPinToPort(pin));
    _mask = digitalPinToBitMask(pin);
#endif
#ifdef ARDUINO_ARCH_MBED
    switch (mode) {
    case INPUT:
        _irq.mode(PullNone);
        break;
    case INPUT_PULLUP:
        _irq.mode(PullUp);
        break;
    case INPUT_PULLDOWN:
    default:
        _irq.mode(PullDown);
        break;
    }
#else
    pinMode(pin, mode);
#endif
    instance = this;
}

#endif

#ifdef __MBED__
CarreraDigitalControlUnit::CarreraDigitalControlUnit(PinName pin, bool inverted)
    : _irq(pin), _data(0), _inverted(inverted), _running(false)
{
}

CarreraDigitalControlUnit::CarreraDigitalControlUnit(PinName pin, PinMode mode, bool inverted)
    : _irq(pin, mode), _data(0), _inverted(inverted), _running(false)
{
}
#endif

void CarreraDigitalControlUnit::start()
{
    critical_section_enter();
    if (!_running) {
        reset();
#ifdef __MBED__
        if (_inverted) {
            _irq.rise(mbed::callback(this, &CarreraDigitalControlUnit::fall));
            _irq.fall(mbed::callback(this, &CarreraDigitalControlUnit::rise));
        } else {
            _irq.rise(mbed::callback(this, &CarreraDigitalControlUnit::rise));
            _irq.fall(mbed::callback(this, &CarreraDigitalControlUnit::fall));
        }
        _timer.start();
#else
        attachInterrupt(digitalPinToInterrupt(_pin), &irq, CHANGE);
#endif
        _running = true;
    }
    critical_section_exit();
}

void CarreraDigitalControlUnit::stop()
{
    critical_section_enter();
    if (_running) {
#ifdef __MBED__
        _timer.stop();
        _irq.rise(0);
        _irq.fall(0);
#else
        detachInterrupt(digitalPinToInterrupt(_pin));
#endif
        _running = false;
    }
    critical_section_exit();
}

void CarreraDigitalControlUnit::reset()
{
    critical_section_enter();
    _time = 0;
    _buffer = 0;
    _index = 0;
    _data = 0;
    critical_section_exit();
}

int CarreraDigitalControlUnit::read()
{
    uint16_t data;
    do {
        data = _data;
    } while (!data || !compare_and_swap(&_data, data, 0));
    return data;
}

int CarreraDigitalControlUnit::read(uint32_t timeout_us)
{
    // read(0) should return data if available
    uint32_t start = time_us();
    uint16_t data = _data;
    while (!data || !compare_and_swap(&_data, data, 0)) {
        if ((time_us() - start) > timeout_us) {
            return -1;
        }
        data = _data;
    }
    return data;
}

void CU_IRQ_HANDLER CarreraDigitalControlUnit::emit()
{
    _data = _buffer;
    _buffer = 0;
    if (++_index == 10) {
        _index = 0;
    }
}

void CU_IRQ_HANDLER CarreraDigitalControlUnit::fall()
{
    // we expect 7.5ms between data packets, so unsigned int should be
    // wide enough on any platform and a little more efficient on AVR...
    unsigned t = time_us();
    unsigned d = t - _time;
    if (_buffer && d >= 80 && d < 128) {
        _buffer <<= 1;
        _buffer |= 1;
        if (_buffer & emit_mask[_index]) {
            emit();
        }
        _time = t;
    } else {
        if (_index == 2 && _buffer && d >= 128) {
            emit();  // probably ACT and not ACK
        }
        if (d > 6000) {
            if (_buffer) {
                _index = 0;  // lost sync
            }
            _buffer = 1;
            _time = t;
        }
    }
}

void CU_IRQ_HANDLER CarreraDigitalControlUnit::rise()
{
    unsigned t = time_us();
    unsigned d = t - _time;
    if (d >= 80 && d < 128) {
        _buffer <<= 1;
        if (_buffer & emit_mask[_index]) {
            emit();
        }
        _time = t;
    }
}

uint32_t CarreraDigitalControlUnit::time_us()
{
#ifdef __MBED__
    return _timer.elapsed_time().count();
#else
    return micros();
#endif
}

#ifndef __MBED__
void CU_IRQ_HANDLER CarreraDigitalControlUnit::irq()
{
#ifdef __AVR
    uint8_t value = *instance->_ireg & instance->_mask;
#else
    int value = digitalRead(instance->_pin);
#endif
    if (instance->_inverted ? !value : value) {
        instance->rise();
    } else {
        instance->fall();
    }
}
#endif

bool CarreraDigitalControlUnit::parse_prog(int data, uint8_t res[3])
{
    if ((data & ~0xfff) == 0x1000) {
        // programming word is LSB
        data = rev12(data);
        res[0] = (data >> 4) & 0x1f;
        res[1] = data & 0x0f;
        res[2] = (data >> 9) & 0x07;
        return true;
    } else {
        return false;
    }
}

bool CarreraDigitalControlUnit::parse_ctrl(int data, uint8_t res[4])
{
    if ((data & ~0x1ff) == 0x200 && (data >> 6) != 0xf) {
        // controller word is MSB
        res[0] = (data >> 6) & 0x7;
        res[1] = (data >> 1) & 0xf;
        res[2] = !(data & 0x20);
        res[3] = data & 0x1;
        return true;
    } else {
        return false;
    }
}

bool CarreraDigitalControlUnit::parse_pace(int data, uint8_t res[4])
{
    if ((data & ~0x3f) == 0x3c0) {
        res[0] = (data >> 5) & 0x1;
        res[1] = (data >> 2) & 0x1;
        res[2] = (data >> 1) & 0x1;
         res[3] = data & 0x1;
        return true;
    } else {
        return false;
    }
}

bool CarreraDigitalControlUnit::parse_act(int data, uint8_t res[2])
{
    if ((data & ~0x7f) == 0x80) {
        // active controller mask is LSB
        res[0] = rev8(data >> 1) >> 2;
        res[1] = data & 0x1;
        return true;
    } else {
        return false;
    }
}

bool CarreraDigitalControlUnit::parse_ack(int data, uint8_t res[1])
{
    if ((data & ~0xff) == 0x100) {
        // acknowledge mask is LSB
        res[0] = rev8(data);
        return true;
    } else {
        return false;
    }
}

CarreraControllerPacket::CarreraControllerPacket(int data)
    : _data((data & ~0x1ff) == 0x200 && (data & ~0x3f) != 0x3c0 ? data : 0)
{}

CarreraPaceCarPacket::CarreraPaceCarPacket(int data)
    : _data((data & ~0x3f) == 0x3c0 ? data : 0)
{}

CarreraActivityPacket::CarreraActivityPacket(int data)
    : _data((data & ~0x7f) == 0x80 ? data : 0)
{
    _data = 0x80 | rev8(_data & 0xff); // LSB
}

CarreraAcknowledgePacket::CarreraAcknowledgePacket(int data)
    : _data((data & ~0xff) == 0x100 ? data : 0)
{
    _data = 0x100 | rev8(_data & 0xff);  // LSB
}

CarreraProgrammingPacket::CarreraProgrammingPacket(int data)
    : _data((data & ~0xfff) == 0x1000 ? data : 0)
{
    _data = 0x1000 | rev12(_data);  // LSB
}
