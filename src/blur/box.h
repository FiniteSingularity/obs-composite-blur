#pragma once

#include <math.h>
#include <obs-module.h>
#include <obs-utils.h>
#include <obs-composite-blur-filter.h>

extern void set_box_blur_types(obs_properties_t *props);
extern void box_setup_callbacks(struct composite_blur_filter_data *data);
extern void render_video_box(struct composite_blur_filter_data *data);
extern void load_effect_box(struct composite_blur_filter_data *filter);

static void box_area_blur(struct composite_blur_filter_data *data);
static void box_directional_blur(struct composite_blur_filter_data *data);
static void box_zoom_blur(struct composite_blur_filter_data *data);
// static void box_motion_blur(struct composite_blur_filter_data *data);
static void box_tilt_shift_blur(struct composite_blur_filter_data *data);

static void load_1d_box_effect(struct composite_blur_filter_data *filter);
static void
load_tiltshift_box_effect(struct composite_blur_filter_data *filter);
static void load_radial_box_effect(struct composite_blur_filter_data *filter);