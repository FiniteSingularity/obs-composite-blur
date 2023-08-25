#pragma once

#include <math.h>
#include <obs-module.h>
#include "../obs-utils.h"
#include "../obs-composite-blur-filter.h"

extern void set_dual_kawase_blur_types(obs_properties_t *props);
extern void dual_kawase_setup_callbacks(composite_blur_filter_data_t *data);
extern void render_video_dual_kawase(composite_blur_filter_data_t *data);
extern void load_effect_dual_kawase(composite_blur_filter_data_t *filter);
static void dual_kawase_blur(composite_blur_filter_data_t *data);
static void
load_dual_kawase_down_sample_effect(composite_blur_filter_data_t *filter);
static void
load_dual_kawase_up_sample_effect(composite_blur_filter_data_t *filter);
static gs_texture_t *mix_textures(composite_blur_filter_data_t *data,
				  gs_texture_t *base, gs_texture_t *residual,
				  float ratio);