#include "gaussian.h"
#include "dual_kawase.h"

void set_gaussian_blur_types(obs_properties_t *props)
{
	obs_property_t *p = obs_properties_get(props, "blur_type");
	obs_property_list_clear(p);
	obs_property_list_add_int(p, obs_module_text(TYPE_AREA_LABEL),
				  TYPE_AREA);
	obs_property_list_add_int(p, obs_module_text(TYPE_DIRECTIONAL_LABEL),
				  TYPE_DIRECTIONAL);
	obs_property_list_add_int(p, obs_module_text(TYPE_ZOOM_LABEL),
				  TYPE_ZOOM);
	obs_property_list_add_int(p, obs_module_text(TYPE_MOTION_LABEL),
				  TYPE_MOTION);
	obs_property_list_add_int(p, obs_module_text(TYPE_VECTOR_LABEL),
				  TYPE_VECTOR);
}

void gaussian_setup_callbacks(composite_blur_filter_data_t *data)
{
	data->video_render = render_video_gaussian;
	data->load_effect = load_effect_gaussian;
	data->update = update_gaussian;
}

void update_gaussian(composite_blur_filter_data_t *data)
{
	if (data->radius != data->radius_last) {
		data->radius_last = data->radius;
		sample_kernel(data->radius, data);
	}
	if (data->vector_blur_amount != data->last_vector_blur_amount) {
		data->last_vector_blur_amount = data->vector_blur_amount;
		float blur_radius = fabsf(data->vector_blur_amount);
		sample_kernel(blur_radius, data);
	}
}

void render_video_gaussian(composite_blur_filter_data_t *data)
{
	switch (data->blur_type) {
	case TYPE_AREA:
		gaussian_area_blur(data);
		break;
	case TYPE_DIRECTIONAL:
		gaussian_directional_blur(data);
		break;
	case TYPE_ZOOM:
		gaussian_zoom_blur(data);
		break;
	case TYPE_MOTION:
		gaussian_motion_blur(data);
		break;
	case TYPE_VECTOR:
		gaussian_vector_blur(data);
		break;
	}
}

void load_effect_gaussian(composite_blur_filter_data_t *filter)
{
	switch (filter->blur_type) {
	case TYPE_AREA:
		load_1d_gaussian_effect(filter);
		break;
	case TYPE_DIRECTIONAL:
		load_1d_gaussian_effect(filter);
		break;
	case TYPE_ZOOM:
		load_radial_gaussian_effect(filter);
		break;
	case TYPE_MOTION:
		load_motion_gaussian_effect(filter);
		break;
	case TYPE_VECTOR:
		load_vector_gaussian_effect(filter);
		load_gradient_effect(filter);
		load_effect_dual_kawase(filter);
		break;
	}
}

extern bool vector_channel_modified(void* data, obs_properties_t* props,
	obs_property_t* p,
	obs_data_t* settings)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(p);
	UNUSED_PARAMETER(settings);
	int channel = (int)obs_data_get_int(settings, "vector_blur_channel");
	composite_blur_filter_data_t* filter = data;
	filter->vector_blur_channel = channel;
	load_gradient_effect(filter);
	return false;
}

/*
 *  Performs an area blur using the gaussian kernel. Blur is
 *  equal in both x and y directions.
 */
