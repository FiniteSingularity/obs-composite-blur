#include "box.h"

void set_box_blur_types(obs_properties_t *props)
{
	obs_property_t *p = obs_properties_get(props, "blur_type");
	obs_property_list_clear(p);
	obs_property_list_add_int(p, obs_module_text(TYPE_AREA_LABEL),
				  TYPE_AREA);
	obs_property_list_add_int(p, obs_module_text(TYPE_DIRECTIONAL_LABEL),
				  TYPE_DIRECTIONAL);
	obs_property_list_add_int(p, obs_module_text(TYPE_ZOOM_LABEL),
				  TYPE_ZOOM);
	// obs_property_list_add_int(p, obs_module_text(TYPE_MOTION_LABEL),
	// 			  TYPE_MOTION);
	obs_property_list_add_int(p, obs_module_text(TYPE_TILTSHIFT_LABEL),
				  TYPE_TILTSHIFT);
}

void box_setup_callbacks(composite_blur_filter_data_t *data)
{
	data->video_render = render_video_box;
	data->load_effect = load_effect_box;
	data->update = NULL;
}

void render_video_box(composite_blur_filter_data_t *data)
{
	switch (data->blur_type) {
	case TYPE_AREA:
		box_area_blur(data);
		break;
	case TYPE_DIRECTIONAL:
		box_directional_blur(data);
		break;
	case TYPE_ZOOM:
		box_zoom_blur(data);
		break;
	case TYPE_TILTSHIFT:
		box_tilt_shift_blur(data);
		break;
	}
}

void load_effect_box(composite_blur_filter_data_t *filter)
{
	switch (filter->blur_type) {
	case TYPE_AREA:
		load_1d_box_effect(filter);
		break;
	case TYPE_DIRECTIONAL:
		load_1d_box_effect(filter);
		break;
	case TYPE_ZOOM:
		load_radial_box_effect(filter);
		break;
	case TYPE_TILTSHIFT:
		load_tiltshift_box_effect(filter);
		break;
	}
}

/*
 *  Performs an area blur using the box kernel.  Blur is
 *  equal in both x and y directions.
 */
static void box_area_blur(composite_blur_filter_data_t *data)
{
	gs_effect_t *effect = data->effect;
	gs_texture_t *texture = gs_texrender_get_texture(data->input_texrender);

	if (!effect || !texture) {
		return;
	}

	if (data->radius < MIN_BOX_BLUR_RADIUS) {
		data->output_texrender =
			create_or_reset_texrender(data->output_texrender);
		texrender_set_texture(texture, data->output_texrender);
		return;
	}

	texture = blend_composite(texture, data);
	const int passes = data->passes < 1 ? 1 : data->passes;
	for (int i = 0; i < passes; i++) {
		// 1. First pass- apply 1D blur kernel to horizontal dir.
		data->render2 = create_or_reset_texrender(data->render2);

		gs_eparam_t *image =
			gs_effect_get_param_by_name(effect, "image");
		gs_effect_set_texture(image, texture);

		if (data->param_radius) {
			gs_effect_set_float(data->param_radius, data->radius);
		}

		struct vec2 texel_step;
		texel_step.x = 1.0f / data->width;
		texel_step.y = 0.0f;
		if (data->param_texel_step) {
			gs_effect_set_vec2(data->param_texel_step, &texel_step);
		}

		set_blending_parameters();

		if (gs_texrender_begin(data->render2, data->width,
				       data->height)) {
			gs_ortho(0.0f, (float)data->width, 0.0f,
				 (float)data->height, -100.0f, 100.0f);
			while (gs_effect_loop(effect, "Draw"))
				gs_draw_sprite(texture, 0, data->width,
					       data->height);
			gs_texrender_end(data->render2);
		}

		// 2. Save texture from first pass in variable "texture"
		texture = gs_texrender_get_texture(data->render2);

		// 3. Second Pass- Apply 1D blur kernel vertically.
		image = gs_effect_get_param_by_name(effect, "image");
		gs_effect_set_texture(image, texture);

		texel_step.x = 0.0f;
		texel_step.y = 1.0f / data->height;
		if (data->param_texel_step) {
			gs_effect_set_vec2(data->param_texel_step, &texel_step);
		}

		data->output_texrender =
			create_or_reset_texrender(data->output_texrender);

		if (gs_texrender_begin(data->output_texrender, data->width,
				       data->height)) {
			gs_ortho(0.0f, (float)data->width, 0.0f,
				 (float)data->height, -100.0f, 100.0f);
			while (gs_effect_loop(effect, "Draw"))
				gs_draw_sprite(texture, 0, data->width,
					       data->height);
			gs_texrender_end(data->output_texrender);
		}

		texture = gs_texrender_get_texture(data->output_texrender);
		gs_blend_state_pop();
	}
}

