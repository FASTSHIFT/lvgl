/**
 * @file lv_draw_vg_lite_box_shadow.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#include "../../misc/lv_area_private.h"
#include "lv_draw_vg_lite.h"

#if LV_USE_DRAW_VG_LITE

#include "lv_draw_vg_lite_type.h"
#include <math.h>

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_draw_vg_lite_box_shadow(lv_draw_task_t * t, const lv_draw_box_shadow_dsc_t * dsc,
                                const lv_area_t * coords)
{
    LV_PROFILER_DRAW_BEGIN;
    /*Calculate the rectangle which is blurred to get the shadow in `shadow_area`*/
    lv_area_t core_area;
    core_area.x1 = coords->x1  + dsc->ofs_x - dsc->spread;
    core_area.x2 = coords->x2  + dsc->ofs_x + dsc->spread;
    core_area.y1 = coords->y1  + dsc->ofs_y - dsc->spread;
    core_area.y2 = coords->y2  + dsc->ofs_y + dsc->spread;

    /*Calculate the bounding box of the shadow*/
    lv_area_t shadow_area;
    shadow_area.x1 = core_area.x1 - dsc->width / 2 - 1;
    shadow_area.x2 = core_area.x2 + dsc->width / 2 + 1;
    shadow_area.y1 = core_area.y1 - dsc->width / 2 - 1;
    shadow_area.y2 = core_area.y2 + dsc->width / 2 + 1;

    /*Get clipped draw area which is the real draw area.
     *It is always the same or inside `shadow_area`*/
    lv_area_t draw_area;
    if(!lv_area_intersect(&draw_area, &shadow_area, &t->clip_area)) {
        LV_PROFILER_DRAW_END;
        return;
    }

    const int32_t half_w = dsc->width >= 2 ? dsc->width / 2 : 1;

    /* Shadow correction factor, used to optimize the effect */
    const int32_t shadow_factor = (int32_t)log2(half_w);

    /* Use the core area as the draw area，The latter will handle the cropping*/
    draw_area = core_area;
    lv_area_increase(&draw_area, -shadow_factor, -shadow_factor);

    lv_draw_border_dsc_t border_dsc;
    lv_draw_border_dsc_init(&border_dsc);
    border_dsc.color = dsc->color;
    border_dsc.radius = dsc->radius - shadow_factor;

    /* Compensate for first border transparency */
    border_dsc.opa = dsc->opa / 2;
    border_dsc.width = 1;
    lv_draw_vg_lite_border(t, &border_dsc, &draw_area);

    /* The back-end borders overlap each other to simulate a gradient */
    border_dsc.width = 2;

    for(int32_t w = 0; w < half_w; w++) {
        border_dsc.opa = lv_map(w, 0, half_w, dsc->opa / 2, LV_OPA_0);
        border_dsc.radius++;
        lv_area_increase(&draw_area, 1, 1);
        lv_draw_vg_lite_border(t, &border_dsc, &draw_area);
    }

    /* fill center */
    lv_draw_fill_dsc_t fill_dsc;
    lv_draw_fill_dsc_init(&fill_dsc);
    fill_dsc.radius = dsc->radius - 1 - shadow_factor;
    fill_dsc.color = dsc->color;

    /* This will show better results */
    fill_dsc.opa = dsc->opa - lv_map(dsc->opa, LV_OPA_TRANSP, LV_OPA_COVER, 0, shadow_factor * 8);
    lv_area_increase(&core_area, -1 - shadow_factor, -1 - shadow_factor);
    lv_draw_vg_lite_fill(t, &fill_dsc, &core_area);

    LV_PROFILER_DRAW_END;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

#endif /*LV_USE_DRAW_VG_LITE*/