static void gaussian_area_blur(composite_blur_filter_data_t *data)
{
	gs_effect_t *effect = data->effect;
	gs_texture_t *texture = gs_texrender_get_texture(data->input_texrender);

	if (!effect || !texture) {
		return;
	}

	if (data->radius < MIN_GAUSSIAN_BLUR_RADIUS) {
		data->output_texrender =
			create_or_reset_texrender(data->output_texrender);
		texrender_set_texture(texture, data->output_texrender);
		return;
	}

	texture = blend_composite(texture, data);

	data->render2 = create_or_reset_texrender(data->render2);

	// 1. First pass- apply 1D blur kernel to horizontal dir.
	gs_eparam_t *image = gs_effect_get_param_by_name(effect, "image");
	gs_effect_set_texture(image, texture);

	switch (data->device_type) {
	case GS_DEVICE_DIRECT3D_11:
		if (data->param_weight) {
			gs_effect_set_val(data->param_weight,
					  data->kernel.array,
					  data->kernel.num * sizeof(float));
		}
		if (data->param_offset) {
			gs_effect_set_val(data->param_offset,
					  data->offset.array,
					  data->offset.num * sizeof(float));
		}
		break;
	case GS_DEVICE_OPENGL:
		if (data->param_kernel_texture) {
			gs_effect_set_texture(data->param_kernel_texture,
					      data->kernel_texture);
		}
	}

	const int k_size = (int)data->kernel_size;
	if (data->param_kernel_size) {
		gs_effect_set_int(data->param_kernel_size, k_size);
	}

	struct vec2 texel_step;
	texel_step.x = 1.0f / data->width;
	texel_step.y = 0.0f;
	if (data->param_texel_step) {
		gs_effect_set_vec2(data->param_texel_step, &texel_step);
	}

	set_blending_parameters();

	if (gs_texrender_begin(data->render2, data->width, data->height)) {
		gs_ortho(0.0f, (float)data->width, 0.0f, (float)data->height,
			 -100.0f, 100.0f);
		while (gs_effect_loop(effect, "Draw"))
			gs_draw_sprite(texture, 0, data->width, data->height);
		gs_texrender_end(data->render2);
	}

	// 2. Save texture from first pass in variable "texture"
	texture = gs_texrender_get_texture(data->render2);

	// 3. Second Pass- Apply 1D blur kernel vertically.
	image = gs_effect_get_param_by_name(effect, "image");
	gs_effect_set_texture(image, texture);

	if (data->device_type == GS_DEVICE_OPENGL &&
	    data->param_kernel_texture) {
		gs_effect_set_texture(data->param_kernel_texture,
				      data->kernel_texture);
	}

	texel_step.x = 0.0f;
	texel_step.y = 1.0f / data->height;
	if (data->param_texel_step) {
		gs_effect_set_vec2(data->param_texel_step, &texel_step);
	}

	data->output_texrender =
		create_or_reset_texrender(data->output_texrender);

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

/*
 *  Performs a directional blur using the gaussian kernel.
 */
static void gaussian_directional_blur(composite_blur_filter_data_t *data)
{
	gs_effect_t *effect = data->effect;
	gs_texture_t *texture = gs_texrender_get_texture(data->input_texrender);

	if (!effect || !texture) {
		return;
	}

	if (data->radius < MIN_GAUSSIAN_BLUR_RADIUS) {
		data->output_texrender =
			create_or_reset_texrender(data->output_texrender);
		texrender_set_texture(texture, data->output_texrender);
		return;
	}

	texture = blend_composite(texture, data);
	//gs_texture_t *background_texture = NULL;
	//if (data->background) {
	//	get_background(data);
	//	background_texture =
	//		gs_texrender_get_texture(data->background_texrender);
	//	gs_eparam_t *background_img =
	//		gs_effect_get_param_by_name(effect, "background");
	//	gs_effect_set_texture(background_img, background_texture);
	//}


	// 1. Single pass- blur only in one direction
	gs_eparam_t *image = gs_effect_get_param_by_name(effect, "image");
	gs_effect_set_texture(image, texture);

	switch (data->device_type) {
	case GS_DEVICE_DIRECT3D_11:
		if (data->param_weight) {
			gs_effect_set_val(data->param_weight,
					  data->kernel.array,
					  data->kernel.num * sizeof(float));
		}
		if (data->param_offset) {
			gs_effect_set_val(data->param_offset,
					  data->offset.array,
					  data->offset.num * sizeof(float));
		}
		break;
	case GS_DEVICE_OPENGL:
		if (data->param_kernel_texture) {
			gs_effect_set_texture(data->param_kernel_texture,
					      data->kernel_texture);
		}
		break;
	}

	const int k_size = (int)data->kernel_size;
	if (data->param_kernel_size) {
		gs_effect_set_int(data->param_kernel_size, k_size);
	}

	struct vec2 texel_step;
	float rads = -data->angle * (M_PI / 180.0f);
	texel_step.x = (float)cos(rads) / data->width;
	texel_step.y = (float)sin(rads) / data->height;
	if (data->param_texel_step) {
		gs_effect_set_vec2(data->param_texel_step, &texel_step);
	}

	set_blending_parameters();

	data->output_texrender =
		create_or_reset_texrender(data->output_texrender);

	//const char *technique = data->background ? "DrawComposite" : "Draw";
	const char *technique = "Draw";

	if (gs_texrender_begin(data->output_texrender, data->width,
			       data->height)) {
		gs_ortho(0.0f, (float)data->width, 0.0f, (float)data->height,
			 -100.0f, 100.0f);
		while (gs_effect_loop(effect, technique))
			gs_draw_sprite(texture, 0, data->width, data->height);
		gs_texrender_end(data->output_texrender);
	}

	gs_blend_state_pop();
}

/*
 *  Performs a motion blur using the gaussian kernel.
 */
static void gaussian_motion_blur(composite_blur_filter_data_t *data)
{
	gs_effect_t *effect = data->effect;
	gs_texture_t *texture = gs_texrender_get_texture(data->input_texrender);

	if (!effect || !texture) {
		return;
	}

	if (data->radius < MIN_GAUSSIAN_BLUR_RADIUS) {
		data->output_texrender =
			create_or_reset_texrender(data->output_texrender);
		texrender_set_texture(texture, data->output_texrender);
		return;
	}

	texture = blend_composite(texture, data);

	// 1. Single pass- blur only in one direction
	gs_eparam_t *image = gs_effect_get_param_by_name(effect, "image");
	gs_effect_set_texture(image, texture);

	switch (data->device_type) {
	case GS_DEVICE_DIRECT3D_11:
		if (data->param_weight) {
			gs_effect_set_val(data->param_weight,
					  data->kernel.array,
					  data->kernel.num * sizeof(float));
		}
		if (data->param_offset) {
			gs_effect_set_val(data->param_offset,
					  data->offset.array,
					  data->offset.num * sizeof(float));
		}
		break;
	case GS_DEVICE_OPENGL:
		if (data->param_kernel_texture) {
			gs_effect_set_texture(data->param_kernel_texture,
					      data->kernel_texture);
		}
		break;
	}

	const int k_size = (int)data->kernel_size;
	if (data->param_kernel_size) {
		gs_effect_set_int(data->param_kernel_size, k_size);
	}

	struct vec2 texel_step;
	float rads = -data->angle * (M_PI / 180.0f);
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
		gs_ortho(0.0f, (float)data->width, 0.0f, (float)data->height,
			 -100.0f, 100.0f);
		while (gs_effect_loop(effect, "Draw"))
			gs_draw_sprite(texture, 0, data->width, data->height);
		gs_texrender_end(data->output_texrender);
	}

	gs_blend_state_pop();
}

