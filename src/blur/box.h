#pragma once

#include <math.h>
#include <obs-module.h>
#include "../obs-utils.h"
#include "../obs-composite-blur-filter.h"

extern void set_box_blur_types(obs_properties_t *props);
extern void box_setup_callbacks(composite_blur_filter_data_t *data);
extern void render_video_box(composite_blur_filter_data_t *data);
extern void load_effect_box(composite_blur_filter_data_t *filter);

static void box_area_blur(composite_blur_filter_data_t *data);
static void box_directional_blur(composite_blur_filter_data_t *data);
static void box_zoom_blur(composite_blur_filter_data_t *data);
// static void box_motion_blur(composite_blur_filter_data_t *data);
static void box_tilt_shift_blur(composite_blur_filter_data_t *data);

static void load_1d_box_effect(composite_blur_filter_data_t *filter);
static void load_tiltshift_box_effect(composite_blur_filter_data_t *filter);
static void load_radial_box_effect(composite_blur_filter_data_t *filter);