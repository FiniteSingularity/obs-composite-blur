#pragma once

#include <obs-module.h>
#include "../obs-utils.h"
#include "../obs-composite-blur-filter.h"

extern void temporal_setup_callbacks(composite_blur_filter_data_t* data);
extern void render_video_temporal(composite_blur_filter_data_t* data);
extern void load_effect_temporal(composite_blur_filter_data_t* filter);

static void temporal_blur(composite_blur_filter_data_t* data);

static void load_temporal_effect(composite_blur_filter_data_t* filter);