/*
 *  Performs a zoom blur using the gaussian kernel. Blur for a pixel
 *  is performed in direction of zoom center point.
 */
static void gaussian_zoom_blur(composite_blur_filter_data_t *data)
{
	gs_effect_t *effect = data->effect;
	gs_texture_t *texture = gs_texrender_get_texture(data->input_texrender);

	if (!effect || !texture) {
		return;
	}

	if (data->radius < MIN_GAUSSIAN_BLUR_RADIUS) {
		data->output_texrender =
			create_or_reset_texrender(data->output_texrender);
		texrender_set_texture(texture, data->output_texrender);
		return;
	}

	texture = blend_composite(texture, data);

	// 1. Single pass- blur only in one direction
	gs_eparam_t *image = gs_effect_get_param_by_name(effect, "image");
	gs_effect_set_texture(image, texture);

	switch (data->device_type) {
	case GS_DEVICE_DIRECT3D_11:
		if (data->param_weight) {
			gs_effect_set_val(data->param_weight,
					  data->kernel.array,
					  data->kernel.num * sizeof(float));
		}
		if (data->param_offset) {
			gs_effect_set_val(data->param_offset,
					  data->offset.array,
					  data->offset.num * sizeof(float));
		}
		break;
	case GS_DEVICE_OPENGL:
		if (data->param_kernel_texture) {
			gs_effect_set_texture(data->param_kernel_texture,
					      data->kernel_texture);
		}
		break;
	}

	const int k_size = (int)data->kernel_size;
	if (data->param_kernel_size) {
		gs_effect_set_int(data->param_kernel_size, k_size);
	}

	struct vec2 radial_center;
	radial_center.x = data->center_x;
	radial_center.y = data->center_y;
	if (data->param_radial_center) {
		gs_effect_set_vec2(data->param_radial_center, &radial_center);
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
		gs_ortho(0.0f, (float)data->width, 0.0f, (float)data->height,
			 -100.0f, 100.0f);
		while (gs_effect_loop(effect, "Draw"))
			gs_draw_sprite(texture, 0, data->width, data->height);
		gs_texrender_end(data->output_texrender);
	}

	gs_blend_state_pop();
}


