/**
 * @file lv_indev_predict.c
 * Touch/pointer coordinate prediction implementation
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_indev_predict.h"
#include "../tick/lv_tick.h"
#include "../misc/lv_math.h"
#include "../stdlib/lv_string.h"
#include "../misc/lv_log.h"

/*********************
 *      DEFINES
 *********************/

/** Minimum time difference to calculate velocity (avoid division by zero) */
#define MIN_TIME_DIFF_MS    4

/** Scale factor for velocity calculations (fixed-point math) */
#define VELOCITY_SCALE      256

/** Smoothing factor for velocity (0-256, higher = more smoothing) */
#define VELOCITY_SMOOTH     192

/** Maximum valid velocity (pixels per second) - filter out noise */
#define MAX_VELOCITY        5000

/**********************
 *  STATIC PROTOTYPES
 **********************/
static int32_t weighted_average_velocity(const lv_indev_predict_t * ctx, bool is_x);
static int32_t clamp_value(int32_t value, int32_t min_val, int32_t max_val);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_indev_predict_init(lv_indev_predict_t * ctx)
{
    if(ctx == NULL) return;

    lv_memzero(ctx, sizeof(lv_indev_predict_t));
    ctx->enabled = true;
    ctx->predict_time_ms = LV_INDEV_PREDICT_TIME_MS;
    ctx->max_distance = LV_INDEV_PREDICT_MAX_DIST;
}

void lv_indev_predict_reset(lv_indev_predict_t * ctx)
{
    if(ctx == NULL) return;

    ctx->hist_index = 0;
    ctx->hist_count = 0;
    ctx->velocity.x = 0;
    ctx->velocity.y = 0;
    ctx->last_predicted.x = 0;
    ctx->last_predicted.y = 0;
    ctx->last_predict_time = 0;

    lv_memzero(ctx->hist, sizeof(ctx->hist));
}

void lv_indev_predict_enable(lv_indev_predict_t * ctx, bool en)
{
    if(ctx == NULL) return;
    ctx->enabled = en;
}

void lv_indev_predict_set_time(lv_indev_predict_t * ctx, int32_t time_ms)
{
    if(ctx == NULL) return;
    ctx->predict_time_ms = time_ms;
}

void lv_indev_predict_set_max_distance(lv_indev_predict_t * ctx, int32_t max_dist)
{
    if(ctx == NULL) return;
    ctx->max_distance = max_dist;
}

void lv_indev_predict_add_point(lv_indev_predict_t * ctx, const lv_point_t * point, uint32_t timestamp)
{
    if(ctx == NULL || point == NULL) return;

    /* Store the new point in history */
    ctx->hist[ctx->hist_index].point = *point;
    ctx->hist[ctx->hist_index].timestamp = timestamp;

    ctx->hist_index = (ctx->hist_index + 1) % LV_INDEV_PREDICT_HIST_SIZE;
    if(ctx->hist_count < LV_INDEV_PREDICT_HIST_SIZE) {
        ctx->hist_count++;
    }

    /* Recalculate velocity with new data */
    lv_indev_predict_calc_velocity(ctx);
}

bool lv_indev_predict_get_point(lv_indev_predict_t * ctx, uint32_t target_time, lv_point_t * predicted_point)
{
    LV_UNUSED(target_time);

    if(ctx == NULL || predicted_point == NULL || !ctx->enabled) {
        return false;
    }

    /* Need at least 2 points to predict */
    if(ctx->hist_count < 2) {
        return false;
    }

    /* Get the most recent point */
    uint8_t last_idx = (ctx->hist_index + LV_INDEV_PREDICT_HIST_SIZE - 1) % LV_INDEV_PREDICT_HIST_SIZE;
    const lv_indev_predict_point_t * last_point = &ctx->hist[last_idx];

    /* Use fixed prediction time instead of timestamp difference to avoid sync issues */
    int32_t dt_ms = ctx->predict_time_ms;

    /* Check if velocity is too low to predict (avoid jitter) */
    int32_t speed_sq = (ctx->velocity.x * ctx->velocity.x + ctx->velocity.y * ctx->velocity.y) /
                       (VELOCITY_SCALE * VELOCITY_SCALE);
    if(speed_sq < 100 * 100) {  /* Less than 100 pixels/second */
        *predicted_point = last_point->point;
        return true;
    }

    /* Calculate predicted displacement
     * velocity is in pixels/second scaled by VELOCITY_SCALE
     * dt_ms is in milliseconds
     */
    int32_t pred_x = (ctx->velocity.x * dt_ms) / (1000 * VELOCITY_SCALE);
    int32_t pred_y = (ctx->velocity.y * dt_ms) / (1000 * VELOCITY_SCALE);

    /* Clamp prediction distance */
    pred_x = clamp_value(pred_x, -ctx->max_distance, ctx->max_distance);
    pred_y = clamp_value(pred_y, -ctx->max_distance, ctx->max_distance);

    /* Calculate predicted point */
    predicted_point->x = last_point->point.x + pred_x;
    predicted_point->y = last_point->point.y + pred_y;

    /* Store for potential debugging/analysis */
    ctx->last_predicted = *predicted_point;
    ctx->last_predict_time = lv_tick_get();

    return true;
}

