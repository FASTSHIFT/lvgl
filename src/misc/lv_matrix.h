/**
 * @file lv_matrix.h
 *
 */

#ifndef LV_MATRIX_H
#define LV_MATRIX_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "../lv_conf_internal.h"

#if LV_USE_MATRIX

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

typedef struct {
    float m[3][3];
} lv_matrix_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Set matrix to identity matrix
 * @param matrix           pointer to a matrix
 */
void lv_matrix_identity(lv_matrix_t * matrix);

/**
 * Translate the matrix to new position
 * @param matrix           pointer to a matrix
 * @param tx               the amount of translate in x direction
 * @param tx               the amount of translate in y direction
 */
void lv_matrix_translate(lv_matrix_t * matrix, float tx, float ty);

/**
 * Change the scale factor of the matrix
 * @param matrix           pointer to a matrix
 * @param scale_x          the scale factor for the X direction
 * @param scale_y          the scale factor for the Y direction
 */
void lv_matrix_scale(lv_matrix_t * matrix, float scale_x, float scale_y);

/**
 * Rotate the matrix with origin
 * @param matrix           pointer to a matrix
 * @param degree           angle to rotate
 */
void lv_matrix_rotate(lv_matrix_t * matrix, float degree);

/**
 * Change the skew factor of the matrix
 * @param matrix           pointer to a matrix
 * @param skew_x           the skew factor for x direction
 * @param skew_y           the skew factor for y direction
 */
void lv_matrix_skew(lv_matrix_t * matrix, float skew_x, float skew_y);

/**
 * Multiply two matrix and store the result to the first one
 * @param matrix           pointer to a matrix
 * @param matrix2          pointer to another matrix
 */
void lv_matrix_multiply(lv_matrix_t * matrix, const lv_matrix_t * matrix2);

/**********************
 *      MACROS
 **********************/

#endif /*LV_USE_MATRIX*/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_MATRIX_H*/