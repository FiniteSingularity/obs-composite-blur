#pragma once

#include <obs-module.h>
#include <util/base.h>
#include <util/dstr.h>
#include <util/darray.h>
#include <util/platform.h>

#include <stdio.h>

#include "obs-utils.h"

#define ALGO_NONE 0
#define ALGO_NONE_LABEL "None"
#define ALGO_GAUSSIAN 1
#define ALGO_GAUSSIAN_LABEL "CompositeBlurFilter.Algorithm.Gaussian"
#define ALGO_BOX 2
#define ALGO_BOX_LABEL "CompositeBlurFilter.Algorithm.Box"
#define ALGO_KAWASE 3
#define ALGO_KAWASE_LABEL "CompositeBlurFilter.Algorithm.Kawase"

#define TYPE_NONE 0
#define TYPE_NONE_LABEL "None"
#define TYPE_AREA 1
#define TYPE_AREA_LABEL "CompositeBlurFilter.Type.Area"
#define TYPE_DIRECTIONAL 2
#define TYPE_DIRECTIONAL_LABEL "CompositeBlurFilter.Type.Directional"
#define TYPE_ZOOM 3
#define TYPE_ZOOM_LABEL "CompositeBlurFilter.Type.Zoom"
#define TYPE_MOTION 4
#define TYPE_MOTION_LABEL "CompositeBlurFilter.Type.Motion"
#define TYPE_TILTSHIFT 5
#define TYPE_TILTSHIFT_LABEL "CompositeBlurFilter.Type.TiltShift"

typedef DARRAY(float) fDarray;

struct composite_blur_filter_data;
typedef struct composite_blur_filter_data composite_blur_filter_data_t;

struct composite_blur_filter_data {
	obs_source_t *context;

	// Effects
	gs_effect_t *effect;
	gs_effect_t *composite_effect;

	// Render pipeline
	bool input_rendered;
	gs_texrender_t *input_texrender;
	bool output_rendered;
	gs_texrender_t *output_texrender;

	gs_texrender_t *render;
	gs_texrender_t *render2;
	gs_texrender_t *composite_render;

	gs_eparam_t *param_uv_size;
	gs_eparam_t *param_dir;
	gs_eparam_t *param_radius;
	gs_eparam_t *param_background;

	bool rendering;
	bool reload;

	struct vec2 uv_size;

	float center_x;
	float center_y;

	float radius;
	float radius_last;
	float angle;
	float tilt_shift_center;
	float tilt_shift_width;
	float tilt_shift_angle;
	int blur_algorithm;
	int blur_algorithm_last;
	int blur_type;
	int blur_type_last;
	int passes;
	obs_weak_source_t *background;
	uint32_t width;
	uint32_t height;

	// Gaussian Kernel
	fDarray kernel;
	fDarray offset;
	gs_texture_t *kernel_texture;
	size_t kernel_size;

	// Callback Functions
	void (*video_render)(composite_blur_filter_data_t *filter);
	void (*load_effect)(composite_blur_filter_data_t *filter);
	void (*update)(composite_blur_filter_data_t *filter);
};

static const char *composite_blur_name(void *type_data);
static void *composite_blur_create(obs_data_t *settings, obs_source_t *source);
static void composite_blur_destroy(void *data);
static uint32_t composite_blur_width(void *data);
static uint32_t composite_blur_height(void *data);
static void composite_blur_update(void *data, obs_data_t *settings);
static void composite_blur_video_render(void *data, gs_effect_t *effect);
static void composite_blur_video_tick(void *data, float seconds);
static obs_properties_t *composite_blur_properties(void *data);
static void composite_blur_reload_effect(composite_blur_filter_data_t *filter);
static void load_composite_effect(composite_blur_filter_data_t *filter);
extern gs_texture_t *blend_composite(gs_texture_t *texture,
				     composite_blur_filter_data_t *data);

static bool setting_blur_algorithm_modified(void *data, obs_properties_t *props,
					    obs_property_t *p,
					    obs_data_t *settings);

static bool setting_blur_types_modified(void *data, obs_properties_t *props,
					obs_property_t *p,
					obs_data_t *settings);
static void setting_visibility(const char *prop_name, bool visible,
			       obs_properties_t *props);
static bool settings_blur_area(obs_properties_t *props);
static bool settings_blur_directional(obs_properties_t *props);
static bool settings_blur_zoom(obs_properties_t *props);
static bool settings_blur_tilt_shift(obs_properties_t *props);