/*
 *  Performs a directional blur using the box kernel.
 */
static void box_directional_blur(composite_blur_filter_data_t *data)
{
	gs_effect_t *effect = data->effect;
	gs_texture_t *texture = gs_texrender_get_texture(data->input_texrender);

	if (!effect || !texture) {
		return;
	}

	if (data->radius < MIN_BOX_BLUR_RADIUS) {
		data->output_texrender =
			create_or_reset_texrender(data->output_texrender);
		texrender_set_texture(texture, data->output_texrender);
		return;
	}

	texture = blend_composite(texture, data);

	for (int i = 0; i < data->passes; i++) {
		gs_texrender_t *tmp = data->render2;
		data->render2 = data->output_texrender;
		data->output_texrender = tmp;

		// 1. Single pass- blur only in one direction
		gs_eparam_t *image =
			gs_effect_get_param_by_name(effect, "image");
		gs_effect_set_texture(image, texture);

		if (data->param_radius) {
			gs_effect_set_float(data->param_radius, data->radius);
		}

		struct vec2 texel_step;
		float rads = -data->angle * ((float)M_PI / 180.0f);
		texel_step.x = (float)cos(rads) / data->width;
		texel_step.y = (float)sin(rads) / data->height;
		if (data->param_texel_step) {
			gs_effect_set_vec2(data->param_texel_step, &texel_step);
		}

		set_blending_parameters();

		data->output_texrender =
			create_or_reset_texrender(data->output_texrender);

		if (gs_texrender_begin(data->output_texrender, data->width,
				       data->height)) {
			gs_ortho(0.0f, (float)data->width, 0.0f,
				 (float)data->height, -100.0f, 100.0f);
			while (gs_effect_loop(effect, "Draw"))
				gs_draw_sprite(texture, 0, data->width,
					       data->height);
			gs_texrender_end(data->output_texrender);
		}
		texture = gs_texrender_get_texture(data->output_texrender);
		gs_blend_state_pop();
	}
}

/*
 *  Performs a zoom blur using the box kernel. Blur for a pixel
 *  is performed in direction of zoom center point.  Blur increases
 *  as pixels move away from center point.
 */
static void box_zoom_blur(composite_blur_filter_data_t *data)
{
	gs_effect_t *effect = data->effect;
	gs_texture_t *texture = gs_texrender_get_texture(data->input_texrender);

	if (!effect || !texture) {
		return;
	}

	if (data->radius < MIN_BOX_BLUR_RADIUS) {
		data->output_texrender =
			create_or_reset_texrender(data->output_texrender);
		texrender_set_texture(texture, data->output_texrender);
		return;
	}

	texture = blend_composite(texture, data);

	for (int i = 0; i < data->passes; i++) {
		gs_texrender_t *tmp = data->render2;
		data->render2 = data->output_texrender;
		data->output_texrender = tmp;

		// 1. Single pass- blur only in one direction
		gs_eparam_t *image =
			gs_effect_get_param_by_name(effect, "image");
		gs_effect_set_texture(image, texture);

		if (data->param_radius) {
			gs_effect_set_float(data->param_radius, data->radius);
		}

		struct vec2 radial_center;
		radial_center.x = data->center_x;
		radial_center.y = data->center_y;
		if (data->param_radial_center) {
			gs_effect_set_vec2(data->param_radial_center,
					   &radial_center);
		}

		struct vec2 uv_size;
		uv_size.x = (float)data->width;
		uv_size.y = (float)data->height;
		if (data->param_uv_size) {
			gs_effect_set_vec2(data->param_uv_size, &uv_size);
		}

		set_blending_parameters();

		data->output_texrender =
			create_or_reset_texrender(data->output_texrender);

		if (gs_texrender_begin(data->output_texrender, data->width,
				       data->height)) {
			gs_ortho(0.0f, (float)data->width, 0.0f,
				 (float)data->height, -100.0f, 100.0f);
			while (gs_effect_loop(effect, "Draw"))
				gs_draw_sprite(texture, 0, data->width,
					       data->height);
			gs_texrender_end(data->output_texrender);
		}
		texture = gs_texrender_get_texture(data->output_texrender);
		gs_blend_state_pop();
	}
}

/*
 *  Performs an area blur using the box kernel. Blur is
 *  equal in both x and y directions.
 */
