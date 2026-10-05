
/*
 * Bullitt WS2815 LED controller
 *
 * ESP8266 + 3 independent WS2815 data channels
 * + WLAN Access Point / Webserver
 *
 * Hardware:
 *   GPIO 5  - left
 *   GPIO 4  - right
 *   GPIO 14 - crank
 *   GPIO 12 - reed
 *   GPIO 13 - effect button
 */

#include <FastLED.h>
#include "Controller.h"
#include "WebServer.h"

// ============================================================================
// Hardware
// ============================================================================

#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB

constexpr uint8_t PIN_LEFT   = 5;    // D1
constexpr uint8_t PIN_RIGHT  = 4;    // D2
constexpr uint8_t PIN_CRANK  = 14;   // D5
constexpr uint8_t PIN_REED   = 12;   // D6
constexpr uint8_t PIN_BUTTON = 13;   // D7

constexpr uint16_t NUM_LEFT  = 60;
constexpr uint16_t NUM_RIGHT = 60;
constexpr uint16_t NUM_CRANK = 27;

CRGB ledsLeft[NUM_LEFT];
CRGB ledsRight[NUM_RIGHT];
CRGB ledsCrank[NUM_CRANK];

// ============================================================================
// Physical / virtual geometry
// ============================================================================

struct Segment
{
    float startMeter;
    float lengthMeter;
    uint16_t ledCount;
    bool reversed;
};

Segment segmentLeft  = { 0.00f, 1.00f, NUM_LEFT,  true };
Segment segmentRight = { 0.00f, 1.00f, NUM_RIGHT, true };
Segment segmentCrank = { 1.00f, 0.45f, NUM_CRANK, false };

// constexpr float VIRTUAL_LENGTH_M = 1.50f; 
constexpr float VIRTUAL_LENGTH_M = 1.45f; 

// ============================================================================
// Wheel sensor
// ============================================================================

constexpr float WHEEL_CIRCUMFERENCE_M = 2.07f; // 26x2"
constexpr uint32_t REED_DEBOUNCE_MS = 120;
constexpr uint32_t SPEED_TIMEOUT_MS = 1500;
constexpr uint32_t STANDSTILL_TIMEOUT_MS = 10000;

constexpr float STANDSTILL_ANIMATION_SPEED = 5.0f;
constexpr float STANDSTILL_RAINBOW_SPEED = 0.5f;
constexpr float STANDSTILL_SCANNER_SPEED = 2.0f;
constexpr float STANDSTILL_FIRE_SPEED = 1.0f;
constexpr float STANDSTILL_SPEED_COLOR_SPEED = 1.0f;
constexpr float STANDSTILL_PULSE_SPEED = 0.3f;

volatile uint32_t reedLastInterruptUs = 0;
volatile uint32_t reedPulseUs = 0;
volatile bool reedPulsePending = false;

float speedKmh = 0.0f;
float smoothSpeedKmh = 0.0f;
float animationSpeedKmh = 0.0f;

uint32_t lastWheelPulseMs = 0;

// ============================================================================
// Wheel state
// ============================================================================

WheelState wheelState = WHEEL_DEMO;

// ============================================================================
// Animation
// ============================================================================

Effect currentEffect = EFFECT_RUNNING;

float animationPositionM = 0.0f;
float animationDirection = 1.0f;

constexpr bool ANIMATION_REVERSE = false;
constexpr float RUNNING_LENGTH_M = 0.12f;

// ============================================================================
// Animation button
// ============================================================================

constexpr uint32_t BUTTON_DEBOUNCE_MS = 50;

bool buttonLastState = HIGH;
uint32_t buttonLastChangeMs = 0;
bool buttonHandled = false;

// ============================================================================
// Central power limiter
// ============================================================================

constexpr float LED_FULL_WHITE_MA = 60.0f;
constexpr float MAX_LED_CURRENT_MA = 1700.0f;

