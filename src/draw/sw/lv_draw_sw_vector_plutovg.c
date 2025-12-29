/**
 * @file lv_draw_sw_vector_plutovg.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "../lv_image_decoder_private.h"
#include "../lv_draw_vector_private.h"
#include "../lv_draw_private.h"
#include "lv_draw_sw.h"

#if LV_USE_VECTOR_GRAPHIC && LV_USE_PLUTOVG

#include "../../libs/plutovg/plutovg.h"
#include "../../stdlib/lv_string.h"
#include "blend/lv_draw_sw_blend_private.h"
#include "blend/lv_draw_sw_blend_to_rgb565.h"
#include "blend/lv_draw_sw_blend_to_rgb888.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    plutovg_canvas_t * canvas;
    plutovg_surface_t * surface;
    int32_t translate_x;
    int32_t translate_y;
    lv_opa_t opa;
} _plutovg_draw_state;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void _task_draw_cb(void * ctx, const lv_vector_path_t * path, const lv_vector_path_ctx_t * dsc);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

static void lv_color_to_plutovg(plutovg_color_t * color, const lv_color32_t * c, lv_opa_t opa)
{
    plutovg_color_init_rgba8(color, c->red, c->green, c->blue, LV_OPA_MIX2(c->alpha, opa));
}

static void lv_matrix_to_plutovg(plutovg_matrix_t * pm, const lv_matrix_t * m)
{
    pm->a = m->m[0][0];
    pm->b = m->m[1][0];
    pm->c = m->m[0][1];
    pm->d = m->m[1][1];
    pm->e = m->m[0][2];
    pm->f = m->m[1][2];
}

static void _set_canvas_matrix(plutovg_canvas_t * canvas, const lv_matrix_t * m)
{
    plutovg_matrix_t pm;
    lv_matrix_to_plutovg(&pm, m);
    plutovg_canvas_set_matrix(canvas, &pm);
}

static void _add_path_to_canvas(plutovg_canvas_t * canvas, const lv_vector_path_t * p)
{
    uint32_t pidx = 0;
    lv_vector_path_op_t * op = lv_array_front(&p->ops);
    uint32_t size = lv_array_size(&p->ops);

    plutovg_canvas_new_path(canvas);

    for(uint32_t i = 0; i < size; i++) {
        switch(op[i]) {
            case LV_VECTOR_PATH_OP_MOVE_TO: {
                    lv_fpoint_t * pt = lv_array_at(&p->points, pidx);
                    plutovg_canvas_move_to(canvas, pt->x, pt->y);
                    pidx += 1;
                }
                break;
            case LV_VECTOR_PATH_OP_LINE_TO: {
                    lv_fpoint_t * pt = lv_array_at(&p->points, pidx);
                    plutovg_canvas_line_to(canvas, pt->x, pt->y);
                    pidx += 1;
                }
                break;
            case LV_VECTOR_PATH_OP_QUAD_TO: {
                    lv_fpoint_t * pt1 = lv_array_at(&p->points, pidx);
                    lv_fpoint_t * pt2 = lv_array_at(&p->points, pidx + 1);
                    plutovg_canvas_quad_to(canvas, pt1->x, pt1->y, pt2->x, pt2->y);
                    pidx += 2;
                }
                break;
            case LV_VECTOR_PATH_OP_CUBIC_TO: {
                    lv_fpoint_t * pt1 = lv_array_at(&p->points, pidx);
                    lv_fpoint_t * pt2 = lv_array_at(&p->points, pidx + 1);
                    lv_fpoint_t * pt3 = lv_array_at(&p->points, pidx + 2);
                    plutovg_canvas_cubic_to(canvas, pt1->x, pt1->y, pt2->x, pt2->y, pt3->x, pt3->y);
                    pidx += 3;
                }
                break;
            case LV_VECTOR_PATH_OP_CLOSE: {
                    plutovg_canvas_close_path(canvas);
                }
                break;
        }
    }
}

static plutovg_line_cap_t lv_stroke_cap_to_plutovg(lv_vector_stroke_cap_t cap)
{
    switch(cap) {
        case LV_VECTOR_STROKE_CAP_SQUARE:
            return PLUTOVG_LINE_CAP_SQUARE;
        case LV_VECTOR_STROKE_CAP_ROUND:
            return PLUTOVG_LINE_CAP_ROUND;
        case LV_VECTOR_STROKE_CAP_BUTT:
            return PLUTOVG_LINE_CAP_BUTT;
        default:
            return PLUTOVG_LINE_CAP_SQUARE;
    }
}

static plutovg_line_join_t lv_stroke_join_to_plutovg(lv_vector_stroke_join_t join)
{
    switch(join) {
        case LV_VECTOR_STROKE_JOIN_BEVEL:
            return PLUTOVG_LINE_JOIN_BEVEL;
        case LV_VECTOR_STROKE_JOIN_ROUND:
            return PLUTOVG_LINE_JOIN_ROUND;
        case LV_VECTOR_STROKE_JOIN_MITER:
            return PLUTOVG_LINE_JOIN_MITER;
        default:
            return PLUTOVG_LINE_JOIN_BEVEL;
    }
}

static plutovg_spread_method_t lv_spread_to_plutovg(lv_vector_gradient_spread_t sp)
{
    switch(sp) {
        case LV_VECTOR_GRADIENT_SPREAD_PAD:
            return PLUTOVG_SPREAD_METHOD_PAD;
        case LV_VECTOR_GRADIENT_SPREAD_REPEAT:
            return PLUTOVG_SPREAD_METHOD_REPEAT;
        case LV_VECTOR_GRADIENT_SPREAD_REFLECT:
            return PLUTOVG_SPREAD_METHOD_REFLECT;
        default:
            return PLUTOVG_SPREAD_METHOD_PAD;
    }
}

static plutovg_fill_rule_t lv_fill_rule_to_plutovg(lv_vector_fill_t rule)
{
    switch(rule) {
        case LV_VECTOR_FILL_NONZERO:
            return PLUTOVG_FILL_RULE_NON_ZERO;
        case LV_VECTOR_FILL_EVENODD:
            return PLUTOVG_FILL_RULE_EVEN_ODD;
        default:
            return PLUTOVG_FILL_RULE_NON_ZERO;
    }
}

static plutovg_gradient_stop_t * _create_gradient_stops(const lv_vector_gradient_t * grad, lv_opa_t opa)
{
    plutovg_gradient_stop_t * stops = lv_malloc(sizeof(plutovg_gradient_stop_t) * grad->stops_count);
    LV_ASSERT_MALLOC(stops);

    for(uint16_t i = 0; i < grad->stops_count; i++) {
        const lv_grad_stop_t * s = &(grad->stops[i]);
        stops[i].offset = s->frac / 255.0f;
        plutovg_color_init_rgba8(&stops[i].color, s->color.red, s->color.green, s->color.blue, LV_OPA_MIX2(s->opa, opa));
    }

    return stops;
}

static void _set_paint_stroke_gradient(plutovg_canvas_t * canvas, const lv_vector_gradient_t * g,
                                       const lv_matrix_t * m, lv_opa_t opa)
{
    plutovg_gradient_stop_t * stops = _create_gradient_stops(g, opa);
    plutovg_matrix_t pm;
    lv_matrix_to_plutovg(&pm, m);

    if(g->style == LV_VECTOR_GRADIENT_STYLE_RADIAL) {
        plutovg_canvas_set_radial_gradient(canvas, g->cx, g->cy, g->cr, g->cx, g->cy, 0,
                                           lv_spread_to_plutovg(g->spread), stops, g->stops_count, &pm);
    }
    else {
        plutovg_canvas_set_linear_gradient(canvas, g->x1, g->y1, g->x2, g->y2,
                                           lv_spread_to_plutovg(g->spread), stops, g->stops_count, &pm);
    }

    lv_free(stops);
}

static void _set_paint_stroke(plutovg_canvas_t * canvas, const lv_vector_stroke_dsc_t * dsc, lv_opa_t opa)
{
    if(dsc->style == LV_VECTOR_DRAW_STYLE_SOLID) {
        plutovg_color_t c;
        lv_color_to_plutovg(&c, &dsc->color, LV_OPA_MIX2(dsc->opa, opa));
        plutovg_canvas_set_color(canvas, &c);
    }
    else {   /*gradient*/
        _set_paint_stroke_gradient(canvas, &dsc->gradient, &dsc->matrix, opa);
    }

    plutovg_canvas_set_line_width(canvas, dsc->width);
    plutovg_canvas_set_miter_limit(canvas, (float)dsc->miter_limit);
    plutovg_canvas_set_line_cap(canvas, lv_stroke_cap_to_plutovg(dsc->cap));
    plutovg_canvas_set_line_join(canvas, lv_stroke_join_to_plutovg(dsc->join));

    if(!lv_array_is_empty(&dsc->dash_pattern)) {
        float * dash_array = lv_array_front(&dsc->dash_pattern);
        plutovg_canvas_set_dash_array(canvas, dash_array, dsc->dash_pattern.size);
    }
    else {
        plutovg_canvas_set_dash_array(canvas, NULL, 0);
    }
}

