/* navigation.c — Pure-math sensor fusion (no hardware access).
 *
 * Orientation: complementary filter. Roll/pitch come from the gravity
 * vector (accelerometer); yaw (heading) is integrated from gyro Z.
 *
 * Position: dead-reckoning by double-integrating horizontal acceleration,
 * bounded by a Zero-velocity UPdaTe (ZUPT). HONEST CAVEAT: integrating
 * consumer-MEMS accelerometer noise diverges in seconds — expect metres of
 * drift over tens of seconds. Heading/bearing is reliable; absolute
 * displacement is the weak link. The model assumes the board is held
 * roughly flat (gravity on Z, ax/ay horizontal).
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

static float deadband(float v, float band)
{
    return (v > band || v < -band) ? v : 0.0f;
}

void Nav_Init(NavState *n, const MPU6050_Scaled *accel_ref)
{
    n->pos.x = n->pos.y = n->pos.z = 0.0f;
    n->vel_x = n->vel_y = 0.0f;
    n->heading_deg = 0.0f;
    n->distance_m = 0.0f;
    n->bearing_deg = 0.0f;
    n->still_count = 0;

    /* Seed roll/pitch from the measured gravity direction at rest. */
    if (accel_ref) {
        float ax = accel_ref->ax, ay = accel_ref->ay, az = accel_ref->az;
        n->roll_deg  = atan2f(ay, az) * RAD2DEG;
        n->pitch_deg = atan2f(-ax, sqrtf(ay * ay + az * az)) * RAD2DEG;
    } else {
        n->roll_deg = n->pitch_deg = 0.0f;
    }
}

void Nav_Update(NavState *n, const MPU6050_Scaled *s,
                const Coordinate *target, float dt)
{
    /* ---- 1. Orientation (complementary filter) ---- */
    float roll_acc  = atan2f(s->ay, s->az) * RAD2DEG;
    float pitch_acc = atan2f(-s->ax, sqrtf(s->ay * s->ay + s->az * s->az)) * RAD2DEG;

    n->roll_deg  = COMP_FILTER_ALPHA * (n->roll_deg  + s->gx * dt) +
                   (1.0f - COMP_FILTER_ALPHA) * roll_acc;
    n->pitch_deg = COMP_FILTER_ALPHA * (n->pitch_deg + s->gy * dt) +
                   (1.0f - COMP_FILTER_ALPHA) * pitch_acc;

    /* Heading (yaw) integrates gyro Z; no absolute yaw reference exists. */
    n->heading_deg = wrap360(n->heading_deg + s->gz * dt);

    /* ---- 2. Horizontal linear acceleration (body frame, flat assumption) ---- */
    float ax_lin = deadband(s->ax, ACC_DEADBAND_MS2);
    float ay_lin = deadband(s->ay, ACC_DEADBAND_MS2);

    /* Rotate body horizontal accel into the world frame by heading. */
    float h = n->heading_deg * DEG2RAD;
    float ch = cosf(h), sh = sinf(h);
    float wax = ax_lin * ch - ay_lin * sh;
    float way = ax_lin * sh + ay_lin * ch;

    /* ---- 3. Zero-velocity update (ZUPT) ---- */
    float amag = sqrtf(s->ax * s->ax + s->ay * s->ay + s->az * s->az);
    float gmag = sqrtf(s->gx * s->gx + s->gy * s->gy + s->gz * s->gz);
    int still = (fabsf(amag - GRAVITY_MS2) < ZUPT_ACCEL_BAND_MS2) &&
                (gmag < ZUPT_GYRO_BAND_DPS);
    n->still_count = still ? (n->still_count + 1) : 0;

    /* ---- 4. Integrate accel -> velocity -> position ---- */
    n->vel_x += wax * dt;
    n->vel_y += way * dt;
    if (n->still_count >= ZUPT_STILL_SAMPLES) {
        n->vel_x = 0.0f;   /* clamp accumulated drift while stationary */
        n->vel_y = 0.0f;
    }
    n->pos.x += n->vel_x * dt;
    n->pos.y += n->vel_y * dt;

    /* ---- 5. Range + bearing to target ---- */
    float dx = target->x - n->pos.x;
    float dy = target->y - n->pos.y;
    n->distance_m  = sqrtf(dx * dx + dy * dy);
    n->bearing_deg = wrap360(atan2f(dy, dx) * RAD2DEG);
}

float Nav_RelativeBearing(const NavState *n)
{
    return wrap180(n->bearing_deg - n->heading_deg);
}