float estimateLedCurrentmA()
{
    uint32_t rgbSum = 0;

    for (uint16_t i = 0; i < NUM_LEFT; i++)
    {
        rgbSum += ledsLeft[i].r;
        rgbSum += ledsLeft[i].g;
        rgbSum += ledsLeft[i].b;
    }

    for (uint16_t i = 0; i < NUM_RIGHT; i++)
    {
        rgbSum += ledsRight[i].r;
        rgbSum += ledsRight[i].g;
        rgbSum += ledsRight[i].b;
    }

    for (uint16_t i = 0; i < NUM_CRANK; i++)
    {
        rgbSum += ledsCrank[i].r;
        rgbSum += ledsCrank[i].g;
        rgbSum += ledsCrank[i].b;
    }

    return LED_FULL_WHITE_MA *
           ((float)rgbSum / (255.0f * 3.0f));
}

void scaleLEDs(float scale)
{
    if (scale >= 1.0f)
        return;

    scale = constrain(scale, 0.0f, 1.0f);

    for (uint16_t i = 0; i < NUM_LEFT; i++)
    {
        ledsLeft[i].r *= scale;
        ledsLeft[i].g *= scale;
        ledsLeft[i].b *= scale;
    }

    for (uint16_t i = 0; i < NUM_RIGHT; i++)
    {
        ledsRight[i].r *= scale;
        ledsRight[i].g *= scale;
        ledsRight[i].b *= scale;
    }

    for (uint16_t i = 0; i < NUM_CRANK; i++)
    {
        ledsCrank[i].r *= scale;
        ledsCrank[i].g *= scale;
        ledsCrank[i].b *= scale;
    }
}

void applyPowerLimiter()
{
    float currentmA = estimateLedCurrentmA();

    if (currentmA > MAX_LED_CURRENT_MA)
        scaleLEDs(MAX_LED_CURRENT_MA / currentmA);
}

// ============================================================================
// Virtual layout
// ============================================================================

float wrapPosition(float position, float length)
{
    if (length <= 0.0f)
        return 0.0f;

    while (position >= length)
        position -= length;

    while (position < 0.0f)
        position += length;

    return position;
}

int virtualPositionToLed(
    const Segment& segment,
    float positionM)
{
    float relative = positionM - segment.startMeter;

    if (relative < 0.0f || relative >= segment.lengthMeter)
        return -1;

    float normalized = relative / segment.lengthMeter;

    if (segment.reversed)
        normalized = 1.0f - normalized;

    int index = (int)(normalized * segment.ledCount);

    return constrain(
        index,
        0,
        (int)segment.ledCount - 1
    );
}

void setVirtualPixel(
    const Segment& segment,
    CRGB* leds,
    float positionM,
    const CRGB& color)
{
    int index = virtualPositionToLed(
        segment,
        positionM
    );

    if (index >= 0)
        leds[index] = color;
}

void setCargoBedPixel(
    float positionM,
    const CRGB& color)
{
    setVirtualPixel(
        segmentLeft,
        ledsLeft,
        positionM,
        color
    );

    setVirtualPixel(
        segmentRight,
        ledsRight,
        positionM,
        color
    );
}

void clearLEDs()
{
    fill_solid(ledsLeft, NUM_LEFT, CRGB::Black);
    fill_solid(ledsRight, NUM_RIGHT, CRGB::Black);
    fill_solid(ledsCrank, NUM_CRANK, CRGB::Black);
}

float getVirtualPosition(
    const Segment& segment,
    uint16_t ledIndex)
{
    float normalized;

    if (segment.ledCount <= 1)
    {
        normalized = 0.0f;
    }
    else
    {
        normalized =
            (float)ledIndex /
            (float)(segment.ledCount - 1);
    }

    if (segment.reversed)
        normalized = 1.0f - normalized;

    return segment.startMeter +
           normalized * segment.lengthMeter;
}

// ============================================================================
// Effects
// ============================================================================