/*
 *  Performs a vector blur using the gaussian kernel. Blur for a pixel
 *  is performed in direction of the image gradient.
 */
static void gaussian_vector_blur(composite_blur_filter_data_t* data)
{
	gaussian_vector_gradient(data);

	//gs_texrender_t* tmp = data->output_texrender;
	//data->output_texrender = data->vb_gradient;
	//data->vb_gradient = tmp;
	//return;

	gaussian_vector_smooth_gradient(data);
	//gs_texrender_t* tmp = data->output_texrender;
	//data->output_texrender = data->vb_smoothed_gradient;
	//data->vb_smoothed_gradient = tmp;
	//return;

	gaussian_vector_apply_blur(data);
}

static void gaussian_vector_gradient(composite_blur_filter_data_t* data)
{
	gs_effect_t* effect = data->gradient_effect;
	gs_texture_t* texture = NULL;

	gs_texrender_t* source_render = NULL;

	if (data->vector_blur_source) {
		obs_source_t* source = obs_weak_source_get_source(
			data->vector_blur_source);

		if (!source) {
			return;
		}

		const enum gs_color_space preferred_spaces[] = {
			GS_CS_SRGB_16F,
			GS_CS_709_EXTENDED,
			//GS_CS_SRGB,
		};
		const enum gs_color_space space = obs_source_get_color_space(
			source, OBS_COUNTOF(preferred_spaces),
			preferred_spaces);
		const enum gs_color_format format =
			gs_get_format_from_space(space);

		// Set up a tex renderer for source
		source_render = gs_texrender_create(format, GS_ZS_NONE);
		uint32_t base_width = obs_source_get_width(source);
		uint32_t base_height = obs_source_get_height(source);
		gs_blend_state_push();
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);
		if (gs_texrender_begin_with_color_space(
			source_render, base_width, base_height, space)) {
			const float w = (float)base_width;
			const float h = (float)base_height;
			struct vec4 clear_color;
			vec4_zero(&clear_color);
			gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
			gs_ortho(0.0f, w, 0.0f, h, -100.0f, 100.0f);
			obs_source_video_render(source);
			gs_texrender_end(source_render);
		}
		gs_blend_state_pop();
		obs_source_release(source);
		texture = gs_texrender_get_texture(source_render);
	} else {
		texture = gs_texrender_get_texture(data->input_texrender);
	}



	if (!effect || !texture) {
		return;
	}
	
	// 1. Single pass- blur only in one direction
	gs_eparam_t* image = gs_effect_get_param_by_name(effect, "image");
	gs_effect_set_texture(image, texture);
	if (data->param_gradient_image) {
		gs_effect_set_texture(data->param_gradient_image, texture);
	}

	// If we are doing the channel thing, change channel...
	if (data->param_gradient_channel) {
		gs_effect_set_int(data->param_gradient_channel, data->vector_blur_channel);
	}
	//const char* technique = data->vector_blur_channel < 4 ? "DrawChannelGrad" : data->vector_blur_channel == 4 ? "DrawLuminanceGrad" : "DrawSaturationGrad";
	const char* technique = data->vector_blur_type == GRADIENT_TYPE_SOBEL ? "DrawSobelGrad" :
		data->vector_blur_type == GRADIENT_TYPE_CENTRAL_LIMIT_DIFF ? "DrawCentralDiffGrad" :
		"DrawForwardDiffGrad";
	struct vec2 uv_size;
	uv_size.x = (float)data->width;
	uv_size.y = (float)data->height;
	if (data->param_gradient_uv_size) {
		gs_effect_set_vec2(data->param_gradient_uv_size, &uv_size);
	}

	set_blending_parameters();

	data->vb_gradient = create_or_reset_texrender(data->vb_gradient);

	if (gs_texrender_begin(data->vb_gradient, data->width,
		data->height)) {
		gs_ortho(0.0f, (float)data->width, 0.0f, (float)data->height,
			-100.0f, 100.0f);
		while (gs_effect_loop(effect, technique))
			gs_draw_sprite(texture, 0, data->width, data->height);
		gs_texrender_end(data->vb_gradient);
	}

	if (source_render) {
		gs_texrender_destroy(source_render);
	}

	gs_blend_state_pop();
}

