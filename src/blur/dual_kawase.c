#include "dual_kawase.h"

void set_dual_kawase_blur_types(obs_properties_t *props)
{
	obs_property_t *p = obs_properties_get(props, "blur_type");
	obs_property_list_clear(p);
	obs_property_list_add_int(p, obs_module_text(TYPE_AREA_LABEL),
				  TYPE_AREA);
}

void dual_kawase_setup_callbacks(composite_blur_filter_data_t *data)
{
	data->video_render = render_video_dual_kawase;
	data->load_effect = load_effect_dual_kawase;
	data->update = NULL;
}

void render_video_dual_kawase(composite_blur_filter_data_t *data)
{
	dual_kawase_blur(data);
}

void load_effect_dual_kawase(composite_blur_filter_data_t *filter)
{
	load_dual_kawase_down_sample_effect(filter);
	load_dual_kawase_up_sample_effect(filter);
}

static void dual_kawase_blur(composite_blur_filter_data_t *data)
{
	gs_effect_t *effect_down = data->effect;
	gs_effect_t *effect_up = data->effect_2;
	gs_texture_t *texture = gs_texrender_get_texture(data->input_texrender);

	if (!effect || !texture) {
		return;
	}

	texture = blend_composite(texture, data);

	// gs_eparam_t *image = gs_effect_get_param_by_name(effect, "image");
	// gs_effect_set_texture(image, texture);

	// const float radius = (float)fmax((float)data->radius, 0.1f);
	// gs_eparam_t *radius_param =
	// 	gs_effect_get_param_by_name(effect, "pixel_size");
	// gs_effect_set_float(radius_param, radius);

	// gs_eparam_t *uv_size = gs_effect_get_param_by_name(effect, "uv_size");
	// struct vec2 size;
	// size.x = (float)data->width;
	// size.y = (float)data->height;
	// gs_effect_set_vec2(uv_size, &size);

	// data->output_texrender =
	// 	create_or_reset_texrender(data->output_texrender);

	// set_blending_parameters();

	// if (gs_texrender_begin(data->output_texrender, data->width,
	// 		       data->height)) {
	// 	while (gs_effect_loop(effect, "Draw"))
	// 		gs_draw_sprite(texture, 0, data->width, data->height);
	// 	gs_texrender_end(data->output_texrender);
	// }

	// gs_blend_state_pop();
}

static void
load_dual_kawase_down_sample_effect(composite_blur_filter_data_t *filter)
{
	const char *effect_file_path =
		"/shaders/dual_kawase_down_sample.effect";
	filter->effect_2 =
		load_shader_effect(filter->effect_2, effect_file_path);
	if (filter->effect_2) {
		size_t effect_count =
			gs_effect_get_num_params(filter->effect_2);
		for (size_t effect_index = 0; effect_index < effect_count;
		     effect_index++) {
			gs_eparam_t *param = gs_effect_get_param_by_idx(
				filter->effect_2, effect_index);
			struct gs_effect_param_info info;
			gs_effect_get_param_info(param, &info);
			if (strcmp(info.name, "uv_size") == 0) {
				filter->param_uv_size = param;
			} else if (strcmp(info.name, "dir") == 0) {
				filter->param_dir = param;
			}
		}
	}
}

static void
load_dual_kawase_up_sample_effect(composite_blur_filter_data_t *filter)
{
	const char *effect_file_path = "/shaders/dual_kawase_up_sample.effect";
	filter->effect = load_shader_effect(filter->effect, effect_file_path);
	if (filter->effect) {
		size_t effect_count = gs_effect_get_num_params(filter->effect);
		for (size_t effect_index = 0; effect_index < effect_count;
		     effect_index++) {
			gs_eparam_t *param = gs_effect_get_param_by_idx(
				filter->effect, effect_index);
			struct gs_effect_param_info info;
			gs_effect_get_param_info(param, &info);
			if (strcmp(info.name, "uv_size") == 0) {
				filter->param_uv_size = param;
			} else if (strcmp(info.name, "dir") == 0) {
				filter->param_dir = param;
			}
		}
	}
}
