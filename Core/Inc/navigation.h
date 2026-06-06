#ifndef NAVIGATION_H
#define NAVIGATION_H

#include <stdint.h>
#include "mpu6050.h"

/* World-frame coordinate, metres. Origin = power-up pose. (spec: float x,y,z) */
typedef struct { float x, y, z; } Coordinate;

/* Fused navigation state.
 * Outputs: pos, heading_deg, distance_m, bearing_deg, steps.
 * The remaining fields are step-detector internals. */
typedef struct {
    Coordinate pos;        /* estimated position (m) from step dead-reckoning — OUTPUT */
    float heading_deg;     /* yaw vs origin (deg), gyro-integrated — OUTPUT */
    float distance_m;      /* Euclidean distance to target (m) — OUTPUT */
    float bearing_deg;     /* absolute bearing to target (deg, world) — OUTPUT */
    uint32_t steps;        /* total steps detected — OUTPUT (telemetry) */
    /* --- step detector internals --- */
    float    acc_lp;       /* low-passed |accel| (gravity baseline) */
    uint16_t since_step;   /* samples since last counted step (refractory) */
    uint8_t  step_armed;   /* 1 = ready to register the next step peak */
} NavState;

/* Zero the estimator; seed the gravity baseline from the at-rest accel reference. */
void Nav_Init(NavState *n, const MPU6050_Scaled *accel_ref);

/* One fusion step (call every dt seconds): integrate heading from gyro Z, detect
 * walking steps from accel magnitude and advance one stride per step along the
 * current heading, then recompute distance/bearing to the target.
 *
 * Position uses STEP DEAD-RECKONING (a pedometer), not acceleration double-
 * integration: each step is a bounded, discrete update, so error does not
 * snowball. Holding the board tilted no longer injects phantom motion. */
void Nav_Update(NavState *n, const MPU6050_Scaled *s,
                const Coordinate *target, float dt);

/* Bearing to target relative to current heading, wrapped to (-180, 180]. */
float Nav_RelativeBearing(const NavState *n);

#endif /* NAVIGATION_H */
