// Control for NeoPixels in secret room

// Circuit contains:
//  + On/off switch
//  + Eight-stop rotary switch, which can be read on an analog pin
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

// Functions to control lights -- all loop until switch changes
namespace neopixels {
    // Simply display colour, checking mode switch intermittently
    void constant_colour(const uint32_t colour);

    // Loops through the colour wheel (i.e. HSV)
    // Possible to change speed via `hue_step_size` and create transitions
    // between adjacent LEDs via `phase_offset`.
    void rainbow(
        const uint16_t hue_step_size = 16,
        const uint16_t phase_offset = 0,
        const uint8_t white = 0,
        const uint8_t saturation = UINT8_MAX,
        const uint8_t value = UINT8_MAX
    );

    // Random colours (change `delay_ms` to set speed)
    void random(const uint16_t delay_ms);

    // Cycles through the colour wheel at fluctuating intensity
    // Picks `n_colours`, evenly spaced by hue. By default, the first colour is
    // centred on red (so any value of `n_colours` divisible by three will
    // include green and blue). This can be changed using `hue_offset` if
    // required. Similarly to other functions, the proportion of `white` and
    // speed (`delay_ms`) can be changed as necessary too.
    void pulsing_colours(
        const uint8_t n_colours,
        const uint16_t delay_ms,
        const uint8_t white = 0,
        const uint16_t hue_offset = 0
    );

    // Set of dots chasing around the LEDs
    // A batch of `n_dots` is released (at a gap of `separation`) which pass
    // along the LEDs. Once all have finished a new batch is released. Also
    // possible to change the length of the trail they leave behind them
    // (`n_fade`) and speed (`delay_ms`).
    void chasing_dots(
        const uint8_t n_dots,
        const uint8_t n_fade,
        const uint16_t separation,
        const uint16_t delay_ms
    );
}

// ----------------------------------------------------------------------------

// Runs once at startup
void setup() {
    // Initialise pixels
    neopixels::strip.begin();  // Initialise NeoPixel strip object (REQUIRED)
    neopixels::strip.setBrightness(neopixels::brightness);
    neopixels::strip.show();  // Turn OFF all pixels ASAP

    // Initialise random number generator
    // Use signal on unconnected pin as seed
    randomSeed(analogRead(A7));

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
                neopixels::strip.Color(0, 0, 0, UINT8_MAX)
            );
            neopixels::constant_colour(colour);
            break;
        }
        // --------------------------------------------------------------------
        case modes::DIM_WHITE : {
            const uint32_t colour = neopixels::strip.gamma32(
                neopixels::strip.Color(0, 0, 0, UINT8_MAX / 2)
            );
            neopixels::constant_colour(colour);
            break;
        }
        // --------------------------------------------------------------------
        // Nice slow fade with all colours in sync, as well as a large amount
        // of background white for pastel shades
        case modes::PASTEL_RAINBOW : {
            const uint16_t hue_step_size = 4;
            const uint16_t phase_offset = 0;
            const uint8_t white = 2 * (UINT8_MAX / 3);
            neopixels::rainbow(hue_step_size, phase_offset, white);
            break;
        }
        // --------------------------------------------------------------------
        // Spread the colour wheel across all LEDs, so seems to slowly move
        // around the room. Small amount of white to smooth intensity
        // differences as hue changes.
        case modes::ROLLING_RAINBOW : {
            const uint16_t hue_step_size = 64;
            const uint16_t phase_offset = UINT16_MAX / neopixels::n_leds;
            const uint8_t white = UINT8_MAX / 4;
            neopixels::rainbow(hue_step_size, phase_offset, white);
            break;
        }
        // --------------------------------------------------------------------
        // Full rainbow rolls around each ring simultaneously
        case modes::ROLLING_RAINBOW_RING : {
            const uint16_t hue_step_size = 256;
            const uint16_t phase_offset = UINT16_MAX / neopixels::n_leds_per_ring;
            const uint8_t white = 0;
            neopixels::rainbow(hue_step_size, phase_offset, white);
            break;
        }
        // --------------------------------------------------------------------
        case modes::PULSING_COLOURS : {
            const uint8_t n_colours = 6;
            const uint16_t delay_ms = 10;
            neopixels::pulsing_colours(n_colours, delay_ms);
            break;
        }
        // --------------------------------------------------------------------
        case modes::CHASING_DOTS : {
            const uint8_t n_dots = 5;
            const uint8_t n_fade = 8;
            const uint16_t separation = 40;
            const uint16_t delay_ms = 50;
            neopixels::chasing_dots(n_dots, n_fade, separation, delay_ms);
            break;
        }
        // --------------------------------------------------------------------
        case modes::RANDOM : {
            const uint16_t delay_ms = 25;
            neopixels::random(delay_ms);
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
                neopixels::strip.Color(UINT8_MAX, 0, 0, 0)
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
    for (uint8_t n = 0; n < (n_stops - 1); ++n) {
        // Calculate voltage halfway between this stop and the next
        const uint16_t threshold = (2 * n + 1) * delta;
        // If we are below this, we're done
        // This is safe as we test in ascending order (and error for voltage<0)
        if (value < threshold) {
            return static_cast<modes>(n);
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
            return;
        }
        delay(100);
    }

    return;
}