void renderRunning()
{
    constexpr uint8_t TRAIL_STEPS = 5;

    for (uint8_t i = 0; i < TRAIL_STEPS; i++)
    {
        float offset =
            i * (RUNNING_LENGTH_M / TRAIL_STEPS);

        float p =
            ANIMATION_REVERSE
                ? animationPositionM + offset
                : animationPositionM - offset;

        p = wrapPosition(p, VIRTUAL_LENGTH_M);

        uint8_t brightness =
            255 - i * (255 / TRAIL_STEPS);

        CRGB color =
            CRGB(255, brightness, 0);

        if (p < segmentCrank.startMeter)
            setCargoBedPixel(p, color);
        else
            setVirtualPixel(
                segmentCrank,
                ledsCrank,
                p,
                color
            );
    }
}

void renderRainbow()
{
    constexpr uint16_t CYCLE_LENGTH = 255;

    auto renderSegment =
        [](CRGB* leds, const Segment& segment)
    {
        for (uint16_t i = 0; i < segment.ledCount; i++)
        {
            float p =
                getVirtualPosition(segment, i);

            float animatedPosition =
                wrapPosition(
                    p - animationPositionM,
                    VIRTUAL_LENGTH_M
                );

            uint8_t hue =
                (uint8_t)(
                    (animatedPosition /
                     VIRTUAL_LENGTH_M) *
                    CYCLE_LENGTH
                );

            leds[i] = CHSV(hue, 255, 255);
        }
    };

    renderSegment(ledsLeft, segmentLeft);
    renderSegment(ledsRight, segmentRight);
    renderSegment(ledsCrank, segmentCrank);
}

void renderWave()
{
    auto renderSegment =
        [](CRGB* leds, const Segment& segment)
    {
        for (uint16_t i = 0; i < segment.ledCount; i++)
        {
            float p =
                getVirtualPosition(segment, i);

            float animatedPosition =
                wrapPosition(
                    p - animationPositionM,
                    VIRTUAL_LENGTH_M
                );

            float wave =
                (sinf(
                    animatedPosition * 2.0f * PI *
                    2.0f / VIRTUAL_LENGTH_M
                ) + 1.0f) * 0.5f;

            uint8_t brightness =
                (uint8_t)(wave * 255.0f);

            leds[i] =
                CHSV(128, 255, brightness);
        }
    };

    renderSegment(ledsLeft, segmentLeft);
    renderSegment(ledsRight, segmentRight);
    renderSegment(ledsCrank, segmentCrank);
}

void renderScanner()
{
    constexpr float TRAIL_LENGTH_M = 0.25f;
    constexpr uint8_t TRAIL_STEPS = 12;

    for (uint8_t i = 0; i < TRAIL_STEPS; i++)
    {
        float p =
            animationPositionM -
            ((float)i / (TRAIL_STEPS - 1)) *
            TRAIL_LENGTH_M;

        p = wrapPosition(p, VIRTUAL_LENGTH_M);

        float fade =
            1.0f -
            ((float)i / (TRAIL_STEPS - 1));

        uint8_t brightness =
            (uint8_t)(255.0f * fade * fade);

        CRGB color =
            CRGB(
                brightness,
                brightness / 3,
                0
            );

        if (p < segmentCrank.startMeter)
            setCargoBedPixel(p, color);
        else
            setVirtualPixel(
                segmentCrank,
                ledsCrank,
                p,
                color
            );
    }
}

void renderFire()
{
    uint32_t time = millis();

    auto renderSegment =
        [time](CRGB* leds, const Segment& segment)
    {
        for (uint16_t i = 0; i < segment.ledCount; i++)
        {
            float p =
                getVirtualPosition(segment, i);

            uint16_t noiseX =
                (uint16_t)(
                    p * 255.0f
                );

            uint16_t noiseY =
                (uint16_t)(
                    time * 0.04f
                );

            uint8_t heat =
                inoise8(noiseX, noiseY);

            heat = scale8(heat, 240);

            CRGB color;

            if (heat < 85)
            {
                color = CRGB(
                    heat * 3,
                    0,
                    0
                );
            }
            else if (heat < 170)
            {
                color = CRGB(
                    255,
                    (heat - 85) * 3,
                    0
                );
            }
            else
            {
                color = CRGB(
                    255,
                    255,
                    (heat - 170) * 3
                );
            }

            leds[i] = color;
        }
    };

    renderSegment(ledsLeft, segmentLeft);
    renderSegment(ledsRight, segmentRight);
    renderSegment(ledsCrank, segmentCrank);
}

