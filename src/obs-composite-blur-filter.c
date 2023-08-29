#include "obs-composite-blur-filter.h"

#include "blur/gaussian.h"
#include "blur/box.h"
#include "blur/pixelate.h"
#include "blur/dual_kawase.h"

struct obs_source_info obs_composite_blur = {
	.id = "obs_composite_blur",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = composite_blur_name,
	.create = composite_blur_create,
	.destroy = composite_blur_destroy,
	.update = composite_blur_update,
	.video_render = composite_blur_video_render,
	.video_tick = composite_blur_video_tick,
	.get_width = composite_blur_width,
	.get_height = composite_blur_height,
	.get_properties = composite_blur_properties};

static const char *composite_blur_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("CompositeBlurFilter");
}

static void *composite_blur_create(obs_data_t *settings, obs_source_t *source)
{
	composite_blur_filter_data_t *filter =
		bzalloc(sizeof(composite_blur_filter_data_t));
	filter->context = source;
	filter->radius = 0.0f;
	filter->radius_last = -1.0f;
	filter->angle = 0.0f;
	filter->passes = 1;
	filter->center_x = 0.0f;
	filter->center_y = 0.0f;
	filter->blur_algorithm = ALGO_NONE;
	filter->blur_algorithm_last = -1;
	filter->blur_type = TYPE_NONE;
	filter->blur_type_last = -1;
	filter->kawase_passes = 1;
	filter->rendering = false;
	filter->reload = true;
	filter->param_uv_size = NULL;
	filter->param_dir = NULL;
	filter->param_radius = NULL;
	filter->param_background = NULL;
	filter->video_render = NULL;
	filter->load_effect = NULL;
	filter->update = NULL;
	filter->kernel_texture = NULL;
	filter->pixelate_type = 1;
	filter->pixelate_type_last = -1;
	filter->mask_crop_left = 0.0f;
	filter->mask_crop_right = 0.0f;
	filter->mask_crop_top = 0.0f;
	filter->mask_crop_bot = 0.0f;
	filter->mask_type = 0;
	filter->mask_type_last = -1;
	filter->mask_crop_corner_radius = 0.0f;
	filter->mask_crop_invert = false;

	filter->mask_source_filter_type = EFFECT_MASK_SOURCE_FILTER_ALPHA;
	filter->mask_source_filter_red = 0.0;
	filter->mask_source_filter_green = 0.0;
	filter->mask_source_filter_blue = 0.0;
	filter->mask_source_filter_alpha = 0.0;
	filter->mask_source_multiplier = 1.0;
	filter->mask_source_source = NULL;
	filter->mask_source_invert = false;

	da_init(filter->kernel);

	obs_source_update(source, settings);

	return filter;
}

static void composite_blur_destroy(void *data)
{
	struct composite_blur_filter_data *filter = data;

	obs_enter_graphics();
	if (filter->effect) {
		gs_effect_destroy(filter->effect);
	}
	if (filter->effect_2) {
		gs_effect_destroy(filter->effect_2);
	}
	if (filter->composite_effect) {
		gs_effect_destroy(filter->composite_effect);
	}
	if (filter->mix_effect) {
		gs_effect_destroy(filter->mix_effect);
	}
	if (filter->effect_mask_effect) {
		gs_effect_destroy(filter->effect_mask_effect);
	}
	if (filter->render) {
		gs_texrender_destroy(filter->render);
	}
	if (filter->render2) {
		gs_texrender_destroy(filter->render2);
	}

	if (filter->input_texrender) {
		gs_texrender_destroy(filter->input_texrender);
	}
	if (filter->output_texrender) {
		gs_texrender_destroy(filter->output_texrender);
	}

	if (filter->kernel_texture) {
		gs_texture_destroy(filter->kernel_texture);
	}

	obs_leave_graphics();
	bfree(filter);
}

static uint32_t composite_blur_width(void *data)
{
	struct composite_blur_filter_data *filter = data;
	return filter->width;
}

static uint32_t composite_blur_height(void *data)
{
	struct composite_blur_filter_data *filter = data;
	return filter->height;
}