static void gaussian_vector_smooth_gradient(composite_blur_filter_data_t* data)
{
	if (!data->vb_smoothed_gradient) {
		data->vb_smoothed_gradient = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	}
	data->kawase_passes = (int)(data->vector_blur_smoothing) + 1;

	gs_texrender_t* tmp = data->input_texrender;
	data->input_texrender = data->vb_gradient;
	data->vb_gradient = tmp;

	render_video_dual_kawase(data);

	tmp = data->input_texrender;
	data->input_texrender = data->vb_gradient;
	data->vb_gradient = tmp;

	tmp = data->output_texrender;
	data->output_texrender = data->vb_smoothed_gradient;
	data->vb_smoothed_gradient = tmp;
}

static void gaussian_vector_apply_blur(composite_blur_filter_data_t* data)
{
	gs_effect_t* effect = data->gv_effect;
	gs_texture_t* texture = gs_texrender_get_texture(data->input_texrender);

	if (!effect || !texture) {
		return;
	}

	if (fabsf(data->vector_blur_amount) < MIN_GAUSSIAN_BLUR_RADIUS) {
		data->output_texrender =
			create_or_reset_texrender(data->output_texrender);
		texrender_set_texture(texture, data->output_texrender);
		return;
	}

	texture = blend_composite(texture, data);

	// 1. Single pass- blur only in one direction
	gs_eparam_t* image = gs_effect_get_param_by_name(effect, "image");
	gs_effect_set_texture(image, texture);

	switch (data->device_type) {
	case GS_DEVICE_DIRECT3D_11:
		if (data->param_weight) {
			gs_effect_set_val(data->param_weight,
				data->kernel.array,
				data->kernel.num * sizeof(float));
		}
		if (data->param_offset) {
			gs_effect_set_val(data->param_offset,
				data->offset.array,
				data->offset.num * sizeof(float));
		}
		break;
	case GS_DEVICE_OPENGL:
		if (data->param_kernel_texture) {
			gs_effect_set_texture(data->param_kernel_texture,
				data->kernel_texture);
		}
		break;
	}

	if (data->param_radius) {
		gs_effect_set_float(data->param_radius, data->vector_blur_amount);
	}

	const int k_size = (int)data->kernel_size;
	if (data->param_kernel_size) {
		gs_effect_set_int(data->param_kernel_size, k_size);
	}

	struct vec2 uv_size;
	uv_size.x = (float)data->width;
	uv_size.y = (float)data->height;
	if (data->param_uv_size) {
		gs_effect_set_vec2(data->param_uv_size, &uv_size);
	}

	if (data->param_gradient_map) {
		gs_texture_t* gradient_map = gs_texrender_get_texture(data->vb_smoothed_gradient);
		gs_effect_set_texture(data->param_gradient_map, gradient_map);
	}

	set_blending_parameters();

	data->output_texrender =
		create_or_reset_texrender(data->output_texrender);

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

static void load_1d_gaussian_effect(composite_blur_filter_data_t *filter)
{
	if (filter->effect != NULL) {
		obs_enter_graphics();
		gs_effect_destroy(filter->effect);
		filter->effect = NULL;
		obs_leave_graphics();
	}

	const char *effect_file_path =
		filter->device_type == GS_DEVICE_DIRECT3D_11
			? "/shaders/gaussian_1d.effect"
			: "/shaders/gaussian_1d_texture.effect";

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
			} else if (strcmp(info.name, "offset") == 0) {
				filter->param_offset = param;
			} else if (strcmp(info.name, "weight") == 0) {
				filter->param_weight = param;
			} else if (strcmp(info.name, "kernel_size") == 0) {
				filter->param_kernel_size = param;
			} else if (strcmp(info.name, "kernel_texture") == 0) {
				filter->param_kernel_texture = param;
			}
		}
	}
}

