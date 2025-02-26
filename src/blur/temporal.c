#include "temporal.h"

void temporal_setup_callbacks(composite_blur_filter_data_t* data)
{
	data->video_render = render_video_temporal;
	data->load_effect = load_effect_temporal;
	data->update = NULL;
}

void render_video_temporal(composite_blur_filter_data_t* data)
{
	temporal_blur(data);
}

void load_effect_temporal(composite_blur_filter_data_t* filter)
{
	load_temporal_effect(filter);
}

static void temporal_blur(composite_blur_filter_data_t* data)
{
	gs_effect_t* effect = data->effect;
	gs_texture_t* texture = gs_texrender_get_texture(data->input_texrender);

	if (!effect || !texture) {
		return;
	}

	texture = blend_composite(texture, data);

	if (data->param_temporal_image) {
		gs_effect_set_texture(data->param_temporal_image, texture);
	}

	if (data->param_temporal_current_weight) {
		gs_effect_set_float(data->param_temporal_current_weight, data->temporal_current_weight);
	}

	if (data->param_temporal_clear_threshold) {
		gs_effect_set_float(data->param_temporal_clear_threshold, data->temporal_clear_threshold);
	}

	gs_texture_t* prior_texture;

	if (!data->temporal_prior_stored) {
		prior_texture = texture;
		data->temporal_prior_stored = true;
	} else {
		gs_texrender_t* tmp = data->render;
		data->render = data->output_texrender;
		data->output_texrender = tmp;
		prior_texture = gs_texrender_get_texture(data->render);
	}

	if (data->param_temporal_prior_image) {
		gs_effect_set_texture(data->param_temporal_prior_image, prior_texture);
	}

	data->output_texrender = create_or_reset_texrender(data->output_texrender);

	set_blending_parameters();

	if (gs_texrender_begin(data->output_texrender, data->width,
		data->height)) {
		gs_ortho(0.0f, (float)data->width, 0.0f, (float)data->height,
			-100.0f, 100.0f);
		while (gs_effect_loop(effect, "Draw"))
			gs_draw_sprite(texture, 0, data->width, data->height);
		gs_texrender_end(data->output_texrender);
	}

	gs_blend_state_pop();
}

static void load_temporal_effect(composite_blur_filter_data_t* filter)
{
	if (filter->effect != NULL) {
		obs_enter_graphics();
		gs_effect_destroy(filter->effect);
		filter->effect = NULL;
		obs_leave_graphics();
	}

	const char* effect_file_path = "/shaders/temporal_blur.effect";
	filter->effect =
		load_shader_effect(filter->effect, effect_file_path);
	if (filter->effect) {
		size_t effect_count =
			gs_effect_get_num_params(filter->effect);
		for (size_t effect_index = 0; effect_index < effect_count;
			effect_index++) {
			gs_eparam_t* param = gs_effect_get_param_by_idx(
				filter->effect, effect_index);
			struct gs_effect_param_info info;
			gs_effect_get_param_info(param, &info);
			if (strcmp(info.name, "image") == 0) {
				filter->param_temporal_image = param;
			} else if (strcmp(info.name, "prior_image") == 0) {
				filter->param_temporal_prior_image = param;
			} else if (strcmp(info.name, "current_weight") == 0) {
				filter->param_temporal_current_weight = param;
			} else if (strcmp(info.name, "clear_threshold") == 0) {
				filter->param_temporal_clear_threshold = param;
			}
		}
	}
}