static void _set_paint_fill_gradient(plutovg_canvas_t * canvas, const lv_vector_gradient_t * g,
                                     const lv_matrix_t * m, lv_opa_t opa)
{
    plutovg_gradient_stop_t * stops = _create_gradient_stops(g, opa);
    plutovg_matrix_t pm;
    lv_matrix_to_plutovg(&pm, m);

    if(g->style == LV_VECTOR_GRADIENT_STYLE_RADIAL) {
        plutovg_canvas_set_radial_gradient(canvas, g->cx, g->cy, g->cr, g->cx, g->cy, 0,
                                           lv_spread_to_plutovg(g->spread), stops, g->stops_count, &pm);
    }
    else {
        plutovg_canvas_set_linear_gradient(canvas, g->x1, g->y1, g->x2, g->y2,
                                           lv_spread_to_plutovg(g->spread), stops, g->stops_count, &pm);
    }

    lv_free(stops);
}

static void _set_paint_fill_pattern(plutovg_canvas_t * canvas, const lv_draw_image_dsc_t * p,
                                    const lv_matrix_t * m, lv_opa_t opa)
{
    lv_image_decoder_dsc_t decoder_dsc;
    lv_image_decoder_args_t args = { 0 };
    args.premultiply = 1;
    lv_result_t res = lv_image_decoder_open(&decoder_dsc, p->src, &args);
    if(res != LV_RESULT_OK) {
        LV_LOG_ERROR("Failed to open image");
        return;
    }

    if(!decoder_dsc.decoded) {
        lv_image_decoder_close(&decoder_dsc);
        LV_LOG_ERROR("Image not ready");
        return;
    }

    const lv_image_header_t * header = &decoder_dsc.decoded->header;
    lv_color_format_t cf = header->cf;

    if(cf != LV_COLOR_FORMAT_ARGB8888) {
        lv_image_decoder_close(&decoder_dsc);
        LV_LOG_ERROR("Not support image format");
        return;
    }

    const uint32_t plutovg_stride = header->w * sizeof(uint32_t);
    if(header->stride != plutovg_stride) {
        LV_LOG_WARN("img_stride != plutovg_stride (%" LV_PRIu32 " != %" LV_PRIu32 "), width = %" LV_PRIu32,
                    (uint32_t)header->stride,
                    plutovg_stride, (uint32_t)header->w);
        lv_result_t result = lv_draw_buf_adjust_stride((lv_draw_buf_t *)decoder_dsc.decoded, plutovg_stride);
        if(result != LV_RESULT_OK) {
            lv_image_decoder_close(&decoder_dsc);
            LV_LOG_ERROR("Failed to adjust stride");
            return;
        }
    }

    plutovg_surface_t * surface = plutovg_surface_create_for_data(
                                      (unsigned char *)decoder_dsc.decoded->data,
                                      header->w, header->h, header->stride);

    plutovg_matrix_t pm;
    lv_matrix_to_plutovg(&pm, m);
    float opacity = LV_OPA_MIX2(p->opa, opa) / 255.0f;
    plutovg_canvas_set_texture(canvas, surface, PLUTOVG_TEXTURE_TYPE_PLAIN, opacity, &pm);

    plutovg_surface_destroy(surface);
    lv_image_decoder_close(&decoder_dsc);
}

