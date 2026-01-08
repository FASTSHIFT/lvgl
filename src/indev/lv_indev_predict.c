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
#include "../misc/lv_assert.h"
#include "../misc/lv_log.h"
#include "../stdlib/lv_sprintf.h"

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

/** Error smoothing factor (0-256, higher = more smoothing) */
#define ERROR_SMOOTH        224

/** Maximum error samples to average */
#define ERROR_MAX_SAMPLES   32

/**********************
 *  STATIC PROTOTYPES
 **********************/
static int32_t weighted_average_velocity(const lv_indev_predict_t * ctx, bool is_x);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_indev_predict_init(lv_indev_predict_t * ctx)
{
    LV_ASSERT_NULL(ctx);

    lv_memzero(ctx, sizeof(lv_indev_predict_t));
    ctx->predict_time_ms = LV_INDEV_PREDICT_TIME_MS;
    ctx->max_distance = LV_INDEV_PREDICT_MAX_DIST;
}

void lv_indev_predict_reset(lv_indev_predict_t * ctx)
{
    LV_ASSERT_NULL(ctx);

    ctx->hist_index = 0;
    ctx->hist_count = 0;
    ctx->velocity.x = 0;
    ctx->velocity.y = 0;
    ctx->last_predicted.x = 0;
    ctx->last_predicted.y = 0;
    ctx->last_predict_time = 0;
    ctx->error_sum = 0;
    ctx->error_count = 0;
    ctx->avg_error = 0;
    ctx->has_pending_prediction = false;

    lv_memzero(ctx->hist, sizeof(ctx->hist));
}

void lv_indev_predict_enable(lv_indev_predict_t * ctx, bool en)
{
    LV_ASSERT_NULL(ctx);
    ctx->enabled = en;
}

void lv_indev_predict_set_time(lv_indev_predict_t * ctx, int32_t time_ms)
{
    LV_ASSERT_NULL(ctx);
    ctx->predict_time_ms = time_ms;
}

void lv_indev_predict_set_max_distance(lv_indev_predict_t * ctx, int32_t max_dist)
{
    LV_ASSERT_NULL(ctx);
    ctx->max_distance = max_dist;
}

void lv_indev_predict_add_point(lv_indev_predict_t * ctx, const lv_point_t * point, uint32_t timestamp)
{
    LV_ASSERT_NULL(ctx);
    LV_ASSERT_NULL(point);

    /* Evaluate prediction accuracy if there's a pending prediction */
    if(ctx->has_pending_prediction && ctx->enabled) {
        /* Calculate error between predicted point and actual point */
        int32_t err_x = point->x - ctx->last_predicted.x;
        int32_t err_y = point->y - ctx->last_predicted.y;

        /* Calculate error magnitude (using Manhattan distance for simplicity, scaled by 256) */
        int32_t error = (LV_ABS(err_x) + LV_ABS(err_y)) * 256;

        /* Update running average with exponential smoothing */
        if(ctx->error_count == 0) {
            ctx->avg_error = error;
        }
        else {
            ctx->avg_error = (ctx->avg_error * ERROR_SMOOTH + error * (256 - ERROR_SMOOTH)) / 256;
        }

        if(ctx->error_count < ERROR_MAX_SAMPLES) {
            ctx->error_count++;
        }

        ctx->has_pending_prediction = false;

        LV_LOG_USER("predict error: %" LV_PRId32 " px, avg: %" LV_PRId32 ".%02" LV_PRId32 " px",
                    error / 256, ctx->avg_error / 256, (ctx->avg_error % 256) * 100 / 256);
    }

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

    LV_ASSERT_NULL(ctx);
    LV_ASSERT_NULL(predicted_point);

    if(!ctx->enabled) {
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
    int64_t vx = ctx->velocity.x;
    int64_t vy = ctx->velocity.y;
    int64_t speed_sq = (vx * vx + vy * vy) / ((int64_t)VELOCITY_SCALE * VELOCITY_SCALE);
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
    pred_x = LV_CLAMP(-ctx->max_distance, pred_x, ctx->max_distance);
    pred_y = LV_CLAMP(-ctx->max_distance, pred_y, ctx->max_distance);

    /* Calculate predicted point */
    predicted_point->x = last_point->point.x + pred_x;
    predicted_point->y = last_point->point.y + pred_y;

    /* Log prediction offset */
    if(pred_x != 0 || pred_y != 0) {
        LV_LOG_TRACE("predict offset: (%" LV_PRId32 ", %" LV_PRId32 ")", pred_x, pred_y);
    }

    /* Store for potential debugging/analysis */
    ctx->last_predicted = *predicted_point;
    ctx->last_predict_time = lv_tick_get();
    ctx->has_pending_prediction = true;

    return true;
}

bool lv_indev_predict_calc_velocity(lv_indev_predict_t * ctx)
{
    LV_ASSERT_NULL(ctx);

    if(ctx->hist_count < 2) {
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
    LV_ASSERT_NULL(ctx);
    LV_ASSERT_NULL(velocity);
    *velocity = ctx->velocity;
}

int32_t lv_indev_predict_get_avg_error(const lv_indev_predict_t * ctx)
{
    LV_ASSERT_NULL(ctx);
    if(ctx->error_count == 0) return 0;
    return ctx->avg_error;
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