static void composite_blur_update(void *data, obs_data_t *settings)
{
	struct composite_blur_filter_data *filter = data;

	filter->blur_algorithm =
		(int)obs_data_get_int(settings, "blur_algorithm");

	if (filter->blur_algorithm != filter->blur_algorithm_last) {
		filter->blur_algorithm_last = filter->blur_algorithm;
		filter->reload = true;
	}

	filter->blur_type = (int)obs_data_get_int(settings, "blur_type");

	if (filter->blur_type != filter->blur_type_last) {
		filter->blur_type_last = filter->blur_type;
		filter->reload = true;
	}

	filter->pixelate_type =
		(int)obs_data_get_int(settings, "pixelate_type");

	if (filter->pixelate_type != filter->pixelate_type_last) {
		filter->pixelate_type_last = filter->pixelate_type;
		filter->reload = true;
	}

	filter->mask_type = (int)obs_data_get_int(settings, "effect_mask");
	filter->mask_crop_top =
		(float)obs_data_get_double(settings, "effect_mask_crop_top");
	filter->mask_crop_bot =
		(float)obs_data_get_double(settings, "effect_mask_crop_bottom");
	filter->mask_crop_left =
		(float)obs_data_get_double(settings, "effect_mask_crop_left");
	filter->mask_crop_right =
		(float)obs_data_get_double(settings, "effect_mask_crop_right");
	filter->mask_crop_corner_radius = (float)obs_data_get_double(
		settings, "effect_mask_crop_corner_radius");
	filter->mask_crop_invert =
		obs_data_get_bool(settings, "effect_mask_crop_invert");
	if (filter->mask_type != filter->mask_type_last) {
		filter->mask_type_last = filter->mask_type;
		effect_mask_load_effect(filter);
	}

	filter->mask_source_filter_type = (int)obs_data_get_int(
		settings, "effect_mask_source_filter_list");
	switch (filter->mask_source_filter_type) {
	case EFFECT_MASK_SOURCE_FILTER_ALPHA:
		filter->mask_source_filter_red = 0.0f;
		filter->mask_source_filter_green = 0.0f;
		filter->mask_source_filter_blue = 0.0f;
		filter->mask_source_filter_alpha = 1.0f;
		break;
	case EFFECT_MASK_SOURCE_FILTER_GRAYSCALE:
		filter->mask_source_filter_red = 0.33334f;
		filter->mask_source_filter_green = 0.33333f;
		filter->mask_source_filter_blue = 0.33333f;
		filter->mask_source_filter_alpha = 0.0f;
		break;
	case EFFECT_MASK_SOURCE_FILTER_LUMINOSITY:
		filter->mask_source_filter_red = 0.299f;
		filter->mask_source_filter_green = 0.587f;
		filter->mask_source_filter_blue = 0.114f;
		filter->mask_source_filter_alpha = 0.0f;
		break;
	case EFFECT_MASK_SOURCE_FILTER_SLIDERS:
		filter->mask_source_filter_red = (float)obs_data_get_double(
			settings, "effect_mask_source_filter_red");
		filter->mask_source_filter_green = (float)obs_data_get_double(
			settings, "effect_mask_source_filter_green");
		filter->mask_source_filter_blue = (float)obs_data_get_double(
			settings, "effect_mask_source_filter_blue");
		filter->mask_source_filter_alpha = (float)obs_data_get_double(
			settings, "effect_mask_source_filter_alpha");
		break;
	}

	const char *mask_source_name =
		obs_data_get_string(settings, "effect_mask_source_source");
	obs_source_t *mask_source =
		(mask_source_name && strlen(mask_source_name))
			? obs_get_source_by_name(mask_source_name)
			: NULL;
	if (mask_source) {
		obs_weak_source_release(filter->mask_source_source);
		filter->mask_source_source =
			obs_source_get_weak_source(mask_source);
		obs_source_release(mask_source);
	} else {
		filter->mask_source_source = NULL;
	}

	filter->mask_source_multiplier = (float)obs_data_get_double(
		settings, "effect_mask_source_filter_multiplier");

	filter->mask_source_invert =
		obs_data_get_bool(settings, "effect_mask_source_invert");

	filter->radius = (float)obs_data_get_double(settings, "radius");
	filter->passes = (int)obs_data_get_int(settings, "passes");
	filter->kawase_passes =
		(int)obs_data_get_int(settings, "kawase_passes");

	filter->center_x = (float)obs_data_get_double(settings, "center_x");
	filter->center_y = (float)obs_data_get_double(settings, "center_y");

	filter->angle = (float)obs_data_get_double(settings, "angle");
	filter->tilt_shift_center =
		(float)obs_data_get_double(settings, "tilt_shift_center");
	filter->tilt_shift_width =
		(float)obs_data_get_double(settings, "tilt_shift_width");
	filter->tilt_shift_angle =
		(float)obs_data_get_double(settings, "tilt_shift_angle");

	const char *source_name = obs_data_get_string(settings, "background");
	obs_source_t *source = (source_name && strlen(source_name))
				       ? obs_get_source_by_name(source_name)
				       : NULL;
	if (source) {
		obs_weak_source_release(filter->background);
		filter->background = obs_source_get_weak_source(source);
		obs_source_release(source);
	} else {
		filter->background = NULL;
	}

	if (filter->reload) {
		filter->reload = false;
		composite_blur_reload_effect(filter);
		obs_source_update_properties(filter->context);
	}

	if (filter->update) {
		filter->update(filter);
	}
}

static void get_input_source(composite_blur_filter_data_t *filter)
{
	gs_effect_t *pass_through = obs_get_base_effect(OBS_EFFECT_DEFAULT);

	filter->input_texrender =
		create_or_reset_texrender(filter->input_texrender);
	if (obs_source_process_filter_begin(filter->context, GS_RGBA,
					    OBS_ALLOW_DIRECT_RENDERING) &&
	    gs_texrender_begin(filter->input_texrender, filter->width,
			       filter->height)) {

		set_blending_parameters();

		gs_ortho(0.0f, (float)filter->width, 0.0f,
			 (float)filter->height, -100.0f, 100.0f);

		obs_source_process_filter_end(filter->context, pass_through,
					      filter->width, filter->height);
		gs_texrender_end(filter->input_texrender);
		gs_blend_state_pop();
	}
}

static void draw_output_to_source(composite_blur_filter_data_t *filter)
{
	gs_texture_t *texture =
		gs_texrender_get_texture(filter->output_texrender);
	gs_effect_t *pass_through = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_eparam_t *param = gs_effect_get_param_by_name(pass_through, "image");
	gs_effect_set_texture(param, texture);
	while (gs_effect_loop(pass_through, "Draw")) {
		gs_draw_sprite(texture, 0, filter->width, filter->height);
	}
}

static void composite_blur_video_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);
	struct composite_blur_filter_data *filter = data;

	if (filter->rendered) {
		draw_output_to_source(filter);
		return;
	}

	if (filter->rendering) {
		obs_source_skip_video_filter(filter->context);
		return;
	}

	filter->rendering = true;

	if (filter->video_render) {
		// 1. Get the input source as a texture renderer:
		get_input_source(filter);

		// 2. Apply effect to texture, and render texture to video
		filter->video_render(filter);

		if (filter->mask_type != EFFECT_MASK_TYPE_NONE) {
			// Swap output and render
			apply_effect_mask(filter);
		}

		// 3. Draw result (filter->output_texrender) to source
		draw_output_to_source(filter);
		filter->rendered = true;
	}

	filter->rendering = false;
}

static void apply_effect_mask(composite_blur_filter_data_t *filter)
{
	switch (filter->mask_type) {
	case EFFECT_MASK_TYPE_CROP:
		apply_effect_mask_crop(filter);
		break;
	case EFFECT_MASK_TYPE_SOURCE:
		apply_effect_mask_source(filter);
		break;
	}
}

