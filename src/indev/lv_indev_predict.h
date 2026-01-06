/**
 * @file lv_indev_predict.h
 * Touch/pointer coordinate prediction for improved responsiveness
 *
 * This module implements coordinate prediction based on timestamp and velocity
 * to reduce perceived input latency, similar to techniques used by:
 * - Android (VelocityTracker + predictedTouches)
 * - iOS (UIKit predictedTouches)
 * - Chrome (PointerEvent predicted events)
 */

#ifndef LV_INDEV_PREDICT_H
#define LV_INDEV_PREDICT_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "../misc/lv_types.h"
#include "../misc/lv_area.h"

/*********************
 *      DEFINES
 *********************/

/** Number of historical points to keep for velocity calculation */
#define LV_INDEV_PREDICT_HIST_SIZE  6

/** Default prediction time in milliseconds (look-ahead time) */
#define LV_INDEV_PREDICT_TIME_MS    6

/** Maximum prediction distance in pixels to prevent over-prediction */
#define LV_INDEV_PREDICT_MAX_DIST   30

/** Velocity decay factor per millisecond (256 = 100%, no decay) */
#define LV_INDEV_PREDICT_DECAY      240

/**********************
 *      TYPEDEFS
 **********************/

/**
 * Historical point data for velocity calculation
 */
typedef struct {
    lv_point_t point;       /**< Position at this timestamp */
    uint32_t timestamp;     /**< Timestamp in ms */
} lv_indev_predict_point_t;

/**
 * Prediction context - stores history and calculated velocity
 */
typedef struct {
    lv_indev_predict_point_t hist[LV_INDEV_PREDICT_HIST_SIZE]; /**< History ring buffer */
    uint8_t hist_index;     /**< Current index in history buffer */
    uint8_t hist_count;     /**< Number of valid entries in history */

    lv_point_t velocity;    /**< Calculated velocity in pixels per second (scaled by 256) */
    lv_point_t last_predicted; /**< Last predicted point */
    uint32_t last_predict_time; /**< Timestamp of last prediction */

    bool enabled;           /**< Enable/disable prediction */
    int32_t predict_time_ms; /**< Look-ahead time for prediction */
    int32_t max_distance;   /**< Maximum prediction distance */
} lv_indev_predict_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Initialize the prediction context
 * @param ctx pointer to prediction context
 */
void lv_indev_predict_init(lv_indev_predict_t * ctx);

/**
 * Reset the prediction context (clear history)
 * @param ctx pointer to prediction context
 */
void lv_indev_predict_reset(lv_indev_predict_t * ctx);

/**
 * Enable or disable coordinate prediction
 * @param ctx pointer to prediction context
 * @param en true to enable, false to disable
 */
void lv_indev_predict_enable(lv_indev_predict_t * ctx, bool en);

/**
 * Set the prediction look-ahead time
 * @param ctx pointer to prediction context
 * @param time_ms prediction time in milliseconds
 */
void lv_indev_predict_set_time(lv_indev_predict_t * ctx, int32_t time_ms);

/**
 * Set the maximum prediction distance
 * @param ctx pointer to prediction context
 * @param max_dist maximum distance in pixels
 */
void lv_indev_predict_set_max_distance(lv_indev_predict_t * ctx, int32_t max_dist);

/**
 * Add a new point to the history and update velocity
 * @param ctx pointer to prediction context
 * @param point current input point
 * @param timestamp timestamp of the input event (from driver or lv_tick_get())
 */
void lv_indev_predict_add_point(lv_indev_predict_t * ctx, const lv_point_t * point, uint32_t timestamp);

/**
 * Get the predicted point based on current velocity and time
 * @param ctx pointer to prediction context
 * @param target_time target timestamp for prediction (usually current time + offset)
 * @param predicted_point output: the predicted point
 * @return true if prediction was made, false if not enough data or disabled
 */
bool lv_indev_predict_get_point(lv_indev_predict_t * ctx, uint32_t target_time, lv_point_t * predicted_point);

/**
 * Calculate velocity from recent history
 * @param ctx pointer to prediction context
 * @return true if velocity was calculated, false if not enough history
 */
bool lv_indev_predict_calc_velocity(lv_indev_predict_t * ctx);

/**
 * Get the current calculated velocity
 * @param ctx pointer to prediction context
 * @param velocity output: velocity in pixels per second (scaled by 256)
 */
void lv_indev_predict_get_velocity(const lv_indev_predict_t * ctx, lv_point_t * velocity);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_INDEV_PREDICT_H*/
