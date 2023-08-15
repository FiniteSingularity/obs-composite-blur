#pragma once

#include <math.h>
#include <obs-module.h>
#include <obs-composite-blur-filter.h>

extern void render_video_gaussian(struct composite_blur_filter_data *data);
static void gaussian_area_blur(struct composite_blur_filter_data *data);
static void gaussian_directional_blur(struct composite_blur_filter_data *data);