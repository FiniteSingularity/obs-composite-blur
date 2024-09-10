#pragma once

#include <math.h>
#include <obs-module.h>
#include "../obs-utils.h"
#include "../obs-composite-blur-filter.h"
#include "gaussian-kernel.h"

#define MIN_GAUSSIAN_BLUR_RADIUS 0.01f

extern void set_gaussian_blur_types(obs_properties_t *props);
extern void gaussian_setup_callbacks(composite_blur_filter_data_t *data);
extern void render_video_gaussian(composite_blur_filter_data_t *data);
extern void load_effect_gaussian(composite_blur_filter_data_t *filter);
extern void update_gaussian(composite_blur_filter_data_t *data);

static void gaussian_area_blur(composite_blur_filter_data_t *data);
static void gaussian_directional_blur(composite_blur_filter_data_t *data);
static void gaussian_zoom_blur(composite_blur_filter_data_t *data);
static void gaussian_motion_blur(composite_blur_filter_data_t *data);
static void gaussian_vector_blur(composite_blur_filter_data_t* data);
static void gaussian_vector_gradient(composite_blur_filter_data_t* data);
static void gaussian_vector_smooth_gradient(composite_blur_filter_data_t* data);
static void gaussian_vector_apply_blur(composite_blur_filter_data_t* data);

static void load_1d_gaussian_effect(composite_blur_filter_data_t *filter);
static void load_motion_gaussian_effect(composite_blur_filter_data_t *filter);
static void load_radial_gaussian_effect(composite_blur_filter_data_t *filter);
static void load_vector_gaussian_effect(composite_blur_filter_data_t* filter);
static gs_effect_t* load_gradient_shader_effect(gs_effect_t* effect,
	const char* effect_file_path, const char* sample_type);
static void load_gradient_effect(composite_blur_filter_data_t* filter);
static void sample_kernel(float radius, composite_blur_filter_data_t *filter);

extern bool vector_channel_modified(void* data, obs_properties_t* props,
	obs_property_t* p,
	obs_data_t* settings);
