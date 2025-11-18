#pragma once
#include <math.h>
#include <xhl/debug.h>

typedef struct SmoothedValue
{
    float current;
    float target;
    float inc;
    int   remaining;
} SmoothedValue;

static inline void smoothvalue_reset(SmoothedValue* sv, float newValue)
{
    sv->target = sv->current = newValue;
    sv->inc                  = 0;
    sv->remaining            = 0;
}

// Produces very few instructions
static inline float smoothvalue_tick(SmoothedValue* sv)
{
    float v = sv->current;
    if (sv->remaining-- > 0)
        sv->current += sv->inc;
    return v;
}

// I think this may be better than the above? FP rounding errors accumulate after LOTS of additions, and multiplies are
// more stable (Might be wrong on that assumption).
// static float smoothvalue_tick5(SmoothedValue* sv)
// {
//     if (sv->remaining-- > 0)
//         sv->current += sv->inc;
//     return sv->target - sv->remaining * sv->inc;
// }

// Not exactly like tick above, but "close enough" and fast
static inline float smoothvalue_tickn(SmoothedValue* sv, int steps)
{
    int num_steps  = steps < sv->remaining ? steps : sv->remaining;
    sv->current   += sv->inc * num_steps;
    sv->remaining -= num_steps;
    return sv->current;
}

static void smoothvalue_set_target(SmoothedValue* sv, float newValue, int steps)
{
    xassert(steps);
    if (sv->target == newValue && sv->remaining)
        return;

    float inc  = 0;
    float diff = newValue - sv->current;

    while (inc == 0 && newValue != sv->target && steps)
    {
        inc = diff / (float)steps;
        xassert(inc == inc);
        // Reducing the amount of steps for micro param changes helps reduce accumulated fp rounding errors
        // By my own estimation, reducing the "accuracy" of smoothing time is better than processing tiny changes
        if (fabsf(inc) < 1.0e-5f)
        {
            inc     = 0;
            steps >>= 1;
        }
    }
    // If the change is too small (see above), or if there was no change, snap to new value
    if (inc == 0)
    {
        sv->current = newValue;
        steps       = 0;
    }

    sv->target    = newValue;
    sv->inc       = inc;
    sv->remaining = steps;
}