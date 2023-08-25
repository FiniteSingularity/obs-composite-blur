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

gs_texture_t *down_sample(composite_blur_filter_data_t *data,
			  gs_texture_t *input_texture, int divisor, float ratio)
{
	gs_effect_t *effect_down = data->effect_2;
	// Swap renderers
	gs_texrender_t *tmp = data->render;
	data->render = data->render2;
	data->render2 = tmp;

	data->render = create_or_reset_texrender(data->render);

	uint32_t w = data->width / divisor;
	uint32_t h = data->height / divisor;
	gs_eparam_t *image = gs_effect_get_param_by_name(effect_down, "image");
	gs_effect_set_texture(image, input_texture);

	gs_eparam_t *texel_step =
		gs_effect_get_param_by_name(effect_down, "texel_step");
	struct vec2 texel_step_size;

	texel_step_size.x = ratio / (float)w;
	texel_step_size.y = ratio / (float)h;
	gs_effect_set_vec2(texel_step, &texel_step_size);

	if (gs_texrender_begin(data->render, w, h)) {
		gs_ortho(0.0f, (float)w, 0.0f, (float)h, -100.0f, 100.0f);
		while (gs_effect_loop(effect_down, "Draw"))
			gs_draw_sprite(input_texture, 0, w, h);
		gs_texrender_end(data->render);
	}
	return gs_texrender_get_texture(data->render);
}

gs_texture_t *up_sample(composite_blur_filter_data_t *data,
			gs_texture_t *input_texture, int divisor, float ratio)
{
	gs_effect_t *effect_up = data->effect;
	// Swap renderers
	gs_texrender_t *tmp = data->render;
	data->render = data->render2;
	data->render2 = tmp;

	data->render = create_or_reset_texrender(data->render);

	uint32_t start_w = gs_texture_get_width(input_texture);
	uint32_t start_h = gs_texture_get_height(input_texture);

	uint32_t w = data->width / divisor;
	uint32_t h = data->height / divisor;
	gs_eparam_t *image = gs_effect_get_param_by_name(effect_up, "image");
	gs_effect_set_texture(image, input_texture);

	gs_eparam_t *texel_step =
		gs_effect_get_param_by_name(effect_up, "texel_step");
	struct vec2 texel_step_size;
	texel_step_size.x = ratio / (float)start_w;
	texel_step_size.y = ratio / (float)start_h;
	gs_effect_set_vec2(texel_step, &texel_step_size);

	if (gs_texrender_begin(data->render, w, h)) {
		gs_ortho(0.0f, (float)w, 0.0f, (float)h, -100.0f, 100.0f);
		while (gs_effect_loop(effect_up, "Draw"))
			gs_draw_sprite(input_texture, 0, w, h);
		gs_texrender_end(data->render);
	}
	return gs_texrender_get_texture(data->render);
}

gs_texture_t *mix_textures(composite_blur_filter_data_t *data,
			   gs_texture_t *base, gs_texture_t *residual,
			   float ratio)
{
	blog(LOG_INFO, "Mixing Textures here!");
	gs_effect_t *effect = data->mix_effect;
	// Swap renderers
	gs_texrender_t *tmp = data->render;
	data->render = data->render2;
	data->render2 = tmp;

	data->render = create_or_reset_texrender(data->render);

	uint32_t w = gs_texture_get_width(base);
	uint32_t h = gs_texture_get_height(base);
	blog(LOG_INFO, "%i, %i", w, h);

	gs_eparam_t *image = gs_effect_get_param_by_name(effect, "image");
	gs_effect_set_texture(image, base);

	gs_eparam_t *image2 = gs_effect_get_param_by_name(effect, "image2");
	gs_effect_set_texture(image2, residual);

	gs_eparam_t *ratio_param = gs_effect_get_param_by_name(effect, "ratio");
	gs_effect_set_float(ratio_param, ratio);

	if (gs_texrender_begin(data->render, w, h)) {
		gs_ortho(0.0f, (float)w, 0.0f, (float)h, -100.0f, 100.0f);
		while (gs_effect_loop(effect, "Draw"))
			gs_draw_sprite(base, 0, w, h);
		gs_texrender_end(data->render);
	}
	return gs_texrender_get_texture(data->render);
}

static void dual_kawase_blur(composite_blur_filter_data_t *data)
{
	gs_texture_t *texture = gs_texrender_get_texture(data->input_texrender);
	if (data->kawase_passes == 1) {
		gs_texrender_t *tmp = data->input_texrender;
		data->input_texrender = data->output_texrender;
		data->output_texrender = tmp;
		return;
	}
	gs_effect_t *effect_up = data->effect;
	gs_effect_t *effect_down = data->effect_2;
	gs_texrender_t *base_render = NULL;

	if (!effect_down || !effect_up || !texture) {
		return;
	}

	texture = blend_composite(texture, data);
	set_blending_parameters();

	int last_pass = 1;
	// Down Sampling Loop
	for (int i = 2; i <= data->kawase_passes; i *= 2) {
		texture = down_sample(data, texture, i, 1.0);
		last_pass = i;
	}
	int residual = data->kawase_passes - last_pass;
	if (residual > 0) {
		int next_pass = last_pass * 2;
		float ratio = (float)residual / (float)(next_pass - last_pass);

		// Downsample one more step
		texture = down_sample(data, texture, next_pass, 1.0);
		// Extract renderer from end of down sampling loop
		base_render = data->render2;
		data->render2 = NULL;
		// Upsample one more step
		texture = up_sample(data, texture, last_pass, 1.0);
		gs_texture_t *base = gs_texrender_get_texture(base_render);
		// Mix the end of the downsample loop with additional step.
		// Use the residual ratio for mixing.
		texture = mix_textures(data, base, texture, ratio);
	}
	// Upsample Loop
	for (int i = last_pass / 2; i >= 1; i /= 2) {
		texture = up_sample(data, texture, i, 1.0);
	}

	gs_blend_state_pop();

	// Copy resulting frame to output-> texrenderer
	gs_texrender_t *tmp = data->render;
	data->render = data->output_texrender;
	data->output_texrender = tmp;
	// Destroy base_render if used (if there was a residual)
	if (base_render) {
		gs_texrender_destroy(base_render);
	}
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
