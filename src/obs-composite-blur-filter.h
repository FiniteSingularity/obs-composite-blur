#include <obs-module.h>
#include <plugin-support.h>
#include <util/base.h>
#include <util/dstr.h>
#include <util/darray.h>
#include <util/platform.h>

#include <stdio.h>
#include "kernel.h"

static const char *composite_blur_name(void *type_data);
static void *composite_blur_create(obs_data_t *settings, obs_source_t *source);
static void composite_blur_destroy(void *data);
static uint32_t composite_blur_width(void *data);
static uint32_t composite_blur_height(void *data);
static void composite_blur_update(void *data, obs_data_t *settings);
static void composite_blur_video_render(void *data, gs_effect_t *effect);
static void composite_blur_video_tick(void *data, float seconds);
static obs_properties_t *composite_blur_properties(void *data);
static char *load_shader_from_file(const char *file_name);
static void
composite_blur_reload_effect(struct composite_blur_filter_data *filter);
static void load_blur_effect(struct composite_blur_filter_data *filter);
static void load_composite_effect(struct composite_blur_filter_data *filter);
