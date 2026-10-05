#pragma once

#include <Arduino.h>

enum Effect
{
    EFFECT_RUNNING,
    EFFECT_RAINBOW,
    EFFECT_WAVE,
    EFFECT_SCANNER,
    EFFECT_FIRE,
    EFFECT_SPEED_COLOR,
    EFFECT_PULSE,
    EFFECT_SOLID
};

constexpr uint8_t EFFECT_COUNT = 8;

enum WheelState
{
    WHEEL_RUNNING,
    WHEEL_STOPPED,
    WHEEL_DEMO
};

// Webserver interface
Effect getCurrentEffect();
void setCurrentEffect(Effect effect);

WheelState getWheelState();
float getSpeedKmh();
float getSmoothSpeedKmh();
float getAnimationSpeedKmh();

const char* getEffectName(Effect effect);
const char* getWheelStateName(WheelState state);
