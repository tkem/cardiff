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
#ifndef CARRERA_DIGITAL_CONTROL_UNIT_H
#define CARRERA_DIGITAL_CONTROL_UNIT_H

#ifdef ARDUINO
#include <Arduino.h>
#endif

#ifdef __MBED__
#include <mbed.h>
#endif

/** A Carrera(R) Digital 124/132 Control Unit connection
 *
 * @note Synchronization level: Interrupt safe
 *
 * @ingroup drivers
 */
class CarreraDigitalControlUnit {
public:
#ifdef ARDUINO
    /** Create a connection to a ControlUnit using the specified pin
     *
     * @param pin A digital input connected to the Control Unit
     * @param inverted Whether the input is logically inverted
     *
     * @note pin must support interrupts
     */
    CarreraDigitalControlUnit(int pin, bool inverted = false);

    /** Create a connection to a ControlUnit using the specified pin
     *
     * @param pin A digital input connected to the Control Unit
     * @param mode The pin mode to set for the input
     * @param inverted Whether the input is logically inverted
     *
     * @note pin must support interrupts
     */
    CarreraDigitalControlUnit(int pin, int mode, bool inverted);
#endif

#ifdef __MBED__
    /** Create a connection to a ControlUnit using the specified pin
     *
     * @param pin A digital input connected to the Control Unit
     * @param inverted Whether the input is logically inverted
     *
     * @note pin must support interrupts
     */
    CarreraDigitalControlUnit(PinName pin, bool inverted = false);

    /** Create a connection to a ControlUnit using the specified pin
     *
     * @param pin A digital input connected to the Control Unit
     * @param mode The pin mode to set for the input
     * @param inverted Whether the input is logically inverted
     *
     * @note pin must support interrupts
     */
    CarreraDigitalControlUnit(PinName pin, PinMode mode, bool inverted);
#endif

    /** Start receiving data from the Control Unit
     */
    void start();

    /** Stop receiving data from the Control Unit
     */
    void stop();

    /** Reset the Control Unit connection, e.g. in case of error
     */
    void reset();

    /** Read a data word from the Control Unit
     */
    int read();

    /** Read a data word from the Control Unit with timeout
     *
     * @param timeout_us The timeout in microseconds
     *
     * @returns the next data word, or -1 on timeout
     */
    int read(uint32_t timeout_us);

    /** Split a programming data word into its components
     *
     * If successfull, res will contain the following
     *
     * - res[0]  The programming command (0..31)
     * - res[1]  The programming value (0..15)
     * - res[2]  The programming address/controller (0..7)
     *
     * @param data The data word to split
     *
     * @param res The programming data word's components
     *
     * @returns true on success, false on error
     */
    static bool parse_prog(int data, uint8_t res[3]);

    /** Split a controller data word into its components
     *
     * If successfull, res will contain the following
     *
     * - res[0]  The controller's address (0..5)
     * - res[1]  The controller's speed (0..15)
     * - res[2]  Whether the lange change button is pressed (0..1)
     * - res[3]  Whether fuel mode is enabled (0..1)
     *
     * @param data The data word to split
     *
     * @param res The controller data word's components
     *
     * @returns true on success, false on error
     */
    static bool parse_ctrl(int data, uint8_t res[4]);

    /** Split a pace/autonomous car data word into its components
     *
     * If successfull, res will contain the following
     *
     * - res[0]  Whether pace car and autonomous car are stopped (0..1)
     * - res[1]  Whether the pace car should return to the box (0..1)
     * - res[2]  Whether the pace car is active (0..1)
     * - res[3]  Whether fuel mode is enabled (0..1)
     *
     * @param data The data word to split
     *
     * @param res The pace/autonomous car data word's components
     *
     * @returns true on success, false on error
     */
    static bool parse_pace(int data, uint8_t res[4]);

    /** Split an active controller data word into its components
     *
     * If successfull, res will contain the following
     *
     * - res[0]  A bit mask representing active controllers (0..63)
     * - res[1]  Whether any controller was active (0..1)
     *
     * @param data The data word to split
     *
     * @param res The active controller data word's components
     *
     * @returns true on success, false on error
     */
    static bool parse_act(int data, uint8_t res[2]);

    /** Split an acknowledge data word into its components
     *
     * If successfull, res will contain the following
     *
     * - res[0]  A bit mask of time slots in which a message was received
     *
     * @param data The data word to split
     *
     * @param res The acknowledge data word's components
     *
     * @returns true on success, false on error
     */
    static bool parse_ack(int data, uint8_t res[1]);

private:
    void emit();
    void fall();
    void rise();
    uint32_t time_us();
#ifdef __MBED__
    mbed::InterruptIn _irq;
    mbed::Timer _timer;
#else
    static void irq();
    int _pin;
#ifdef __AVR
    volatile uint8_t* _ireg;
    uint8_t _mask;
#endif
#endif
    volatile uint16_t _data;
    bool _inverted;
    bool _running;
    unsigned _time;
    unsigned _buffer;
    unsigned _index;
};

class CarreraControllerPacket {
public:

    CarreraControllerPacket() : _data(0) {}

    CarreraControllerPacket(int data);

    uint8_t address() const { return (_data >> 6) & 0x07; }

    bool laneChange() const { return (_data & 0x20) == 0; }

    uint8_t throttle() const { return (_data >> 1) & 0x0f; }

    bool fuelMode() const { return (_data & 0x01) != 0; }

    operator bool() const { return _data != 0; }

private:
    uint16_t _data;
};

class CarreraPaceCarPacket {
public:
    CarreraPaceCarPacket() : _data(0) {}

    CarreraPaceCarPacket(int data);

    bool stopped() const { return (_data & 0x20) != 0; }

    bool returnToPit() const { return (_data & 0x04) != 0; }

    bool active() const { return (_data & 0x02) != 0; }

    bool fuelMode() const { return (_data & 0x01) != 0; }

    operator bool() const { return _data != 0; }

private:
    uint16_t _data;
};

class CarreraActivityPacket {
public:
    CarreraActivityPacket() : _data(0) {}

    CarreraActivityPacket(int data);

    uint8_t controllerMask() const { return (_data >> 1) & 0x3f; }

    bool anyControllerActive() const { return (_data & 0x80) != 0; }

    operator bool() const { return _data != 0; }

private:
    uint16_t _data;
};

class CarreraAcknowledgePacket {
public:
    CarreraAcknowledgePacket() : _data(0) {}

    CarreraAcknowledgePacket(int data);

    uint8_t slotMask() const { return _data & 0xff; }

    operator bool() const { return _data != 0; }

private:
    uint16_t _data;
};

class CarreraProgrammingPacket {
public:
    CarreraProgrammingPacket() : _data(0) {}

    CarreraProgrammingPacket(int data);

    uint8_t value() const { return _data & 0x0f; }

    uint8_t command() const { return (_data >> 4) & 0x1f; }

    uint8_t address() const { return (_data >> 9) & 0x07; }

    operator bool() const { return _data != 0; }

private:
    uint16_t _data;
};

#endif