static void load_motion_gaussian_effect(composite_blur_filter_data_t *filter)
{

	if (filter->effect != NULL) {
		obs_enter_graphics();
		gs_effect_destroy(filter->effect);
		filter->effect = NULL;
		obs_leave_graphics();
	}

	const char *effect_file_path =
		filter->device_type == GS_DEVICE_DIRECT3D_11
			? "/shaders/gaussian_motion.effect"
			: "/shaders/gaussian_motion_texture.effect";

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
			} else if (strcmp(info.name, "offset") == 0) {
				filter->param_offset = param;
			} else if (strcmp(info.name, "weight") == 0) {
				filter->param_weight = param;
			} else if (strcmp(info.name, "kernel_size") == 0) {
				filter->param_kernel_size = param;
			} else if (strcmp(info.name, "kernel_texture") == 0) {
				filter->param_kernel_texture = param;
			}
		}
	}
}

static void load_radial_gaussian_effect(composite_blur_filter_data_t *filter)
{
	if (filter->effect != NULL) {
		obs_enter_graphics();
		gs_effect_destroy(filter->effect);
		filter->effect = NULL;
		obs_leave_graphics();
	}

	const char *effect_file_path =
		filter->device_type == GS_DEVICE_DIRECT3D_11
			? "/shaders/gaussian_radial.effect"
			: "/shaders/gaussian_radial_texture.effect";

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
			} else if (strcmp(info.name, "offset") == 0) {
				filter->param_offset = param;
			} else if (strcmp(info.name, "weight") == 0) {
				filter->param_weight = param;
			} else if (strcmp(info.name, "kernel_size") == 0) {
				filter->param_kernel_size = param;
			} else if (strcmp(info.name, "kernel_texture") == 0) {
				filter->param_kernel_texture = param;
			} else if (strcmp(info.name, "radial_center") == 0) {
				filter->param_radial_center = param;
			}
		}
	}
}

static void load_vector_gaussian_effect(composite_blur_filter_data_t* filter)
{
	if (filter->gv_effect != NULL) {
		obs_enter_graphics();
		gs_effect_destroy(filter->gv_effect);
		filter->gv_effect = NULL;
		obs_leave_graphics();
	}

	const char* effect_file_path =
		filter->device_type == GS_DEVICE_DIRECT3D_11
		? "/shaders/gaussian_vector.effect"
		: "/shaders/gaussian_vector_texture.effect";

	filter->gv_effect = load_shader_effect(filter->gv_effect, effect_file_path);
	if (filter->gv_effect) {
		size_t effect_count = gs_effect_get_num_params(filter->gv_effect);
		for (size_t effect_index = 0; effect_index < effect_count;
			effect_index++) {
			gs_eparam_t* param = gs_effect_get_param_by_idx(
				filter->gv_effect, effect_index);
			struct gs_effect_param_info info;
			gs_effect_get_param_info(param, &info);
			if (strcmp(info.name, "uv_size") == 0) {
				filter->param_uv_size = param;
			}
			else if (strcmp(info.name, "gradient") == 0) {
				filter->param_gradient_map = param;
			}
			else if (strcmp(info.name, "offset") == 0) {
				filter->param_offset = param;
			}
			else if (strcmp(info.name, "weight") == 0) {
				filter->param_weight = param;
			}
			else if (strcmp(info.name, "kernel_size") == 0) {
				filter->param_kernel_size = param;
			}
			else if (strcmp(info.name, "kernel_texture") == 0) {
				filter->param_kernel_texture = param;
			}
			else if (strcmp(info.name, "blur_radius") == 0) {
				filter->param_radius = param;
			}
		}
	}
}