static void _set_paint_fill(plutovg_canvas_t * canvas, const lv_vector_fill_dsc_t * dsc,
                            const lv_matrix_t * matrix, lv_opa_t opa)
{
    plutovg_canvas_set_fill_rule(canvas, lv_fill_rule_to_plutovg(dsc->fill_rule));

    if(dsc->style == LV_VECTOR_DRAW_STYLE_SOLID) {
        plutovg_color_t c;
        lv_color_to_plutovg(&c, &dsc->color, LV_OPA_MIX2(dsc->opa, opa));
        plutovg_canvas_set_color(canvas, &c);
    }
    else if(dsc->style == LV_VECTOR_DRAW_STYLE_PATTERN) {
        /* Pattern fill needs special handling - use path as clip */
        lv_matrix_t imx = *matrix;

        if(dsc->fill_units == LV_VECTOR_FILL_UNITS_OBJECT_BOUNDING_BOX) {
            /* Get path bounds for object bounding box mode */
            plutovg_rect_t bounds;
            plutovg_canvas_fill_extents(canvas, &bounds);
            lv_matrix_translate(&imx, bounds.x, bounds.y);
        }

        lv_matrix_multiply(&imx, &dsc->matrix);

        _set_paint_fill_pattern(canvas, &dsc->img_dsc, &imx, opa);
    }
    else if(dsc->style == LV_VECTOR_DRAW_STYLE_GRADIENT) {
        _set_paint_fill_gradient(canvas, &dsc->gradient, &dsc->matrix, opa);
    }
}