static void apply_effect_mask_source(composite_blur_filter_data_t *filter)
{
	// Get source
	obs_source_t *source =
		filter->mask_source_source
			? obs_weak_source_get_source(filter->mask_source_source)
			: NULL;
	if (!source) {
		return;
	}

	const enum gs_color_space preferred_spaces[] = {
		GS_CS_SRGB,
		GS_CS_SRGB_16F,
		GS_CS_709_EXTENDED,
	};
	const enum gs_color_space space = obs_source_get_color_space(
		source, OBS_COUNTOF(preferred_spaces), preferred_spaces);
	const enum gs_color_format format = gs_get_format_from_space(space);

	// Set up a tex renderer for source
	gs_texrender_t *source_render = gs_texrender_create(format, GS_ZS_NONE);
	uint32_t base_width = obs_source_get_base_width(source);
	uint32_t base_height = obs_source_get_base_height(source);
	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);
	if (gs_texrender_begin_with_color_space(source_render, base_width,
						base_height, space)) {
		const float w = (float)base_width;
		const float h = (float)base_height;
		uint32_t flags = obs_source_get_output_flags(source);
		const bool custom_draw = (flags & OBS_SOURCE_CUSTOM_DRAW) != 0;
		const bool async = (flags & OBS_SOURCE_ASYNC) != 0;
		struct vec4 clear_color;

		vec4_zero(&clear_color);
		gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
		gs_ortho(0.0f, w, 0.0f, h, -100.0f, 100.0f);

		if (!custom_draw && !async)
			obs_source_default_render(source);
		else
			obs_source_video_render(source);
		gs_texrender_end(source_render);
	}
	gs_blend_state_pop();
	obs_source_release(source);
	gs_texture_t *alpha_texture = gs_texrender_get_texture(source_render);

	// Swap output with render
	gs_texrender_t *tmp = filter->output_texrender;
	filter->output_texrender = filter->render;
	filter->render = tmp;

	gs_effect_t *effect = filter->effect_mask_effect;
	gs_texture_t *texture =
		gs_texrender_get_texture(filter->input_texrender);
	gs_texture_t *filtered_texture =
		gs_texrender_get_texture(filter->render);

	if (!effect || !texture || !filtered_texture) {
		return;
	}
	gs_eparam_t *image = gs_effect_get_param_by_name(effect, "image");
	gs_effect_set_texture(image, texture);

	gs_eparam_t *filtered_image =
		gs_effect_get_param_by_name(effect, "filtered_image");
	gs_effect_set_texture(filtered_image, filtered_texture);

	gs_eparam_t *alpha_source =
		gs_effect_get_param_by_name(effect, "alpha_source");
	gs_effect_set_texture(alpha_source, alpha_texture);

	gs_eparam_t *invert = gs_effect_get_param_by_name(effect, "inv");
	bool invert_v = filter->mask_source_invert;
	gs_effect_set_bool(invert, invert_v);

	gs_eparam_t *rgba_weights =
		gs_effect_get_param_by_name(effect, "rgba_weights");
	struct vec4 weights;
	weights.x = filter->mask_source_filter_red;
	weights.y = filter->mask_source_filter_green;
	weights.z = filter->mask_source_filter_blue;
	weights.w = filter->mask_source_filter_alpha;
	gs_effect_set_vec4(rgba_weights, &weights);

	gs_eparam_t *multiplier =
		gs_effect_get_param_by_name(effect, "multiplier");
	gs_effect_set_float(multiplier, filter->mask_source_multiplier);

	set_blending_parameters();

	filter->output_texrender =
		create_or_reset_texrender(filter->output_texrender);

	if (gs_texrender_begin(filter->output_texrender, filter->width,
			       filter->height)) {
		while (gs_effect_loop(effect, "Draw"))
			gs_draw_sprite(texture, 0, filter->width,
				       filter->height);
		gs_texrender_end(filter->output_texrender);
	}
	texture = gs_texrender_get_texture(filter->output_texrender);
	gs_texrender_destroy(source_render);
	gs_blend_state_pop();
}

static void apply_effect_mask_crop(composite_blur_filter_data_t *filter)
{
	float right = filter->mask_crop_right / 100.0f;
	float left = filter->mask_crop_left / 100.0f;
	float top = filter->mask_crop_top / 100.0f;
	float bot = filter->mask_crop_bot / 100.0f;

	// Swap output with render
	gs_texrender_t *tmp = filter->output_texrender;
	filter->output_texrender = filter->render;
	filter->render = tmp;

	gs_effect_t *effect = filter->effect_mask_effect;
	gs_texture_t *texture =
		gs_texrender_get_texture(filter->input_texrender);
	gs_texture_t *filtered_texture =
		gs_texrender_get_texture(filter->render);

	if (!effect || !texture || !filtered_texture) {
		return;
	}
	gs_eparam_t *image = gs_effect_get_param_by_name(effect, "image");
	gs_effect_set_texture(image, texture);

	gs_eparam_t *filtered_image =
		gs_effect_get_param_by_name(effect, "filtered_image");
	gs_effect_set_texture(filtered_image, filtered_texture);

	gs_eparam_t *scale = gs_effect_get_param_by_name(effect, "scale");
	struct vec2 scale_v;
	scale_v.x = 1.0f / max(1.0f - right - left, 1.e-6f);
	scale_v.y = 1.0f / max(1.0f - bot - top, 1.e-6f);
	gs_effect_set_vec2(scale, &scale_v);

	gs_eparam_t *box_aspect_ratio =
		gs_effect_get_param_by_name(effect, "box_aspect_ratio");
	struct vec2 box_ar;
	box_ar.x = (1.0f - right - left) * filter->width /
		   min(filter->width, filter->height);
	box_ar.y = (1.0f - bot - top) * filter->height /
		   min(filter->width, filter->height);
	gs_effect_set_vec2(box_aspect_ratio, &box_ar);

	gs_eparam_t *offset = gs_effect_get_param_by_name(effect, "offset");
	struct vec2 offset_v;
	offset_v.x = 1.0f - right - left > 0.0f ? left : -1000.0f;
	offset_v.y = 1.0f - bot - top > 0.0f ? top : -1000.0f;
	gs_effect_set_vec2(offset, &offset_v);

	gs_eparam_t *invert = gs_effect_get_param_by_name(effect, "inv");
	bool invert_v = filter->mask_crop_invert;
	gs_effect_set_bool(invert, invert_v);

	float radius = filter->mask_crop_corner_radius / 100.0f *
		       min(box_ar.x, box_ar.y);

	gs_eparam_t *corner_radius =
		gs_effect_get_param_by_name(effect, "corner_radius");
	gs_effect_set_float(corner_radius, radius);

	set_blending_parameters();

	filter->output_texrender =
		create_or_reset_texrender(filter->output_texrender);

	if (gs_texrender_begin(filter->output_texrender, filter->width,
			       filter->height)) {
		while (gs_effect_loop(effect, "Draw"))
			gs_draw_sprite(texture, 0, filter->width,
				       filter->height);
		gs_texrender_end(filter->output_texrender);
	}
	texture = gs_texrender_get_texture(filter->output_texrender);
	gs_blend_state_pop();
}