void renderSpeedColor()
{
    float speed =
        constrain(
            smoothSpeedKmh,
            0.0f,
            30.0f
        );

    uint8_t hue =
        map(
            (int)(speed * 10.0f),
            0,
            300,
            160,
            0
        );

    CRGB color =
        CHSV(hue, 255, 255);

    fill_solid(ledsLeft, NUM_LEFT, color);
    fill_solid(ledsRight, NUM_RIGHT, color);
    fill_solid(ledsCrank, NUM_CRANK, color);
}

void renderPulse()
{
    uint8_t brightness =
        beatsin8(
            30,
            20,
            255
        );

    CRGB color =
        CRGB(
            brightness,
            brightness / 4,
            0
        );

    fill_solid(ledsLeft, NUM_LEFT, color);
    fill_solid(ledsRight, NUM_RIGHT, color);
    fill_solid(ledsCrank, NUM_CRANK, color);
}

void renderSolid()
{
    fill_solid(ledsLeft, NUM_LEFT, CRGB::Red);
    fill_solid(ledsRight, NUM_RIGHT, CRGB::Red);
    fill_solid(ledsCrank, NUM_CRANK, CRGB::Red);
}

void renderEffect()
{
    switch (currentEffect)
    {
        case EFFECT_RUNNING:
            renderRunning();
            break;

        case EFFECT_RAINBOW:
            renderRainbow();
            break;

        case EFFECT_WAVE:
            renderWave();
            break;

        case EFFECT_SCANNER:
            renderScanner();
            break;

        case EFFECT_FIRE:
            renderFire();
            break;

        case EFFECT_SPEED_COLOR:
            renderSpeedColor();
            break;

        case EFFECT_PULSE:
            renderPulse();
            break;

        case EFFECT_SOLID:
            renderSolid();
            break;
    }
}

// ============================================================================
// Button
// ============================================================================

void processButton()
{
    bool buttonState =
        digitalRead(PIN_BUTTON);

    if (buttonState != buttonLastState)
    {
        buttonLastChangeMs = millis();
        buttonLastState = buttonState;
    }

    if (millis() - buttonLastChangeMs <
        BUTTON_DEBOUNCE_MS)
        return;

    if (buttonState == LOW)
    {
        if (!buttonHandled)
        {
            buttonHandled = true;

            setCurrentEffect(
                static_cast<Effect>(
                    (static_cast<int>(currentEffect) + 1) %
                    EFFECT_COUNT
                )
            );

            Serial.print("Effect changed to: ");
            Serial.println(
                getEffectName(currentEffect)
            );
        }
    }
    else
    {
        buttonHandled = false;
    }
}

// ============================================================================
// Reed sensor
// ============================================================================

void IRAM_ATTR reedISR()
{
    uint32_t nowUs = micros();

    if ((uint32_t)(
            nowUs - reedLastInterruptUs
        ) <
        REED_DEBOUNCE_MS * 1000UL)
        return;

    reedLastInterruptUs = nowUs;
    reedPulseUs = nowUs;
    reedPulsePending = true;
}