static void _blend_draw_buf(lv_draw_buf_t * draw_buf, const lv_area_t * dst_area, const lv_draw_buf_t * new_buf,
                            const lv_area_t * src_area)
{
    lv_draw_sw_blend_image_dsc_t fill_dsc;
    fill_dsc.dest_w = src_area->x2;
    fill_dsc.dest_h = src_area->y2;
    fill_dsc.dest_stride = draw_buf->header.stride;
    fill_dsc.dest_buf = draw_buf->data;

    fill_dsc.opa = LV_OPA_100;
    fill_dsc.blend_mode = LV_BLEND_MODE_NORMAL;
    fill_dsc.src_stride = new_buf->header.stride;
    fill_dsc.src_color_format = new_buf->header.cf;
    fill_dsc.src_buf = new_buf->data;

    fill_dsc.mask_buf = NULL;
    fill_dsc.mask_stride = 0;

    fill_dsc.relative_area = *dst_area;
    fill_dsc.src_area = *src_area;

    switch(draw_buf->header.cf) {
#if LV_DRAW_SW_SUPPORT_RGB565
        case LV_COLOR_FORMAT_RGB565:
        case LV_COLOR_FORMAT_RGB565A8:
            lv_draw_sw_blend_image_to_rgb565(&fill_dsc);
            break;
#endif
#if LV_DRAW_SW_SUPPORT_RGB888
        case LV_COLOR_FORMAT_RGB888:
            lv_draw_sw_blend_image_to_rgb888(&fill_dsc, 3);
            break;
#endif
        default:
            break;
    }
}

