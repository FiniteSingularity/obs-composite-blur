#pragma once
#include <obs-module.h>
#include <util/base.h>
#include <util/dstr.h>
#include <util/darray.h>
#include <util/platform.h>

#include <stdio.h>

extern gs_texrender_t *create_or_reset_texrender(gs_texrender_t *render);
extern void set_blending_parameters();
extern void set_render_parameters();
extern bool add_source_to_list(void *data, obs_source_t *source);
extern char *load_shader_from_file(const char *file_name);