void processWheelSensor()
{
    uint32_t pulseUs = 0;

    noInterrupts();

    if (reedPulsePending)
    {
        pulseUs = reedPulseUs;
        reedPulsePending = false;
    }

    interrupts();

    if (pulseUs == 0)
        return;

    static uint32_t previousPulseUs = 0;

    lastWheelPulseMs = millis();

    if (wheelState != WHEEL_RUNNING)
    {
        wheelState = WHEEL_RUNNING;
        animationDirection = 1.0f;

        // First pulse after a stop establishes the timing reference.
        previousPulseUs = pulseUs;

        Serial.println("Wheel moving - RUNNING");
        return;
    }

    if (previousPulseUs != 0)
    {
        uint32_t intervalUs =
            pulseUs - previousPulseUs;

        if (intervalUs > 0)
        {
            float intervalSeconds =
                intervalUs / 1000000.0f;

            speedKmh =
                (WHEEL_CIRCUMFERENCE_M /
                 intervalSeconds) *
                3.6f;
        }
    }

    previousPulseUs = pulseUs;
}

// ============================================================================
// Wheel state machine
// ============================================================================

void updateWheelState()
{
    if (lastWheelPulseMs == 0)
        return;

    uint32_t elapsed =
        millis() - lastWheelPulseMs;

    if (wheelState == WHEEL_RUNNING &&
        elapsed > SPEED_TIMEOUT_MS)
    {
        wheelState = WHEEL_STOPPED;
        speedKmh = 0.0f;

        Serial.println("Wheel stopped");
    }

    if (wheelState == WHEEL_STOPPED &&
        elapsed > STANDSTILL_TIMEOUT_MS)
    {
        wheelState = WHEEL_DEMO;

        speedKmh = 0.0f;
        animationDirection = 1.0f;

        Serial.println("Standstill demo mode");
    }
}

// ============================================================================
// Speed smoothing / animation speed
// ============================================================================

void updateSmoothSpeed()
{
    constexpr float ACCEL_SMOOTHING = 0.08f;
    constexpr float DECEL_SMOOTHING = 0.15f;

    float difference =
        speedKmh - smoothSpeedKmh;

    float smoothing =
        (difference > 0.0f)
            ? ACCEL_SMOOTHING
            : DECEL_SMOOTHING;

    smoothSpeedKmh +=
        difference * smoothing;

    if (fabs(smoothSpeedKmh) < 0.05f)
        smoothSpeedKmh = 0.0f;
}

void updateAnimationSpeed()
{
    if (wheelState != WHEEL_DEMO)
    {
        animationSpeedKmh =
            smoothSpeedKmh;

        return;
    }

    switch (currentEffect)
    {
        case EFFECT_RAINBOW:
            animationSpeedKmh =
                STANDSTILL_RAINBOW_SPEED;
            break;

        case EFFECT_SCANNER:
            animationSpeedKmh =
                STANDSTILL_SCANNER_SPEED;
            break;

        case EFFECT_FIRE:
            animationSpeedKmh =
                STANDSTILL_FIRE_SPEED;
            break;

        case EFFECT_SPEED_COLOR:
            animationSpeedKmh =
                STANDSTILL_SPEED_COLOR_SPEED;
            break;

        case EFFECT_PULSE:
            animationSpeedKmh =
                STANDSTILL_PULSE_SPEED;
            break;

        default:
            animationSpeedKmh =
                STANDSTILL_ANIMATION_SPEED;
            break;
    }
}

// ============================================================================
// Animation movement
// ============================================================================

void updateAnimationPosition()
{
    static uint32_t lastUpdateMs = 0;

    uint32_t now = millis();

    if (lastUpdateMs == 0)
    {
        lastUpdateMs = now;
        return;
    }

    uint32_t elapsedMs =
        now - lastUpdateMs;

    lastUpdateMs = now;

    if (wheelState == WHEEL_STOPPED)
        return;

    float distanceM =
        (animationSpeedKmh / 3.6f) *
        (elapsedMs / 1000.0f);

    if (wheelState == WHEEL_DEMO)
    {
        animationPositionM +=
            distanceM * animationDirection;

        if (animationPositionM >=
            VIRTUAL_LENGTH_M)
        {
            animationPositionM =
                VIRTUAL_LENGTH_M;

            animationDirection =
                -1.0f;
        }
        else if (animationPositionM <= 0.0f)
        {
            animationPositionM = 0.0f;
            animationDirection = 1.0f;
        }

        return;
    }

    if (ANIMATION_REVERSE)
        animationPositionM -= distanceM;
    else
        animationPositionM += distanceM;

    animationPositionM =
        wrapPosition(
            animationPositionM,
            VIRTUAL_LENGTH_M
        );
}