static void box_tilt_shift_blur(composite_blur_filter_data_t *data)
{
	gs_effect_t *effect = data->effect;
	gs_texture_t *texture = gs_texrender_get_texture(data->input_texrender);

	if (!effect || !texture) {
		return;
	}

	if (data->radius < MIN_BOX_BLUR_RADIUS) {
		data->output_texrender =
			create_or_reset_texrender(data->output_texrender);
		texrender_set_texture(texture, data->output_texrender);
		return;
	}

	texture = blend_composite(texture, data);

	for (int i = 0; i < data->passes; i++) {
		// 1. First pass- apply 1D blur kernel to horizontal dir.
		data->render2 = create_or_reset_texrender(data->render2);

		gs_eparam_t *image =
			gs_effect_get_param_by_name(effect, "image");
		gs_effect_set_texture(image, texture);

		if (data->param_radius) {
			gs_effect_set_float(data->param_radius,
					    (float)data->radius);
		}

		const float focus_center =
			1.0f - (float)data->tilt_shift_center;
		if (data->param_focus_center) {
			gs_effect_set_float(data->param_focus_center,
					    focus_center);
		}

		const float focus_width = (float)data->tilt_shift_width / 2.0f;
		if (data->param_focus_width) {
			gs_effect_set_float(data->param_focus_width,
					    focus_width);
		}

		const float focus_angle =
			(float)data->tilt_shift_angle * (M_PI / 180.0f);
		if (data->param_focus_angle) {
			gs_effect_set_float(data->param_focus_angle,
					    focus_angle);
		}

		struct vec2 texel_step;
		texel_step.x = 1.0f / data->width;
		texel_step.y = 0.0f;
		if (data->param_texel_step) {
			gs_effect_set_vec2(data->param_texel_step, &texel_step);
		}

		struct vec2 size;
		size.x = (float)data->width;
		size.y = (float)data->height;
		if (data->param_uv_size) {
			gs_effect_set_vec2(data->param_uv_size, &size);
		}

		set_blending_parameters();

		if (gs_texrender_begin(data->render2, data->width,
				       data->height)) {
			gs_ortho(0.0f, (float)data->width, 0.0f,
				 (float)data->height, -100.0f, 100.0f);
			while (gs_effect_loop(effect, "Draw"))
				gs_draw_sprite(texture, 0, data->width,
					       data->height);
			gs_texrender_end(data->render2);
		}

		// 2. Save texture from first pass in variable "texture"
		texture = gs_texrender_get_texture(data->render2);

		// 3. Second Pass- Apply 1D blur kernel vertically.
		image = gs_effect_get_param_by_name(effect, "image");
		gs_effect_set_texture(image, texture);

		texel_step.x = 0.0f;
		texel_step.y = 1.0f / data->height;
		if (data->param_texel_step) {
			gs_effect_set_vec2(data->param_texel_step, &texel_step);
		}

		data->output_texrender =
			create_or_reset_texrender(data->output_texrender);

		if (gs_texrender_begin(data->output_texrender, data->width,
				       data->height)) {
			gs_ortho(0.0f, (float)data->width, 0.0f,
				 (float)data->height, -100.0f, 100.0f);
			while (gs_effect_loop(effect, "Draw"))
				gs_draw_sprite(texture, 0, data->width,
					       data->height);
			gs_texrender_end(data->output_texrender);
		}
		texture = gs_texrender_get_texture(data->output_texrender);
		gs_blend_state_pop();
	}
}

static void load_1d_box_effect(composite_blur_filter_data_t *filter)
{
	const char *effect_file_path = "/shaders/box_1d.effect";
	filter->effect = load_shader_effect(filter->effect, effect_file_path);
	if (filter->effect) {
		size_t effect_count = gs_effect_get_num_params(filter->effect);
		for (size_t effect_index = 0; effect_index < effect_count;
		     effect_index++) {
			gs_eparam_t *param = gs_effect_get_param_by_idx(
				filter->effect, effect_index);
			struct gs_effect_param_info info;
			gs_effect_get_param_info(param, &info);
			if (strcmp(info.name, "texel_step") == 0) {
				filter->param_texel_step = param;
			} else if (strcmp(info.name, "radius") == 0) {
				filter->param_radius = param;
			}
		}
	}
}

static void load_tiltshift_box_effect(composite_blur_filter_data_t *filter)
{
	const char *effect_file_path = "/shaders/box_tiltshift.effect";
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
			} else if (strcmp(info.name, "texel_step") == 0) {
				filter->param_texel_step = param;
			} else if (strcmp(info.name, "radius") == 0) {
				filter->param_radius = param;
			} else if (strcmp(info.name, "focus_center") == 0) {
				filter->param_focus_center = param;
			} else if (strcmp(info.name, "focus_width") == 0) {
				filter->param_focus_width = param;
			} else if (strcmp(info.name, "focus_angle") == 0) {
				filter->param_focus_angle = param;
			}
		}
	}
}

static void load_radial_box_effect(composite_blur_filter_data_t *filter)
{
	const char *effect_file_path = "/shaders/box_radial.effect";
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
			} else if (strcmp(info.name, "radius") == 0) {
				filter->param_radius = param;
			} else if (strcmp(info.name, "radial_center") == 0) {
				filter->param_radial_center = param;
			}
		}
	}
}
