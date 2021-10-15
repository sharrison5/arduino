// Control for NeoPixels in secret room

// Circuit contains:
//  + On/off switch
//  + Eight-stop rotary switch, which can be read on an analogue pin
//  + Five 12-LED NeoPixel rings

// ----------------------------------------------------------------------------

// SPDX-License-Identifier: Apache-2.0

// Copyright 2021 Sam Harrison

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

// http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// ----------------------------------------------------------------------------

#include <inttypes.h>

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

// ----------------------------------------------------------------------------
// Config

namespace neopixels {
    constexpr uint8_t pin = 2;

    constexpr uint8_t n_rings = 5;
    constexpr uint8_t n_leds_per_ring = 12;
    constexpr uint8_t n_leds = n_leds_per_ring * n_rings;
    constexpr uint8_t brightness = 200;  // Max: 255

    // Declare the NeoPixel strip object
    // Note that this set of NeoPixels have a non-standard colour wiring
    // See e.g. the Adafruit NeoPixel Überguide
    // https://learn.adafruit.com/adafruit-neopixel-uberguide/arduino-library-use
    Adafruit_NeoPixel strip(n_leds, pin, NEO_GRBW + NEO_KHZ800);
}

namespace mode_switch {
    constexpr uint8_t pin = A0;
    constexpr uint8_t n_stops = 8;
}

enum class modes: uint8_t {
    BRIGHT_WHITE,
    DIM_WHITE,
    PASTEL_RAINBOW,
    ROLLING_RAINBOW,
    ROLLING_RAINBOW_RING,
    CHASING_DOTS,
    PULSING_COLOURS,
    RANDOM,
    // -----
    ERROR
};
// Initialise status: this will be overwritten immediately
modes current_mode = modes::ERROR;

// ----------------------------------------------------------------------------
// Function declarations

namespace mode_switch {
    // Reads and transforms switch voltage to a lighting mode
    modes read_mode();
}

namespace neopixels {
    void constant_colour(const uint32_t colour);
}

// ----------------------------------------------------------------------------

// Runs once at startup
void setup() {
    // Initialise pixels
    neopixels::strip.begin();  // Initialise NeoPixel strip object (REQUIRED)
    neopixels::strip.setBrightness(neopixels::brightness);
    neopixels::strip.show();  // Turn OFF all pixels ASAP

    // Open serial port (speed in bits per second)
    //Serial.begin(9600);
    // https://www.arduino.cc/reference/en/language/functions/communication/serial/ifserial
    //while (!Serial) {
    //    ; // wait for serial port to connect. Needed for native USB
    //}
}

// Runs repeatedly after `setup()` has finished
void loop() {

    // Jump into subfunction to control NeoPixels: these functions are all
    // designed to remain running until the switch changes
    current_mode = mode_switch::read_mode();
    switch (current_mode) {
        // --------------------------------------------------------------------
        case modes::BRIGHT_WHITE : {
            const uint32_t colour = neopixels::strip.gamma32(
                neopixels::strip.Color(0, 0, 0, 255)
            );
            neopixels::constant_colour(colour);
            break;
        }
        // --------------------------------------------------------------------
        case modes::DIM_WHITE : {
            const uint32_t colour = neopixels::strip.gamma32(
                neopixels::strip.Color(0, 0, 0, 100)
            );
            neopixels::constant_colour(colour);
            break;
        }
        // --------------------------------------------------------------------
        // Error reading pin, or...
        case modes::ERROR :
        // Invalid state. We *really* shouldn't end up in a mode outside the
        // enumerated set.
        // In both cases use lights to make it obvious! Then retry
        default : {
            // Nice bright red
            const uint32_t colour = neopixels::strip.gamma32(
                neopixels::strip.Color(255, 0, 0, 50)
            );
            // Fill directly rather than entering a subfunction
            // We don't want to wait for a switch to change if we don't trust
            // what we are reading or how we got here
            neopixels::strip.fill(colour);
            neopixels::strip.show();
            // And pause to see if that helps
            delay(250);
            break;
        }
        // --------------------------------------------------------------------
    }

    // Brief pause is fine -- may well be changing several stops at once
    delay(50);

    //Serial.println(analogRead(mode_switch::pin));
    //delay(250);
}

// ----------------------------------------------------------------------------

// Reads and transforms switch voltage to a lighting mode
modes mode_switch::read_mode() {
    // 10-bit ADC on ATmega based boards (UNO, Nano, etc.)
    // This gives a voltage between 0 and 1023 as read by the Arduino
    const int voltage = analogRead(pin);

    // Check nothing weird going on
    if ((voltage < 0) || (voltage > 1023)) {
        return modes::ERROR;
    }

    // See where in the resistive ladder we are
    // Have a 1k resistor between each of the `n_stops`, so each is at an
    // equally spaced voltage. Here, we just assign to the nearest stop, or
    // equivalently, split at the voltages halfway between each stop.

    // Might as well use the extra 6 bits of precision available
    const uint16_t value = (static_cast<uint16_t>(voltage) << 6);
    // Cache half the distance between stops
    const uint16_t delta = UINT16_MAX / (2 * (n_stops - 1));
    // And now test if we are in range of each step one by one
    for (uint8_t i = 0; i < (n_stops - 1); ++i) {
        // Calculate voltage halfway between this stop and the next
        const int threshold = (2 * i + 1) * delta;
        // If we are below this, we're done
        // This is safe as we test in ascending order (and error for voltage<0)
        if (value < threshold) {
            return static_cast<modes>(i);
        }
    }
    // And if we don't find a match we must be at the last stop
    // Note that we can't test this in the loop above due to overflow
    return static_cast<modes>(n_stops - 1);

    // Thought experiment: can we choose `n_stops` and a set of resistor values
    // such that the above is just bit shifting? E.g.
    // mode = static_cast<modes>(voltage << 6);
}

// ----------------------------------------------------------------------------

void neopixels::constant_colour(const uint32_t colour) {
    // Display the colour on all LEDs
    strip.fill(colour);
    strip.show();

    // And wait for input to change
    while (true) {
        const modes mode = mode_switch::read_mode();
        if (mode != current_mode) {
            break;
        }
        delay(100);
    }

    return;
}

// ----------------------------------------------------------------------------