static obs_properties_t *composite_blur_properties(void *data)
{
	struct composite_blur_filter_data *filter = data;

	obs_properties_t *props = obs_properties_create();
	obs_properties_set_param(props, filter, NULL);

	obs_property_t *blur_algorithms = obs_properties_add_list(
		props, "blur_algorithm",
		obs_module_text("CompositeBlurFilter.BlurAlgorithm"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);

	obs_property_list_add_int(blur_algorithms,
				  obs_module_text(ALGO_GAUSSIAN_LABEL),
				  ALGO_GAUSSIAN);
	obs_property_list_add_int(blur_algorithms,
				  obs_module_text(ALGO_BOX_LABEL), ALGO_BOX);
	obs_property_list_add_int(blur_algorithms,
				  obs_module_text(ALGO_DUAL_KAWASE_LABEL),
				  ALGO_DUAL_KAWASE);
	obs_property_list_add_int(blur_algorithms,
				  obs_module_text(ALGO_PIXELATE_LABEL),
				  ALGO_PIXELATE);
	obs_property_set_modified_callback2(
		blur_algorithms, setting_blur_algorithm_modified, data);

	obs_property_t *blur_types = obs_properties_add_list(
		props, "blur_type",
		obs_module_text("CompositeBlurFilter.BlurType"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(blur_types, obs_module_text(TYPE_AREA_LABEL),
				  TYPE_AREA);
	obs_property_list_add_int(blur_types,
				  obs_module_text(TYPE_DIRECTIONAL_LABEL),
				  TYPE_DIRECTIONAL);
	obs_property_list_add_int(blur_types, obs_module_text(TYPE_ZOOM_LABEL),
				  TYPE_ZOOM);
	obs_property_list_add_int(
		blur_types, obs_module_text(TYPE_MOTION_LABEL), TYPE_MOTION);
	obs_property_list_add_int(blur_types,
				  obs_module_text(TYPE_TILTSHIFT_LABEL),
				  TYPE_TILTSHIFT);
	obs_property_set_modified_callback2(blur_types,
					    setting_blur_types_modified, data);

	obs_property_t *pixelate_types = obs_properties_add_list(
		props, "pixelate_type",
		obs_module_text("CompositeBlurFilter.PixelateType"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);

	obs_property_list_add_int(pixelate_types,
				  obs_module_text(PIXELATE_TYPE_SQUARE_LABEL),
				  PIXELATE_TYPE_SQUARE);
	obs_property_list_add_int(
		pixelate_types, obs_module_text(PIXELATE_TYPE_HEXAGONAL_LABEL),
		PIXELATE_TYPE_HEXAGONAL);
	obs_property_list_add_int(pixelate_types,
				  obs_module_text(PIXELATE_TYPE_CIRCLE_LABEL),
				  PIXELATE_TYPE_CIRCLE);
	obs_property_list_add_int(pixelate_types,
				  obs_module_text(PIXELATE_TYPE_TRIANGLE_LABEL),
				  PIXELATE_TYPE_TRIANGLE);

	obs_properties_add_float_slider(
		props, "radius", obs_module_text("CompositeBlurFilter.Radius"),
		0.0, 80.1, 0.1);

	obs_properties_add_int_slider(
		props, "passes", obs_module_text("CompositeBlurFilter.Passes"),
		1, 5, 1);

	obs_properties_add_int_slider(
		props, "kawase_passes",
		obs_module_text("CompositeBlurFilter.DualKawase.Passes"), 1,
		1025, 1);

	obs_properties_add_float_slider(
		props, "angle", obs_module_text("CompositeBlurFilter.Angle"),
		-360.0, 360.1, 0.1);

	obs_properties_t *center_coords = obs_properties_create();

	obs_properties_add_float_slider(
		center_coords, "center_x",
		obs_module_text("CompositeBlurFilter.Center.X"), -3840.0,
		7680.0, 1.0);

	obs_properties_add_float_slider(
		center_coords, "center_y",
		obs_module_text("CompositeBlurFilter.Center.Y"), -2160.0,
		4320.0, 1.0);

	obs_properties_add_group(
		props, "center_coordinate",
		obs_module_text("CompositeBlurFilter.CenterCoordinate"),
		OBS_GROUP_NORMAL, center_coords);

	obs_properties_t *tilt_shift_bounds = obs_properties_create();
	obs_properties_add_float_slider(
		tilt_shift_bounds, "tilt_shift_center",
		obs_module_text("CompositeBlurFilter.TiltShift.Center"), 0.0,
		1.0, 0.01);
	obs_properties_add_float_slider(
		tilt_shift_bounds, "tilt_shift_angle",
		obs_module_text("CompositeBlurFilter.TiltShift.Angle"), -180.0,
		181.0, 0.1);
	obs_properties_add_float_slider(
		tilt_shift_bounds, "tilt_shift_width",
		obs_module_text("CompositeBlurFilter.TiltShift.Width"), 0.0,
		1.01, 0.01);

	obs_properties_add_group(
		props, "tilt_shift_bounds",
		obs_module_text("CompositeBlurFilter.TiltShift"),
		OBS_GROUP_NORMAL, tilt_shift_bounds);

	obs_property_t *p = obs_properties_add_list(
		props, "background",
		obs_module_text("CompositeBlurFilter.Background"),
		OBS_COMBO_TYPE_EDITABLE, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(p, "None", "");
	obs_enum_sources(add_source_to_list, p);
	obs_enum_scenes(add_source_to_list, p);

	obs_property_t *effect_mask_list = obs_properties_add_list(
		props, "effect_mask",
		obs_module_text("CompositeBlurFilter.EffectMask"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);

	obs_property_list_add_int(effect_mask_list,
				  obs_module_text(EFFECT_MASK_TYPE_NONE_LABEL),
				  EFFECT_MASK_TYPE_NONE);
	obs_property_list_add_int(effect_mask_list,
				  obs_module_text(EFFECT_MASK_TYPE_CROP_LABEL),
				  EFFECT_MASK_TYPE_CROP);
	obs_property_list_add_int(
		effect_mask_list,
		obs_module_text(EFFECT_MASK_TYPE_SOURCE_LABEL),
		EFFECT_MASK_TYPE_SOURCE);

	obs_property_set_modified_callback(effect_mask_list,
					   setting_effect_mask_modified);

	obs_properties_t *effect_mask_source = obs_properties_create();

	obs_property_t *effect_mask_source_source = obs_properties_add_list(
		effect_mask_source, "effect_mask_source_source",
		obs_module_text("CompositeBlurFilter.EffectMask.Source.Source"),
		OBS_COMBO_TYPE_EDITABLE, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(effect_mask_source_source, "None", "");
	obs_enum_sources(add_source_to_list, effect_mask_source_source);
	obs_enum_scenes(add_source_to_list, effect_mask_source_source);

	obs_property_t *effect_mask_source_filter_list = obs_properties_add_list(
		effect_mask_source, "effect_mask_source_filter_list",
		obs_module_text("CompositeBlurFilter.EffectMask.Source.Filter"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);

	obs_property_list_add_int(
		effect_mask_source_filter_list,
		obs_module_text(EFFECT_MASK_SOURCE_FILTER_ALPHA_LABEL),
		EFFECT_MASK_SOURCE_FILTER_ALPHA);
	obs_property_list_add_int(
		effect_mask_source_filter_list,
		obs_module_text(EFFECT_MASK_SOURCE_FILTER_GRAYSCALE_LABEL),
		EFFECT_MASK_SOURCE_FILTER_GRAYSCALE);
	obs_property_list_add_int(
		effect_mask_source_filter_list,
		obs_module_text(EFFECT_MASK_SOURCE_FILTER_LUMINOSITY_LABEL),
		EFFECT_MASK_SOURCE_FILTER_LUMINOSITY);
	obs_property_list_add_int(
		effect_mask_source_filter_list,
		obs_module_text(EFFECT_MASK_SOURCE_FILTER_SLIDERS_LABEL),
		EFFECT_MASK_SOURCE_FILTER_SLIDERS);

	obs_property_set_modified_callback(
		effect_mask_source_filter_list,
		setting_effect_mask_source_filter_modified);

	obs_properties_add_float_slider(
		effect_mask_source, "effect_mask_source_filter_red",
		obs_module_text("CompositeBlurFilter.Channel.Red"), -100.01,
		100.01, 0.01);

	obs_properties_add_float_slider(
		effect_mask_source, "effect_mask_source_filter_green",
		obs_module_text("CompositeBlurFilter.Channel.Green"), -100.01,
		100.01, 0.01);

	obs_properties_add_float_slider(
		effect_mask_source, "effect_mask_source_filter_blue",
		obs_module_text("CompositeBlurFilter.Channel.Blue"), -100.01,
		100.01, 0.01);

	obs_properties_add_float_slider(
		effect_mask_source, "effect_mask_source_filter_alpha",
		obs_module_text("CompositeBlurFilter.Channel.Alpha"), -100.01,
		100.01, 0.01);

	obs_properties_add_float_slider(
		effect_mask_source, "effect_mask_source_filter_multiplier",
		obs_module_text(
			"CompositeBlurFilter.EffectMask.Source.Multiplier"),
		-100.01, 100.01, 0.01);

	obs_properties_add_bool(
		effect_mask_source, "effect_mask_source_invert",
		obs_module_text("CompositeBlurFilter.EffectMask.Invert"));

	obs_properties_add_group(
		props, "effect_mask_source",
		obs_module_text(
			"CompositeBlurFilter.EffectMask.SourceParameters"),
		OBS_GROUP_NORMAL, effect_mask_source);

	obs_properties_t *effect_mask_crop = obs_properties_create();

	obs_properties_add_float_slider(
		effect_mask_crop, "effect_mask_crop_top",
		obs_module_text("CompositeBlurFilter.EffectMask.Crop.Top"), 0.0,
		100.01, 0.01);

	obs_properties_add_float_slider(
		effect_mask_crop, "effect_mask_crop_bottom",
		obs_module_text("CompositeBlurFilter.EffectMask.Crop.Bottom"),
		0.0, 100.01, 0.01);

	obs_properties_add_float_slider(
		effect_mask_crop, "effect_mask_crop_left",
		obs_module_text("CompositeBlurFilter.EffectMask.Crop.Left"),
		0.0, 100.01, 0.01);

	obs_properties_add_float_slider(
		effect_mask_crop, "effect_mask_crop_right",
		obs_module_text("CompositeBlurFilter.EffectMask.Crop.Right"),
		0.0, 100.01, 0.01);

	obs_properties_add_float_slider(
		effect_mask_crop, "effect_mask_crop_corner_radius",
		obs_module_text("CompositeBlurFilter.EffectMask.CornerRadius"),
		0.0, 50.01, 0.01);

	obs_properties_add_bool(
		effect_mask_crop, "effect_mask_crop_invert",
		obs_module_text("CompositeBlurFilter.EffectMask.Invert"));

	obs_properties_add_group(
		props, "effect_mask_crop",
		obs_module_text(
			"CompositeBlurFilter.EffectMask.CropParameters"),
		OBS_GROUP_NORMAL, effect_mask_crop);

	return props;
}

static bool setting_effect_mask_source_filter_modified(obs_properties_t *props,
						       obs_property_t *p,
						       obs_data_t *settings)
{
	UNUSED_PARAMETER(p);
	int source_filter_type = (int)obs_data_get_int(
		settings, "effect_mask_source_filter_list");
	switch (source_filter_type) {
	case EFFECT_MASK_SOURCE_FILTER_SLIDERS:
		setting_visibility("effect_mask_source_filter_red", true,
				   props);
		setting_visibility("effect_mask_source_filter_green", true,
				   props);
		setting_visibility("effect_mask_source_filter_blue", true,
				   props);
		setting_visibility("effect_mask_source_filter_alpha", true,
				   props);
		break;
	default:
		setting_visibility("effect_mask_source_filter_red", false,
				   props);
		setting_visibility("effect_mask_source_filter_green", false,
				   props);
		setting_visibility("effect_mask_source_filter_blue", false,
				   props);
		setting_visibility("effect_mask_source_filter_alpha", false,
				   props);
		break;
	}
	return true;
}

static bool setting_effect_mask_modified(obs_properties_t *props,
					 obs_property_t *p,
					 obs_data_t *settings)
{
	UNUSED_PARAMETER(p);
	int mask_type = (int)obs_data_get_int(settings, "effect_mask");
	switch (mask_type) {
	case EFFECT_MASK_TYPE_NONE:
		setting_visibility("effect_mask_crop", false, props);
		setting_visibility("effect_mask_source", false, props);
		break;
	case EFFECT_MASK_TYPE_CROP:
		setting_visibility("effect_mask_crop", true, props);
		setting_visibility("effect_mask_source", false, props);
		break;
	case EFFECT_MASK_TYPE_SOURCE:
		setting_visibility("effect_mask_crop", false, props);
		setting_visibility("effect_mask_source", true, props);
		break;
	}
	return true;
}

static void effect_mask_load_effect(composite_blur_filter_data_t *filter)
{
	switch (filter->mask_type) {
	case EFFECT_MASK_TYPE_CROP:
		load_crop_mask_effect(filter);
		break;
	case EFFECT_MASK_TYPE_SOURCE:
		load_source_mask_effect(filter);
		break;
	}
}

static bool setting_blur_algorithm_modified(void *data, obs_properties_t *props,
					    obs_property_t *p,
					    obs_data_t *settings)
{
	UNUSED_PARAMETER(p);
	UNUSED_PARAMETER(data);
	int blur_algorithm = (int)obs_data_get_int(settings, "blur_algorithm");
	switch (blur_algorithm) {
	case ALGO_GAUSSIAN:
		setting_visibility("radius", true, props);
		setting_visibility("passes", false, props);
		setting_visibility("kawase_passes", false, props);
		setting_visibility("blur_type", true, props);
		setting_visibility("pixelate_type", false, props);
		set_blur_radius_settings(
			obs_module_text("CompositeBlurFilter.Radius"), 0.0f,
			80.01f, 0.1f, props);
		set_gaussian_blur_types(props);
		break;
	case ALGO_BOX:
		setting_visibility("radius", true, props);
		setting_visibility("kawase_passes", false, props);
		setting_visibility("passes", true, props);
		setting_visibility("blur_type", true, props);
		setting_visibility("pixelate_type", false, props);
		set_blur_radius_settings(
			obs_module_text("CompositeBlurFilter.Radius"), 0.0f,
			100.01f, 0.1f, props);
		set_box_blur_types(props);
		break;
	case ALGO_DUAL_KAWASE:
		setting_visibility("radius", false, props);
		setting_visibility("passes", false, props);
		setting_visibility("kawase_passes", true, props);
		setting_visibility("blur_type", false, props);
		setting_visibility("pixelate_type", false, props);
		set_dual_kawase_blur_types(props);
		obs_data_set_int(settings, "blur_type", TYPE_AREA);
		settings_blur_area(props, settings);
		break;
	case ALGO_PIXELATE:
		setting_visibility("radius", true, props);
		setting_visibility("passes", false, props);
		setting_visibility("kawase_passes", false, props);
		setting_visibility("blur_type", false, props);
		setting_visibility("pixelate_type", true, props);
		set_blur_radius_settings(
			obs_module_text(
				"CompositeBlurFilter.Pixelate.PixelSize"),
			1.0f, 1024.01f, 0.1f, props);
		set_pixelate_blur_types(props);
		obs_data_set_int(settings, "blur_type", TYPE_AREA);
		settings_blur_area(props, settings);

		break;
	}
	return true;
}

static bool setting_blur_types_modified(void *data, obs_properties_t *props,
					obs_property_t *p, obs_data_t *settings)
{
	UNUSED_PARAMETER(p);
	UNUSED_PARAMETER(data);
	int blur_type = (int)obs_data_get_int(settings, "blur_type");
	if (blur_type == TYPE_AREA) {
		return settings_blur_area(props, settings);
	} else if (blur_type == TYPE_DIRECTIONAL) {
		return settings_blur_directional(props);
	} else if (blur_type == TYPE_ZOOM) {
		return settings_blur_zoom(props);
	} else if (blur_type == TYPE_MOTION) {
		return settings_blur_directional(props);
	} else if (blur_type == TYPE_TILTSHIFT) {
		return settings_blur_tilt_shift(props);
	}
	return true;
}

static void setting_visibility(const char *prop_name, bool visible,
			       obs_properties_t *props)
{
	obs_property_t *p = obs_properties_get(props, prop_name);
	obs_property_set_enabled(p, visible);
	obs_property_set_visible(p, visible);
}

static void set_blur_radius_settings(const char *name, float min_val,
				     float max_val, float step_size,
				     obs_properties_t *props)
{
	obs_property_t *p = obs_properties_get(props, "radius");
	obs_property_set_description(p, name);
	obs_property_float_set_limits(p, (double)min_val, (double)max_val,
				      (double)step_size);
}

static bool settings_blur_area(obs_properties_t *props, obs_data_t *settings)
{
	int algorithm = (int)obs_data_get_int(settings, "blur_algorithm");
	setting_visibility("radius", algorithm != ALGO_DUAL_KAWASE, props);
	setting_visibility("angle", false, props);
	setting_visibility("center_coordinate", false, props);
	setting_visibility("background", true, props);
	setting_visibility("tilt_shift_bounds", false, props);
	return true;
}

static bool settings_blur_directional(obs_properties_t *props)
{
	setting_visibility("radius", true, props);
	setting_visibility("angle", true, props);
	setting_visibility("center_coordinate", false, props);
	setting_visibility("background", true, props);
	setting_visibility("tilt_shift_bounds", false, props);
	return true;
}

static bool settings_blur_zoom(obs_properties_t *props)
{
	setting_visibility("radius", true, props);
	setting_visibility("angle", false, props);
	setting_visibility("center_coordinate", true, props);
	setting_visibility("background", true, props);
	setting_visibility("tilt_shift_bounds", false, props);
	return true;
}

static bool settings_blur_tilt_shift(obs_properties_t *props)
{
	setting_visibility("radius", true, props);
	setting_visibility("angle", false, props);
	setting_visibility("center_coordinate", false, props);
	setting_visibility("background", true, props);
	setting_visibility("tilt_shift_bounds", true, props);
	return true;
}

static void composite_blur_video_tick(void *data, float seconds)
{
	UNUSED_PARAMETER(seconds);
	composite_blur_filter_data_t *filter = data;
	obs_source_t *target = obs_filter_get_target(filter->context);
	if (!target) {
		return;
	}
	filter->width = (uint32_t)obs_source_get_base_width(target);
	filter->height = (uint32_t)obs_source_get_base_height(target);
	filter->uv_size.x = (float)filter->width;
	filter->uv_size.y = (float)filter->height;
	filter->rendered = false;
}

static void composite_blur_reload_effect(composite_blur_filter_data_t *filter)
{
	filter->reload = false;
	obs_data_t *settings = obs_source_get_settings(filter->context);
	filter->param_uv_size = NULL;

	if (filter->blur_algorithm == ALGO_GAUSSIAN) {
		gaussian_setup_callbacks(filter);
	} else if (filter->blur_algorithm == ALGO_BOX) {
		box_setup_callbacks(filter);
	} else if (filter->blur_algorithm == ALGO_PIXELATE) {
		pixelate_setup_callbacks(filter);
	} else if (filter->blur_algorithm == ALGO_DUAL_KAWASE) {
		dual_kawase_setup_callbacks(filter);
	}

	if (filter->load_effect) {
		filter->load_effect(filter);
		load_composite_effect(filter);
		load_mix_effect(filter);
	}

	obs_data_release(settings);
}

static void load_composite_effect(composite_blur_filter_data_t *filter)
{
	if (filter->composite_effect != NULL) {
		obs_enter_graphics();
		gs_effect_destroy(filter->composite_effect);
		filter->composite_effect = NULL;
		obs_leave_graphics();
	}

	char *shader_text = NULL;
	struct dstr filename = {0};
	dstr_cat(&filename, obs_get_module_data_path(obs_current_module()));
	dstr_cat(&filename, "/shaders/composite.effect");
	shader_text = load_shader_from_file(filename.array);
	char *errors = NULL;

	obs_enter_graphics();
	filter->composite_effect = gs_effect_create(shader_text, NULL, &errors);
	obs_leave_graphics();

	bfree(shader_text);
	if (filter->composite_effect == NULL) {
		blog(LOG_WARNING,
		     "[obs-composite-blur] Unable to load composite.effect file.  Errors:\n%s",
		     (errors == NULL || strlen(errors) == 0 ? "(None)"
							    : errors));
		bfree(errors);
	} else {
		size_t effect_count =
			gs_effect_get_num_params(filter->composite_effect);
		for (size_t effect_index = 0; effect_index < effect_count;
		     effect_index++) {
			gs_eparam_t *param = gs_effect_get_param_by_idx(
				filter->composite_effect, effect_index);
			struct gs_effect_param_info info;
			gs_effect_get_param_info(param, &info);
			if (strcmp(info.name, "background") == 0) {
				filter->param_background = param;
			}
		}
	}
}

static void load_crop_mask_effect(composite_blur_filter_data_t *filter)
{
	if (filter->effect_mask_effect != NULL) {
		obs_enter_graphics();
		gs_effect_destroy(filter->effect_mask_effect);
		filter->effect_mask_effect = NULL;
		obs_leave_graphics();
	}

	char *shader_text = NULL;
	struct dstr filename = {0};
	dstr_cat(&filename, obs_get_module_data_path(obs_current_module()));
	dstr_cat(&filename, "/shaders/effect_mask_crop.effect");
	shader_text = load_shader_from_file(filename.array);
	char *errors = NULL;

	obs_enter_graphics();
	filter->effect_mask_effect =
		gs_effect_create(shader_text, NULL, &errors);
	obs_leave_graphics();

	bfree(shader_text);
	if (filter->effect_mask_effect == NULL) {
		blog(LOG_WARNING,
		     "[obs-composite-blur] Unable to load effect_mask_crop.effect file.  Errors:\n%s",
		     (errors == NULL || strlen(errors) == 0 ? "(None)"
							    : errors));
		bfree(errors);
	} else {
		size_t effect_count =
			gs_effect_get_num_params(filter->effect_mask_effect);
		for (size_t effect_index = 0; effect_index < effect_count;
		     effect_index++) {
			gs_eparam_t *param = gs_effect_get_param_by_idx(
				filter->effect_mask_effect, effect_index);
			struct gs_effect_param_info info;
			gs_effect_get_param_info(param, &info);
		}
	}
}

static void load_source_mask_effect(composite_blur_filter_data_t *filter)
{
	if (filter->effect_mask_effect != NULL) {
		obs_enter_graphics();
		gs_effect_destroy(filter->effect_mask_effect);
		filter->effect_mask_effect = NULL;
		obs_leave_graphics();
	}

	char *shader_text = NULL;
	struct dstr filename = {0};
	dstr_cat(&filename, obs_get_module_data_path(obs_current_module()));
	dstr_cat(&filename, "/shaders/effect_mask_source.effect");
	shader_text = load_shader_from_file(filename.array);
	char *errors = NULL;

	obs_enter_graphics();
	filter->effect_mask_effect =
		gs_effect_create(shader_text, NULL, &errors);
	obs_leave_graphics();

	bfree(shader_text);
	if (filter->effect_mask_effect == NULL) {
		blog(LOG_WARNING,
		     "[obs-composite-blur] Unable to load effect_mask_crop.effect file.  Errors:\n%s",
		     (errors == NULL || strlen(errors) == 0 ? "(None)"
							    : errors));
		bfree(errors);
	} else {
		size_t effect_count =
			gs_effect_get_num_params(filter->effect_mask_effect);
		for (size_t effect_index = 0; effect_index < effect_count;
		     effect_index++) {
			gs_eparam_t *param = gs_effect_get_param_by_idx(
				filter->effect_mask_effect, effect_index);
			struct gs_effect_param_info info;
			gs_effect_get_param_info(param, &info);
		}
	}
}

static void load_mix_effect(composite_blur_filter_data_t *filter)
{
	if (filter->mix_effect != NULL) {
		obs_enter_graphics();
		gs_effect_destroy(filter->mix_effect);
		filter->mix_effect = NULL;
		obs_leave_graphics();
	}

	char *shader_text = NULL;
	struct dstr filename = {0};
	dstr_cat(&filename, obs_get_module_data_path(obs_current_module()));
	dstr_cat(&filename, "/shaders/mix.effect");
	shader_text = load_shader_from_file(filename.array);
	char *errors = NULL;

	obs_enter_graphics();
	filter->mix_effect = gs_effect_create(shader_text, NULL, &errors);
	obs_leave_graphics();

	bfree(shader_text);
	if (filter->mix_effect == NULL) {
		blog(LOG_WARNING,
		     "[obs-composite-blur] Unable to load mix.effect file.  Errors:\n%s",
		     (errors == NULL || strlen(errors) == 0 ? "(None)"
							    : errors));
		bfree(errors);
	} else {
		size_t effect_count =
			gs_effect_get_num_params(filter->mix_effect);
		for (size_t effect_index = 0; effect_index < effect_count;
		     effect_index++) {
			gs_eparam_t *param = gs_effect_get_param_by_idx(
				filter->mix_effect, effect_index);
			struct gs_effect_param_info info;
			gs_effect_get_param_info(param, &info);
			if (strcmp(info.name, "background") == 0) {
				filter->param_background = param;
			}
		}
	}
}

gs_texture_t *blend_composite(gs_texture_t *texture,
			      struct composite_blur_filter_data *data)
{
	// Get source
	obs_source_t *source =
		data->background ? obs_weak_source_get_source(data->background)
				 : NULL;

	gs_effect_t *composite_effect = data->composite_effect;
	if (source) {
		const enum gs_color_space preferred_spaces[] = {
			GS_CS_SRGB,
			GS_CS_SRGB_16F,
			GS_CS_709_EXTENDED,
		};
		const enum gs_color_space space = obs_source_get_color_space(
			source, OBS_COUNTOF(preferred_spaces),
			preferred_spaces);
		const enum gs_color_format format =
			gs_get_format_from_space(space);

		// Set up a tex renderer for source
		gs_texrender_t *source_render =
			gs_texrender_create(format, GS_ZS_NONE);
		uint32_t base_width = obs_source_get_base_width(source);
		uint32_t base_height = obs_source_get_base_height(source);
		gs_blend_state_push();
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);
		if (gs_texrender_begin_with_color_space(
			    source_render, base_width, base_height, space)) {
			const float w = (float)base_width;
			const float h = (float)base_height;
			uint32_t flags = obs_source_get_output_flags(source);
			const bool custom_draw =
				(flags & OBS_SOURCE_CUSTOM_DRAW) != 0;
			const bool async = (flags & OBS_SOURCE_ASYNC) != 0;
			struct vec4 clear_color;

			vec4_zero(&clear_color);
			gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
			gs_ortho(0.0f, w, 0.0f, h, -100.0f, 100.0f);

			if (!custom_draw && !async)
				obs_source_default_render(source);
			else
				obs_source_video_render(source);
			gs_texrender_end(source_render);
		}
		gs_blend_state_pop();
		obs_source_release(source);
		gs_texture_t *tex = gs_texrender_get_texture(source_render);

		gs_eparam_t *background = gs_effect_get_param_by_name(
			composite_effect, "background");
		gs_effect_set_texture(background, tex);
		gs_eparam_t *image =
			gs_effect_get_param_by_name(composite_effect, "image");
		gs_effect_set_texture(image, texture);

		data->composite_render =
			create_or_reset_texrender(data->composite_render);
		set_blending_parameters();
		if (gs_texrender_begin(data->composite_render, data->width,
				       data->height)) {
			while (gs_effect_loop(composite_effect, "Draw"))
				gs_draw_sprite(texture, 0, data->width,
					       data->height);
			gs_texrender_end(data->composite_render);
		}
		texture = gs_texrender_get_texture(data->composite_render);
		gs_texrender_destroy(source_render);
		gs_blend_state_pop();
	}
	return texture;
}