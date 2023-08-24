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
	gs_effect_t *effect_down = data->effect_2;
	gs_effect_t *effect_up = data->effect;
	gs_texture_t *texture = gs_texrender_get_texture(data->input_texrender);

	if (!effect_down || !effect_up || !texture) {
		return;
	}

	texture = blend_composite(texture, data);
	set_blending_parameters();
	set_render_parameters();
	for(int i=1; i<=data->kawase_passes; i++) {
		// Swap renderers
		gs_texrender_t *tmp = data->render;
		data->render = data->render2;
		data->render2 = tmp;

		data->render = create_or_reset_texrender(data->render);

		uint32_t w = data->width >> i;
		uint32_t h = data->height >> i;
		gs_eparam_t *image = gs_effect_get_param_by_name(effect_down, "image");
		gs_effect_set_texture(image, texture);

		gs_eparam_t *texel_step =
			gs_effect_get_param_by_name(effect_down, "texel_step");
		struct vec2 texel_step_size;

		texel_step_size.x = 1.0f / (float)w;
		texel_step_size.y = 1.0f / (float)h;
		gs_effect_set_vec2(texel_step, &texel_step_size);

		if (gs_texrender_begin(data->render, w, h)) {
			gs_ortho(0.0f, w, 0.0f, h, -100.0f, 100.0f);
			while (gs_effect_loop(effect_down, "Draw"))
				gs_draw_sprite(texture, 0, w, h);
			gs_texrender_end(data->render);
		}
		texture = gs_texrender_get_texture(data->render);
	}

	for(int i=data->kawase_passes; i>=1; i--) {
		// Swap renderers
		gs_texrender_t *tmp = data->render;
		data->render = data->render2;
		data->render2 = tmp;

		data->render = create_or_reset_texrender(data->render);

		uint32_t start_w = gs_texture_get_width(texture);
		uint32_t start_h = gs_texture_get_height(texture);

		uint32_t w = data->width >> (i-1);
		uint32_t h = data->height >> (i-1);
		gs_eparam_t *image = gs_effect_get_param_by_name(effect_up, "image");
		gs_effect_set_texture(image, texture);

		gs_eparam_t *texel_step =
			gs_effect_get_param_by_name(effect_up, "texel_step");
		struct vec2 texel_step_size;
		texel_step_size.x = 1.0f / (float)start_w;
		texel_step_size.y = 1.0f / (float)start_h;
		gs_effect_set_vec2(texel_step, &texel_step_size);

		if (gs_texrender_begin(data->render, w, h)) {
			gs_ortho(0.0f, w, 0.0f, h, -100.0f, 100.0f);
			while (gs_effect_loop(effect_up, "Draw"))
				gs_draw_sprite(texture, 0, w, h);
			gs_texrender_end(data->render);
		}
		texture = gs_texrender_get_texture(data->render);
	}

	gs_blend_state_pop();

	gs_texrender_t *tmp2 = data->render;
	data->render = data->output_texrender;
	data->output_texrender = tmp2;
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
