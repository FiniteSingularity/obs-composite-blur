#pragma once

#include <math.h>
#include <obs-module.h>
#include <obs-utils.h>
#include <obs-composite-blur-filter.h>
#include "gaussian-kernel.h"

extern void set_gaussian_blur_types(obs_properties_t *props);
extern void gaussian_setup_callbacks(struct composite_blur_filter_data *data);
extern void render_video_gaussian(struct composite_blur_filter_data *data);
extern void load_effect_gaussian(struct composite_blur_filter_data *filter);
extern void update_gaussian(struct composite_blur_filter_data *data);

static void gaussian_area_blur(struct composite_blur_filter_data *data);
static void gaussian_directional_blur(struct composite_blur_filter_data *data);
static void gaussian_zoom_blur(struct composite_blur_filter_data *data);
static void gaussian_motion_blur(struct composite_blur_filter_data *data);

static void load_1d_gaussian_effect(struct composite_blur_filter_data *filter);
static void
load_motion_gaussian_effect(struct composite_blur_filter_data *filter);
static void
load_radial_gaussian_effect(struct composite_blur_filter_data *filter);
static void sample_kernel(float radius,
			  struct composite_blur_filter_data *filter);