// ----------------------------------------------------------------------------

// Loop through colour wheel (i.e. HSV)
// Note the code below makes extensive use of unsigned integer rollover!
void neopixels::rainbow(
    const uint16_t hue_step_size,
    const uint16_t phase_offset,
    const uint8_t white,
    const uint8_t saturation,
    const uint8_t value
) {
    // This is the reference that steadily increases
    // LEDs may be offset relative to this to make apparent motion
    uint16_t base_hue = 0;

    // Loop indefinitely until switch changes
    while (true) {
        // Fill each LED with its colour
        for (uint8_t n = 0; n < n_leds; ++n) {
            // Combine base hue with LED offsets and any baseline white
            // Note that offsets going backwards in hue make it look like the
            // effect is moving forwards in space (think of the rollers on a
            // treadmill).
            const uint16_t hue = base_hue - n * phase_offset;
            const uint32_t colour = strip.gamma32(
                strip.ColorHSV(hue, saturation, value)
                | (static_cast<uint32_t>(white) << 24)  // See definition of `strip.Color()`
            );
            strip.setPixelColor(n, colour);
        }
        // And go!
        strip.show();

        // Now take a small step along the hue axis for next time
        // While we could have passed in the step size as a signed integer to
        // allow reverse cycles, this is potentially dangerous to be trying to
        // do signed arithmetic when we are near UINT_MAX.
        base_hue += hue_step_size;
        // While we could have done this as an explicit loop through hues,
        // it doesn't give us any more control
        //for (uint16_t base_hue = 0; base_hue <= UINT16_MAX; ++base_hue) {
        // Similarly, this runs slow enough that extra delays are unnecessary
        //delay(100);

        // Finally, check the switch and return control if mode has changed
        const modes mode = mode_switch::read_mode();
        if (mode != current_mode) {
            return;
        }
    }

    return;
}

// ----------------------------------------------------------------------------

// Random colours (change `delay_ms` to set speed)
void neopixels::random(const uint16_t delay_ms) {
    // Loop indefinitely until switch changes
    while (true) {
        // Fill each LED with a random colour
        for (uint8_t n = 0; n < n_leds; ++n) {
            // Squaring the random variables gives a sparser distribution,
            // which results in brighter colours (i.e. less whitish mixes)
            const uint32_t colour = neopixels::strip.gamma32(
                neopixels::strip.Color(
                    //sq(sq(::random(UINT8_MAX)) / UINT8_MAX) / UINT8_MAX,
                    sq(::random(UINT8_MAX + 1)) / UINT8_MAX,
                    sq(::random(UINT8_MAX + 1)) / UINT8_MAX,
                    sq(::random(UINT8_MAX + 1)) / UINT8_MAX,
                    sq(::random(UINT8_MAX + 1)) / (2 * UINT8_MAX)
                )
            );

            // Alternative in HSV space
            // Personal preference is for the RGB version
            //const uint16_t hue = ::random(UINT16_MAX);
            //const uint8_t saturation = UINT8_MAX;  // White desaturates
            //const uint8_t value = sq(::random(UINT8_MAX + 1)) / UINT8_MAX;
            //const uint8_t white = sq(::random(UINT8_MAX + 1)) / (2 * UINT8_MAX);
            //const uint32_t colour = strip.gamma32(
            //    strip.ColorHSV(hue, saturation, value)
            //    | (static_cast<uint32_t>(white) << 24)  // See definition of `strip.Color()`
            //);

            strip.setPixelColor(n, colour);
        }
        // And go!
        strip.show();

        // Finally, check the switch and return control if mode has changed
        const modes mode = mode_switch::read_mode();
        if (mode != current_mode) {
            return;
        }

        // Pause to slow down effect
        delay(delay_ms);
    }

    return;
}

