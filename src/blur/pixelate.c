#include "pixelate.h"

void set_pixelate_blur_types(obs_properties_t *props)
{
	obs_property_t *p = obs_properties_get(props, "blur_type");
	obs_property_list_clear(p);
	obs_property_list_add_int(p, obs_module_text(TYPE_AREA_LABEL),
				  TYPE_AREA);
}

void pixelate_setup_callbacks(composite_blur_filter_data_t *data)
{
	data->video_render = render_video_pixelate;
	data->load_effect = load_effect_pixelate;
	data->update = NULL;
}

void render_video_pixelate(composite_blur_filter_data_t *data)
{
	pixelate_square_blur(data);
}

void load_effect_pixelate(composite_blur_filter_data_t *filter)
{
	switch (filter->pixelate_type) {
	case PIXELATE_TYPE_SQUARE:
		load_pixelate_square_effect(filter);
		break;
	case PIXELATE_TYPE_HEXAGONAL:
		load_pixelate_hexagonal_effect(filter);
		break;
	case PIXELATE_TYPE_CIRCLE:
		load_pixelate_circle_effect(filter);
		break;
	case PIXELATE_TYPE_TRIANGLE:
		load_pixelate_triangle_effect(filter);
		break;
	}
}

static void pixelate_square_blur(composite_blur_filter_data_t *data)
{
	gs_effect_t *effect = data->effect;
	gs_texture_t *texture = gs_texrender_get_texture(data->input_texrender);

	if (!effect || !texture) {
		return;
	}

	texture = blend_composite(texture, data);

	gs_eparam_t *image = gs_effect_get_param_by_name(effect, "image");
	gs_effect_set_texture(image, texture);

	const float radius = (float)fmax((float)data->radius, 1.0f);
	if(data->param_pixel_size) {
		gs_effect_set_float(data->param_pixel_size, radius);
	}

	struct vec2 uv_size;
	uv_size.x = (float)data->width;
	uv_size.y = (float)data->height;
	if(data->param_uv_size) {
		gs_effect_set_vec2(data->param_uv_size, &uv_size);
	}

	data->output_texrender =
		create_or_reset_texrender(data->output_texrender);

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

static void load_pixelate_square_effect(composite_blur_filter_data_t *filter)
{
	const char *effect_file_path = "/shaders/pixelate_square.effect";
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
			} else if (strcmp(info.name, "pixel_size") == 0) {
				filter->param_pixel_size = param;
			}
		}
	}
}

static void load_pixelate_hexagonal_effect(composite_blur_filter_data_t *filter)
{
	const char *effect_file_path = "/shaders/pixelate_hexagonal.effect";
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
			} else if (strcmp(info.name, "pixel_size") == 0) {
				filter->param_pixel_size = param;
			}
		}
	}
}

static void load_pixelate_circle_effect(composite_blur_filter_data_t *filter)
{
	const char *effect_file_path = "/shaders/pixelate_circle.effect";
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
			} else if (strcmp(info.name, "pixel_size") == 0) {
				filter->param_pixel_size = param;
			}
		}
	}
}

static void load_pixelate_triangle_effect(composite_blur_filter_data_t *filter)
{
	const char *effect_file_path = "/shaders/pixelate_triangle.effect";
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
			} else if (strcmp(info.name, "pixel_size") == 0) {
				filter->param_pixel_size = param;
			}
		}
	}
}
