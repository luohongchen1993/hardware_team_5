#ifndef NAVIGATION_H
#define NAVIGATION_H

#include <stdint.h>
#include "mpu6050.h"

/* World-frame coordinate, metres. Origin = power-up pose. (spec: float x,y,z) */
typedef struct { float x, y, z; } Coordinate;

/* Fused navigation state. Outputs: pos, heading_deg, distance_m, bearing_deg.
 * The remaining fields are integrator internals. */
typedef struct {
    Coordinate pos;          /* estimated position (m) — OUTPUT (drift-prone, see .c) */
    float vel_x, vel_y;      /* world-frame velocity (m/s) — internal */
    float heading_deg;       /* yaw vs origin (deg) — OUTPUT */
    float roll_deg, pitch_deg; /* tilt estimate — internal */
    float distance_m;        /* Euclidean distance to target (m) — OUTPUT */
    float bearing_deg;       /* absolute bearing to target (deg, world) — OUTPUT */
    uint32_t still_count;    /* consecutive "still" samples for ZUPT — internal */
} NavState;

/* Zero the estimator; seed tilt from the at-rest accelerometer reference. */
void Nav_Init(NavState *n, const MPU6050_Scaled *accel_ref);

/* One fusion step (call every dt seconds). Updates orientation by
 * complementary filter, dead-reckons position with ZUPT, and recomputes
 * distance/bearing to the target. */
void Nav_Update(NavState *n, const MPU6050_Scaled *s,
                const Coordinate *target, float dt);

/* Bearing to target relative to current heading, wrapped to (-180, 180]. */
float Nav_RelativeBearing(const NavState *n);

#endif /* NAVIGATION_H */