gs_effect_t* load_gradient_shader_effect(gs_effect_t* effect,
	const char* effect_file_path, const char* sample_type)
{
	if (effect != NULL) {
		obs_enter_graphics();
		gs_effect_destroy(effect);
		effect = NULL;
		obs_leave_graphics();
	}
	char* shader_text = NULL;
	struct dstr filename = { 0 };
	dstr_cat(&filename, obs_get_module_data_path(obs_current_module()));
	dstr_cat(&filename, effect_file_path);
	shader_text = load_shader_from_file(filename.array);
	char* errors = NULL;
	struct dstr shader_dstr = { 0 };
	dstr_init_copy(&shader_dstr, shader_text);
	dstr_replace(&shader_dstr, "<SAMPLE_FUNCTION>", sample_type);
	obs_enter_graphics();
	effect = gs_effect_create(shader_dstr.array, NULL, &errors);
	obs_leave_graphics();

	dstr_free(&shader_dstr);
	bfree(shader_text);

	if (effect == NULL) {
		blog(LOG_WARNING,
			"[obs-composite-blur] Unable to load .effect file.  Errors:\n%s",
			(errors == NULL || strlen(errors) == 0 ? "(None)"
				: errors));
		bfree(errors);
	}

	dstr_free(&filename);

	return effect;
}

static void load_gradient_effect(composite_blur_filter_data_t* filter)
{
	if (filter->gradient_effect != NULL) {
		obs_enter_graphics();
		gs_effect_destroy(filter->gradient_effect);
		filter->gradient_effect = NULL;
		obs_leave_graphics();
	}

	const char* effect_file_path = "/shaders/gradient_map.effect";
	const char* sample_type = filter->vector_blur_channel == GRADIENT_CHANNEL_LUMINANCE ? "luminance" :
		filter->vector_blur_channel == GRADIENT_CHANNEL_SATURATION ? "saturation" :
		"sample_channel";

	filter->gradient_effect = load_gradient_shader_effect(filter->gradient_effect, effect_file_path, sample_type);
	if (filter->gradient_effect) {
		size_t effect_count = gs_effect_get_num_params(filter->gradient_effect);
		for (size_t effect_index = 0; effect_index < effect_count;
			effect_index++) {
			gs_eparam_t* param = gs_effect_get_param_by_idx(
				filter->gradient_effect, effect_index);
			struct gs_effect_param_info info;
			gs_effect_get_param_info(param, &info);
			if (strcmp(info.name, "uv_size") == 0) {
				filter->param_gradient_uv_size = param;
			}
			else if (strcmp(info.name, "image") == 0) {
				filter->param_gradient_image = param;
			}
			else if (strcmp(info.name, "channel") == 0) {
				filter->param_gradient_channel = param;
			}
		}
	}
}