bool lv_indev_predict_calc_velocity(lv_indev_predict_t * ctx)
{
    if(ctx == NULL || ctx->hist_count < 2) {
        return false;
    }

    /* Calculate new velocity from weighted average */
    int32_t new_vx = weighted_average_velocity(ctx, true);
    int32_t new_vy = weighted_average_velocity(ctx, false);

    /* Filter out unreasonable velocities (noise) */
    if(LV_ABS(new_vx) > MAX_VELOCITY * VELOCITY_SCALE) new_vx = 0;
    if(LV_ABS(new_vy) > MAX_VELOCITY * VELOCITY_SCALE) new_vy = 0;

    /* Apply exponential smoothing to reduce jitter */
    ctx->velocity.x = (ctx->velocity.x * VELOCITY_SMOOTH + new_vx * (256 - VELOCITY_SMOOTH)) / 256;
    ctx->velocity.y = (ctx->velocity.y * VELOCITY_SMOOTH + new_vy * (256 - VELOCITY_SMOOTH)) / 256;

    return true;
}

void lv_indev_predict_get_velocity(const lv_indev_predict_t * ctx, lv_point_t * velocity)
{
    if(ctx == NULL || velocity == NULL) return;
    *velocity = ctx->velocity;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * Calculate weighted average velocity from history
 * More recent samples get higher weight
 * Uses linear regression-like approach for better accuracy
 */
static int32_t weighted_average_velocity(const lv_indev_predict_t * ctx, bool is_x)
{
    if(ctx->hist_count < 2) return 0;

    int32_t total_weight = 0;
    int64_t weighted_velocity = 0;

    /* Start from the most recent point and go backwards */
    uint8_t curr_idx = (ctx->hist_index + LV_INDEV_PREDICT_HIST_SIZE - 1) % LV_INDEV_PREDICT_HIST_SIZE;

    for(uint8_t i = 0; i < ctx->hist_count - 1; i++) {
        uint8_t prev_idx = (curr_idx + LV_INDEV_PREDICT_HIST_SIZE - 1) % LV_INDEV_PREDICT_HIST_SIZE;

        const lv_indev_predict_point_t * curr = &ctx->hist[curr_idx];
        const lv_indev_predict_point_t * prev = &ctx->hist[prev_idx];

        /* Calculate time difference */
        int32_t dt = (int32_t)(curr->timestamp - prev->timestamp);
        if(dt < MIN_TIME_DIFF_MS) {
            /* Skip if timestamps are too close */
            curr_idx = prev_idx;
            continue;
        }

        /* Calculate position difference */
        int32_t dp = is_x ? (curr->point.x - prev->point.x) : (curr->point.y - prev->point.y);

        /* Calculate velocity for this segment (pixels per second, scaled) */
        int32_t segment_velocity = (dp * 1000 * VELOCITY_SCALE) / dt;

        /* Weight: more recent segments get higher weight
         * Weight decreases exponentially with age
         * Most recent: weight = hist_count-1, next: hist_count-2, etc.
         */
        int32_t weight = (int32_t)(ctx->hist_count - 1 - i);
        if(weight <= 0) weight = 1;

        weighted_velocity += (int64_t)segment_velocity * weight;
        total_weight += weight;

        curr_idx = prev_idx;
    }

    if(total_weight == 0) return 0;

    return (int32_t)(weighted_velocity / total_weight);
}

/**
 * Clamp a value to a range
 */
static int32_t clamp_value(int32_t value, int32_t min_val, int32_t max_val)
{
    if(value < min_val) return min_val;
    if(value > max_val) return max_val;
    return value;
}
