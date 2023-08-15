#pragma once

#include <obs-module.h>
#include <plugin-support.h>
#include <util/base.h>
#include <util/dstr.h>
#include <util/darray.h>
#include <util/platform.h>

#include <stdio.h>
#include "kernel.h"

#include "obs-utils.h"
#include "blur/gaussian.h"

typedef DARRAY(float) fDarray;

struct composite_blur_filter_data {
	obs_source_t *context;
	gs_effect_t *effect;
	gs_effect_t *composite_effect;
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

	float radius;
	float angle;
	const char *blur_algorithm;
	const char *blur_type;
	obs_weak_source_t *background;
	uint32_t width;
	uint32_t height;

	fDarray kernel;
	fDarray offset;
	size_t kernel_size;

	void (*video_render)(struct composite_blur_filter_data *filter);
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
static void
composite_blur_reload_effect(struct composite_blur_filter_data *filter);
static void load_blur_effect(struct composite_blur_filter_data *filter);
static void load_composite_effect(struct composite_blur_filter_data *filter);
extern gs_texture_t *blend_composite(gs_texture_t *texture,
				     struct composite_blur_filter_data *data);

static bool setting_blur_types_modified(obs_properties_t *props,
					obs_property_t *p,
					obs_data_t *settings);
static bool settings_blur_area(obs_properties_t *props);
static bool settings_blur_directional(obs_properties_t *props);
static bool settings_blur_zoom(obs_properties_t *props);