static void _task_draw_cb(void * ctx, const lv_vector_path_t * path, const lv_vector_path_ctx_t * dsc)
{
    LV_PROFILER_BEGIN_TAG("plutovg_vector_draw");
    _plutovg_draw_state * state = (_plutovg_draw_state *)ctx;
    plutovg_canvas_t * canvas = state->canvas;

    plutovg_canvas_save(canvas);

    plutovg_rect_t clip_rect = {
        (float)(dsc->scissor_area.x1 + state->translate_x),
        (float)(dsc->scissor_area.y1 + state->translate_y),
        (float)lv_area_get_width(&dsc->scissor_area),
        (float)lv_area_get_height(&dsc->scissor_area)
    };

    if(!path) {  /*clear*/
        plutovg_color_t c;
        lv_color_to_plutovg(&c, &dsc->fill_dsc.color, LV_OPA_MIX2(dsc->fill_dsc.opa, state->opa));

        plutovg_canvas_reset_matrix(canvas);
        plutovg_canvas_set_color(canvas, &c);
        plutovg_canvas_set_opacity(canvas, state->opa / 255.0f);
        plutovg_canvas_fill_rect(canvas, clip_rect.x, clip_rect.y, clip_rect.w, clip_rect.h);
    }
    else {
        /* Set clip region */
        plutovg_canvas_reset_matrix(canvas);
        plutovg_canvas_clip_rect(canvas, clip_rect.x, clip_rect.y, clip_rect.w, clip_rect.h);

        /* Set transform matrix */
        lv_matrix_t matrix;
        lv_matrix_identity(&matrix);
        lv_matrix_translate(&matrix, state->translate_x, state->translate_y);
        lv_matrix_multiply(&matrix, &dsc->matrix);
        _set_canvas_matrix(canvas, &matrix);

        /* Add path to canvas */
        _add_path_to_canvas(canvas, path);

        /* Set opacity */
        plutovg_canvas_set_opacity(canvas, state->opa / 255.0f);

        /* Fill path */
        if(dsc->fill_dsc.opa > LV_OPA_MIN) {
            _set_paint_fill(canvas, &dsc->fill_dsc, &matrix, state->opa);
            plutovg_canvas_fill_preserve(canvas);
        }

        /* Stroke path */
        if(dsc->stroke_dsc.opa > LV_OPA_MIN && dsc->stroke_dsc.width > 0) {
            _set_paint_stroke(canvas, &dsc->stroke_dsc, state->opa);
            plutovg_canvas_stroke(canvas);
        }
        else {
            plutovg_canvas_new_path(canvas);
        }
    }

    plutovg_canvas_restore(canvas);
    LV_PROFILER_END_TAG("plutovg_vector_draw");
}

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_draw_sw_vector(lv_draw_task_t * t, lv_draw_vector_dsc_t * dsc)
{
    if(dsc->task_list == NULL)
        return;

    lv_layer_t * layer = dsc->base.layer;
    lv_draw_buf_t * draw_buf = layer->draw_buf;
    if(draw_buf == NULL)
        return;

    void * buf = draw_buf->data;
    int32_t width = lv_area_get_width(&layer->buf_area);
    int32_t height = lv_area_get_height(&layer->buf_area);
    uint32_t stride = draw_buf->header.stride;

    lv_color_format_t cf = draw_buf->header.cf;

    bool allow_buffer = false;
    lv_draw_buf_t * new_buf = NULL;

    if(cf != LV_COLOR_FORMAT_ARGB8888 &&
       cf != LV_COLOR_FORMAT_XRGB8888) {
        allow_buffer = true;
        new_buf = lv_draw_buf_create(width, height, LV_COLOR_FORMAT_ARGB8888, LV_STRIDE_AUTO);
        lv_draw_buf_clear(new_buf, NULL);
        buf = new_buf->data;
        stride = new_buf->header.stride;
    }

    /* Create plutovg surface and canvas */
    plutovg_surface_t * surface = plutovg_surface_create_for_data(buf, width, height, stride);
    plutovg_canvas_t * canvas = plutovg_canvas_create(surface);

    _plutovg_draw_state state = {
        canvas,
        surface,
        -layer->buf_area.x1,
        -layer->buf_area.y1,
        t->opa
    };

    lv_ll_t * task_list = dsc->task_list;
    lv_vector_for_each_destroy_tasks(task_list, _task_draw_cb, &state);
    dsc->task_list = NULL;

    if(allow_buffer) {
        lv_area_t src_area = {0, 0, width, height};
        _blend_draw_buf(draw_buf, &layer->buf_area, new_buf, &src_area);
        lv_draw_buf_destroy(new_buf);
    }

    plutovg_canvas_destroy(canvas);
    plutovg_surface_destroy(surface);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

#endif /*LV_USE_VECTOR_GRAPHIC && LV_USE_PLUTOVG*/