// ============================================================================
// Controller API for WebServer.cpp
// ============================================================================

Effect getCurrentEffect()
{
    return currentEffect;
}

void setCurrentEffect(Effect effect)
{
    if (effect < EFFECT_RUNNING ||
        effect > EFFECT_SOLID)
        return;

    currentEffect = effect;
}

WheelState getWheelState()
{
    return wheelState;
}

float getSpeedKmh()
{
    return speedKmh;
}

float getSmoothSpeedKmh()
{
    return smoothSpeedKmh;
}

float getAnimationSpeedKmh()
{
    return animationSpeedKmh;
}

const char* getEffectName(Effect effect)
{
    switch (effect)
    {
        case EFFECT_RUNNING:     return "RUNNING";
        case EFFECT_RAINBOW:     return "RAINBOW";
        case EFFECT_WAVE:        return "WAVE";
        case EFFECT_SCANNER:     return "SCANNER";
        case EFFECT_FIRE:        return "FIRE";
        case EFFECT_SPEED_COLOR: return "SPEED COLOR";
        case EFFECT_PULSE:       return "PULSE";
        case EFFECT_SOLID:       return "SOLID";
        default:                 return "UNKNOWN";
    }
}

const char* getWheelStateName(WheelState state)
{
    switch (state)
    {
        case WHEEL_RUNNING: return "RUNNING";
        case WHEEL_STOPPED: return "STOPPED";
        case WHEEL_DEMO:    return "DEMO";
        default:            return "UNKNOWN";
    }
}

// ============================================================================
// Setup
// ============================================================================

void setup()
{
    Serial.begin(115200);
    delay(500);

    pinMode(PIN_REED, INPUT_PULLUP);
    pinMode(PIN_BUTTON, INPUT_PULLUP);

    attachInterrupt(
        digitalPinToInterrupt(PIN_REED),
        reedISR,
        FALLING
    );

    FastLED.addLeds<
        LED_TYPE,
        PIN_LEFT,
        COLOR_ORDER
    >(ledsLeft, NUM_LEFT);

    FastLED.addLeds<
        LED_TYPE,
        PIN_RIGHT,
        COLOR_ORDER
    >(ledsRight, NUM_RIGHT);

    FastLED.addLeds<
        LED_TYPE,
        PIN_CRANK,
        COLOR_ORDER
    >(ledsCrank, NUM_CRANK);

    FastLED.clear(true);

    webServerSetup();

    Serial.println();
    Serial.println("================================");
    Serial.println("Bullitt WS2815 controller started");
    Serial.println("================================");
    Serial.println("Power limiter enabled");
}

// ============================================================================
// Main loop
// ============================================================================

void loop()
{
    processWheelSensor();
    updateWheelState();
    updateSmoothSpeed();
    processButton();
    updateAnimationSpeed();
    updateAnimationPosition();

    clearLEDs();
    renderEffect();
    applyPowerLimiter();

    FastLED.show();

    webServerLoop();

    delay(10);

    static uint32_t lastDebugMs = 0;

    if (millis() - lastDebugMs >= 1000)
    {
        lastDebugMs = millis();

        Serial.print("State: ");
        Serial.print(getWheelStateName(wheelState));

        Serial.print(", Speed: ");
        Serial.print(speedKmh, 1);

        Serial.print(" km/h, smooth: ");
        Serial.print(smoothSpeedKmh, 1);

        Serial.print(" km/h, animation: ");
        Serial.print(animationSpeedKmh, 1);

        Serial.println(" km/h");
    }
}
