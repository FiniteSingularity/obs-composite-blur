#pragma once

#include <math.h>
#include <obs-module.h>
#include "../obs-utils.h"
#include "../obs-composite-blur-filter.h"

extern void set_pixelate_blur_types(obs_properties_t *props);
extern void pixelate_setup_callbacks(composite_blur_filter_data_t *data);
extern void render_video_pixelate(composite_blur_filter_data_t *data);
extern void load_effect_pixelate(composite_blur_filter_data_t *filter);

static void pixelate_square_blur(composite_blur_filter_data_t *data);

static void load_pixelate_square_effect(composite_blur_filter_data_t *filter);
static void
load_pixelate_hexagonal_effect(composite_blur_filter_data_t *filter);
static void load_pixelate_circle_effect(composite_blur_filter_data_t *filter);