static void sample_kernel(float radius, composite_blur_filter_data_t *filter)
{
	const size_t max_size = 128;
	const float max_radius = 250.0;
	const float min_radius = 0.0;

	fDarray d_weights;
	da_init(d_weights);

	radius *= 3.0f;
	radius = (float)fmax(fmin(radius, max_radius), min_radius);

	// 1. Calculate discrete weights
	const float bins_per_pixel =
		((2.f * (float)gaussian_kernel_size - 1.f)) /
		(1.f + 2.f * radius);
	size_t current_bin = 0;
	float fractional_bin = 0.5f;
	float ceil_radius = (radius - (float)floor(radius)) < 0.001f
				    ? radius
				    : (float)ceil(radius);
	float fractional_extra = 1.0f - (ceil_radius - radius);

	for (int i = 0; i <= (int)ceil_radius; i++) {
		float fractional_pixel = i < (int)ceil_radius ? 1.0f
					 : fractional_extra < 0.002f
						 ? 1.0f
						 : fractional_extra;
		float bpp_mult = i == 0 ? 0.5f : 1.0f;
		float weight = 1.0f / bpp_mult * fractional_bin *
			       gaussian_kernel[current_bin];
		float remaining_bins =
			bpp_mult * fractional_pixel * bins_per_pixel -
			fractional_bin;
		while ((int)floor(remaining_bins) > 0) {
			current_bin++;
			weight +=
				1.0f / bpp_mult * gaussian_kernel[current_bin];
			remaining_bins -= 1.f;
		}
		current_bin++;
		if (remaining_bins > 1.e-6f) {
			weight += 1.0f / bpp_mult *
				  gaussian_kernel[current_bin] * remaining_bins;
			fractional_bin = 1.0f - remaining_bins;
		} else {
			fractional_bin = 1.0f;
		}
		if (weight > 1.0001f || weight < 0.0f) {
			blog(LOG_WARNING,
			     "   === BAD WEIGHT VALUE FOR GAUSSIAN === [%d] %f",
			     (int)(d_weights.num + 1), weight);
			weight = 0.0;
		}
		da_push_back(d_weights, &weight);
	}
	fDarray d_offsets;
	da_init(d_offsets);

	// 2. Calculate discrete offsets
	for (int i = 0; i <= (int)ceil_radius; i++) {
		float val = (float)i;
		da_push_back(d_offsets, &val);
	}

	fDarray weights;
	da_init(weights);

	fDarray offsets;
	da_init(offsets);

	DARRAY(float) weight_offset_texture;
	da_init(weight_offset_texture);

	// 3. Calculate linear sampled weights and offsets
	da_push_back(weights, &d_weights.array[0]);
	da_push_back(offsets, &d_offsets.array[0]);

	da_push_back(weight_offset_texture, &d_weights.array[0]);
	da_push_back(weight_offset_texture, &d_offsets.array[0]);

	for (size_t i = 1; i < d_weights.num - 1; i += 2) {
		const float weight =
			d_weights.array[i] + d_weights.array[i + 1];
		const float offset =
			(d_offsets.array[i] * d_weights.array[i] +
			 d_offsets.array[i + 1] * d_weights.array[i + 1]) /
			weight;

		da_push_back(weights, &weight);
		da_push_back(offsets, &offset);

		da_push_back(weight_offset_texture, &weight);
		da_push_back(weight_offset_texture, &offset);
	}
	if (d_weights.num % 2 == 0) {
		const float weight = d_weights.array[d_weights.num - 1];
		const float offset = d_offsets.array[d_offsets.num - 1];
		da_push_back(weights, &weight);
		da_push_back(offsets, &offset);

		da_push_back(weight_offset_texture, &weight);
		da_push_back(weight_offset_texture, &offset);
	}

	// 4. Pad out kernel arrays to length of max_size
	const size_t padding = max_size - weights.num;
	filter->kernel_size = weights.num;

	for (size_t i = 0; i < padding; i++) {
		float pad = 0.0f;
		da_push_back(weights, &pad);
	}
	da_free(filter->kernel);
	filter->kernel = weights;
	for (size_t i = 0; i < padding; i++) {
		float pad = 0.0f;
		da_push_back(offsets, &pad);
	}

	da_free(filter->offset);
	filter->offset = offsets;

	// Generate the kernel and offsets as a texture for OpenGL systems
	// where the red value is the kernel weight and the green value
	// is the offset value. This is only used for OpenGL systems, and
	// should probably be macro'd out on windows builds, but generating
	// the texture is quite cheap.
	obs_enter_graphics();
	if (filter->kernel_texture) {
		gs_texture_destroy(filter->kernel_texture);
	}

	filter->kernel_texture = gs_texture_create(
		(uint32_t)weight_offset_texture.num / 2u, 1u, GS_RG32F, 1u,
		(const uint8_t **)&weight_offset_texture.array, 0);

	if (!filter->kernel_texture) {
		blog(LOG_WARNING, "Gaussian Texture couldn't be created.");
	}
	obs_leave_graphics();
	da_free(d_weights);
	da_free(d_offsets);
	da_free(weight_offset_texture);
}
