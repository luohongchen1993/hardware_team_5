/* navigation.c — Pure-math sensor fusion (no hardware access).
 *
 * Heading: yaw integrated from gyro Z (gyro bias removed in mpu6050.c).
 *
 * Position: PEDESTRIAN DEAD-RECKONING (a pedometer). We detect each walking
 * step from the accelerometer magnitude and advance the position by one fixed
 * stride along the current heading. Unlike double-integrating acceleration,
 * each step is a discrete, bounded update, so position error does NOT snowball
 * quadratically, and tilting the board no longer injects phantom motion
 * (|accel| is orientation-independent). This makes "walk toward the target and
 * the distance actually shrinks" work for a real demo.
 *
 * Step detection: keep a slow low-pass of |accel| as the gravity baseline; the
 * dynamic component (|accel| - baseline) swings up on each footfall. Count a
 * step on an upward threshold crossing, with hysteresis (must fall back below a
 * re-arm level) and a refractory window (minimum samples between steps) to avoid
 * double-counting one footfall.
 *
 * Complexity: O(1) time, O(1) space per call. */

#include "navigation.h"
#include "config.h"
#include <math.h>

#define DEG2RAD   0.01745329252f
#define RAD2DEG   57.2957795131f

static float wrap360(float a)
{
    while (a >= 360.0f) a -= 360.0f;
    while (a < 0.0f)    a += 360.0f;
    return a;
}

static float wrap180(float a)
{
    while (a >   180.0f) a -= 360.0f;
    while (a <= -180.0f) a += 360.0f;
    return a;
}

void Nav_Init(NavState *n, const MPU6050_Scaled *accel_ref)
{
    n->pos.x = n->pos.y = n->pos.z = 0.0f;
    n->heading_deg = 0.0f;
    n->distance_m  = 0.0f;
    n->bearing_deg = 0.0f;
    n->steps       = 0;

    /* Seed the gravity baseline with the measured resting |accel| (~9.81),
     * so the detector does not fire a phantom step on the first samples. */
    if (accel_ref) {
        n->acc_lp = sqrtf(accel_ref->ax * accel_ref->ax +
                          accel_ref->ay * accel_ref->ay +
                          accel_ref->az * accel_ref->az);
    } else {
        n->acc_lp = GRAVITY_MS2;
    }
    n->step_armed = 1;
    n->since_step = STEP_MIN_INTERVAL_SAMPLES;   /* allow the first step immediately */
}

void Nav_Update(NavState *n, const MPU6050_Scaled *s,
                const Coordinate *target, float dt)
{
    /* ---- 1. Heading (yaw) from gyro Z ---- */
    n->heading_deg = wrap360(n->heading_deg + s->gz * dt);

    /* ---- 2. Step detection from accel magnitude (tilt-independent) ---- */
    float amag = sqrtf(s->ax * s->ax + s->ay * s->ay + s->az * s->az);
    n->acc_lp += STEP_LP_ALPHA * (amag - n->acc_lp);   /* slow gravity baseline */
    float dyn = amag - n->acc_lp;                       /* ~0 at rest, peaks per step */

    if (n->since_step < 0xFFFFu) n->since_step++;

    if (n->step_armed &&
        dyn > STEP_ACCEL_THRESH_MS2 &&
        n->since_step >= STEP_MIN_INTERVAL_SAMPLES) {
        /* Footfall: advance one stride along the current heading. */
        float h = n->heading_deg * DEG2RAD;
        n->pos.x += STRIDE_M * cosf(h);
        n->pos.y += STRIDE_M * sinf(h);
        n->steps++;
        n->step_armed = 0;          /* disarm until the peak passes */
        n->since_step = 0;
    } else if (!n->step_armed && dyn < STEP_REARM_MS2) {
        n->step_armed = 1;          /* hysteresis: re-arm for the next step */
    }

    /* ---- 3. Range + bearing to target ---- */
    float dx = target->x - n->pos.x;
    float dy = target->y - n->pos.y;
    n->distance_m  = sqrtf(dx * dx + dy * dy);
    n->bearing_deg = wrap360(atan2f(dy, dx) * RAD2DEG);
}

float Nav_RelativeBearing(const NavState *n)
{
    return wrap180(n->bearing_deg - n->heading_deg);
}