// ----------------------------------------------------------------------------

// Cycles through the colour wheel at fluctuating intensity
void neopixels::pulsing_colours(
    const uint8_t n_colours,
    const uint16_t delay_ms,
    const uint8_t white = 0,
    const uint16_t hue_offset = 0
) {
    // Loop indefinitely until switch changes
    while (true) {

        // Pick evenly spaced hues
        for (uint8_t hue_index = 0; hue_index < n_colours; ++hue_index) {
            const uint16_t hue =
                hue_index * (UINT16_MAX / n_colours) + hue_offset;

            // Loop through a full cycle of a sine wave in intensity
            // This goes from 0 to 255 (i.e. rather than -1 to 1) in 256 steps
            // Minimum is at 270°, so 3/4 of a cycle
            const uint8_t offset = 3 * (UINT8_MAX / 4);
            for (uint8_t phase = 25; phase <= (UINT8_MAX - 25); ++phase) {
                // Transform hue & intensity (i.e. value) to a single colour
                const uint8_t saturation = UINT8_MAX;
                const uint8_t value = max(
                    strip.sine8(phase + offset), UINT8_MAX / 16
                );
                const uint32_t colour = neopixels::strip.gamma32(
                    strip.ColorHSV(hue, saturation, value)
                    | (static_cast<uint32_t>(white) << 24)  // See definition of `strip.Color()`
                );
                // Display the colour on all LEDs
                strip.fill(colour);
                strip.show();

                // Finally, check the switch and return control if mode has changed
                const modes mode = mode_switch::read_mode();
                if (mode != current_mode) {
                    return;
                }

                // Pause to slow down effect
                delay(delay_ms);
            }
        }
    }

    return;
}

// ----------------------------------------------------------------------------

// Set of dots chasing around the LEDs
void neopixels::chasing_dots(
    const uint8_t n_dots,
    const uint8_t n_fade,
    const uint16_t separation,
    const uint16_t delay_ms
) {
    // How long does it take for all dots to travel along the strip?
    // Extra timepoints give pause between repeats
    const uint16_t n_timepoints =
        n_leds + ((n_dots - 1) * separation) + n_fade + 3;

    // Cache how much fading to apply
    // `n_fade = 2` gives: | 1 | 2/3 | 1/3 | 0 | ...
    uint8_t intensities[n_fade];
    for (uint8_t n_f = 0; n_f < n_fade; n_f++) {
        intensities[n_f] = (n_fade - n_f) * (UINT8_MAX / (n_fade + 1));
    }

    // Loop indefinitely until switch changes
    while (true) {
        // Loop through time
        for (uint16_t t = 0; t < n_timepoints; ++t) {
            // Start with everything off and add back in as needed
            strip.clear();

            // Place dots
            for (uint8_t n = 0; n < n_dots; ++n) {
                const uint16_t offset = n * separation;
                // Check if dot is present, but splitting comparisons to avoid
                // over/underflow affecting checks
                // If time hasn't advanced far enough for this dot to appear,
                // we're done rendering as none of the others are ready either
                if (t < offset) {break;}
                // Similarly, if this dot has finished we can move on to the
                // next one
                if (t > (n_leds + offset + n_fade)) {continue;}

                // Place the dot itself
                // Again, need to make sure it is visible (may only be tail left)
                if (t <= (offset + n_leds)) {
                    const uint32_t colour = neopixels::strip.gamma32(
                        neopixels::strip.Color(0, 0, 0, UINT8_MAX)
                    );
                    strip.setPixelColor(t - offset, colour);
                }

                // Add fade strip
                for (uint8_t n_f = 0; n_f < n_fade; n_f++) {
                    if (t <= (offset + n_f + 1 + n_leds)) {
                        const uint32_t colour = neopixels::strip.gamma32(
                            neopixels::strip.Color(0, 0, 0, intensities[n_f])
                        );
                        strip.setPixelColor(t - offset - n_f - 1, colour);
                    }
                }
            }
            // And go!
            strip.show();

            // Finally, check the switch and return control if mode has changed
            const modes mode = mode_switch::read_mode();
            if (mode != current_mode) {
                return;
            }

            // Pause to slow down effect
            delay(delay_ms);
        }
    }

    return;

}
// ----------------------------------------------------------------------------
