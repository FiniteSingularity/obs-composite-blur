#include "obs-composite-blur-filter.h"
#include <util/dstr.h>

#include "blur/gaussian.h"
#include "blur/box.h"
#include "blur/pixelate.h"
#include "blur/dual_kawase.h"
#include "blur/temporal.h"

struct obs_source_info obs_composite_blur = {
	.id = "obs_composite_blur",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_SRGB,
	.get_name = composite_blur_name,
	.create = composite_blur_create,
	.destroy = composite_blur_destroy,
	.update = composite_blur_update,
	.video_render = composite_blur_video_render,
	.video_tick = composite_blur_video_tick,
	.get_width = composite_blur_width,
	.get_height = composite_blur_height,
	.get_properties = composite_blur_properties,
	.get_defaults = composite_blur_defaults};

static const char *composite_blur_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "Composite Blur";
	//return obs_module_text("CompositeBlurFilter");
}

static void composite_blur_defaults(obs_data_t *settings)
{
	obs_data_set_default_double(settings, "radius", 10.0);
	obs_data_set_default_string(
		settings, "background",
		obs_module_text("CompositeBlurFilter.Background.None"));
	obs_data_set_default_int(settings, "passes", 1);
	obs_data_set_default_int(settings, "kawase_passes", 10);
	obs_data_set_default_string(
		settings, "effect_mask_source_source",
		obs_module_text("CompositeBlurFilter.EffectMask.Source.None"));
	obs_data_set_default_double(
		settings, "effect_mask_source_filter_multiplier", 1.0);
	obs_data_set_default_double(settings, "pixelate_origin_x", -1.e9);
	obs_data_set_default_double(settings, "pixelate_origin_y", -1.e9);
	obs_data_set_default_double(settings, "pixelate_animation_speed", 50.0);
	obs_data_set_default_double(settings, "effect_mask_circle_center_x",
				    50.0);
	obs_data_set_default_double(settings, "effect_mask_circle_center_y",
				    50.0);
	obs_data_set_default_double(settings, "effect_mask_circle_radius",
				    40.0);

	obs_data_set_default_double(settings, "effect_mask_rect_center_x",
				    50.0);
	obs_data_set_default_double(settings, "effect_mask_rect_center_y",
				    50.0);
	obs_data_set_default_double(settings, "effect_mask_rect_width", 50.0);
	obs_data_set_default_double(settings, "effect_mask_rect_height", 50.0);

	obs_data_set_default_double(settings, "effect_mask_crop_top", 20.0);
	obs_data_set_default_double(settings, "effect_mask_crop_bottom", 20.0);
	obs_data_set_default_double(settings, "effect_mask_crop_left", 20.0);
	obs_data_set_default_double(settings, "effect_mask_crop_right", 20.0);
	obs_data_set_default_string(settings, "vector_source", "");
	obs_data_set_default_double(settings, "temporal_current_weight", 0.95);
	obs_data_set_default_double(settings, "temporal_clear_threshold", 5.0);

}

static void *composite_blur_create(obs_data_t *settings, obs_source_t *source)
{
	composite_blur_filter_data_t *filter =
		bzalloc(sizeof(composite_blur_filter_data_t));
	dstr_init_copy(&filter->filter_name, "");
	dstr_init_copy(&filter->mask_source_name, "");
	dstr_init_copy(&filter->background_source_name, "");

	filter->context = source;
	signal_handler_t *sh = obs_source_get_signal_handler(filter->context);
	signal_handler_connect_ref(sh, "rename", composite_blur_rename, filter);
	filter->hotkey = OBS_INVALID_HOTKEY_PAIR_ID;
	filter->radius = 0.0f;
	filter->inactive_radius = 0.0f;
	filter->radius_last = -1.0f;
	filter->time = 0.0f;
	filter->angle = 0.0f;
	filter->passes = 1;
	filter->center_x = 0.0f;
	filter->center_y = 0.0f;
	filter->blur_algorithm = ALGO_NONE;
	filter->blur_algorithm_last = -1;
	filter->blur_type = TYPE_NONE;
	filter->blur_type_last = -1;
	filter->kawase_passes = 1.0;
	filter->rendering = false;
	filter->reload = true;
	filter->video_render = NULL;
	filter->load_effect = NULL;
	filter->update = NULL;
	filter->kernel_texture = NULL;
	filter->pixelate_type = 1;
	filter->pixelate_type_last = -1;

	filter->temporal_prior_stored = false;

	filter->has_background_source = false;

	// Params
	filter->param_uv_size = NULL;
	filter->param_radius = NULL;
	filter->param_texel_step = NULL;
	filter->param_kernel_size = NULL;
	filter->param_offset = NULL;
	filter->param_weight = NULL;
	filter->param_kernel_texture = NULL;
	filter->param_radial_center = NULL;
	filter->param_focus_width = NULL;
	filter->param_focus_center = NULL;
	filter->param_focus_angle = NULL;
	filter->param_background = NULL;
	filter->param_pixel_size = NULL;

	filter->param_pixel_center = NULL;
	filter->param_pixel_rot = NULL;
	filter->param_pixel_cos_theta = NULL;
	filter->param_pixel_sin_theta = NULL;
	filter->param_pixel_cos_rtheta = NULL;
	filter->param_pixel_sin_rtheta = NULL;
	filter->param_mask_crop_scale = NULL;
	filter->param_mask_crop_offset = NULL;
	filter->param_mask_crop_box_aspect_ratio = NULL;
	filter->param_mask_crop_corner_radius = NULL;
	filter->param_mask_crop_feathering = NULL;
	filter->param_mask_crop_invert = NULL;
	filter->param_mask_source_alpha_source = NULL;
	filter->param_mask_source_rgba_weights = NULL;
	filter->param_mask_source_multiplier = NULL;
	filter->param_mask_source_invert = NULL;
	filter->param_mask_circle_center = NULL;
	filter->param_mask_circle_radius = NULL;
	filter->param_mask_circle_feathering = NULL;
	filter->param_mask_circle_inv = NULL;
	filter->param_mask_circle_uv_scale = NULL;
	filter->param_temporal_image = NULL;
	filter->param_temporal_prior_image = NULL;
	filter->param_temporal_current_weight = NULL;
	filter->param_temporal_clear_threshold = NULL;

	filter->mask_image = NULL;

	filter->mask_crop_left = 0.0f;
	filter->mask_crop_right = 0.0f;
	filter->mask_crop_top = 0.0f;
	filter->mask_crop_bot = 0.0f;
	filter->mask_type = 0;
	filter->mask_type_last = -1;
	filter->mask_crop_corner_radius = 0.0f;
	filter->mask_crop_feathering = 0.0;
	filter->mask_crop_invert = false;

	filter->mask_source_filter_type = EFFECT_MASK_SOURCE_FILTER_ALPHA;
	filter->mask_source_filter_red = 0.0;
	filter->mask_source_filter_green = 0.0;
	filter->mask_source_filter_blue = 0.0;
	filter->mask_source_filter_alpha = 0.0;
	filter->mask_source_multiplier = 1.0;
	filter->mask_source_source = NULL;
	filter->mask_source_invert = false;

	filter->param_output_image = NULL;

	da_init(filter->kernel);
	//composite_blur_defaults(settings);
	obs_source_update(source, settings);
	obs_enter_graphics();
	// Grab the device type, can be:
	// GS_DEVICE_OPENGL
	// GS_DEVICE_DIRECT3D_11
	filter->device_type = gs_get_device_type();
	obs_leave_graphics();
	return filter;
}

static void composite_blur_destroy(void *data)
{
	composite_blur_filter_data_t *filter = data;

	signal_handler_t *sh = obs_source_get_signal_handler(filter->context);
	signal_handler_disconnect(sh, "rename", composite_blur_rename, filter);

	dstr_free(&filter->filter_name);
	dstr_free(&filter->mask_source_name);
	dstr_free(&filter->background_source_name);

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
	if (filter->pixelate_effect) {
		gs_effect_destroy(filter->pixelate_effect);
	}
	if (filter->output_effect) {
		gs_effect_destroy(filter->output_effect);
	}
	if (filter->gradient_effect) {
		gs_effect_destroy(filter->gradient_effect);
	}
	if (filter->gv_effect) {
		gs_effect_destroy(filter->gv_effect);
	}

	if (filter->render) {
		gs_texrender_destroy(filter->render);
	}
	if (filter->render2) {
		gs_texrender_destroy(filter->render2);
	}
	if (filter->background_texrender) {
		gs_texrender_destroy(filter->background_texrender);
	}
	if (filter->input_texrender) {
		gs_texrender_destroy(filter->input_texrender);
	}
	if (filter->output_texrender) {
		gs_texrender_destroy(filter->output_texrender);
	}
	if (filter->composite_render) {
		gs_texrender_destroy(filter->composite_render);
	}
	if (filter->pixelate_texrender) {
		gs_texrender_destroy(filter->pixelate_texrender);
	}

	if (filter->kernel_texture) {
		gs_texture_destroy(filter->kernel_texture);
	}
	if (filter->mask_image) {
		gs_image_file_free(filter->mask_image);
		bfree(filter->mask_image);
	}

	if (filter->background) {
		obs_weak_source_release(filter->background);
	}

	if (filter->mask_source_source) {
		obs_weak_source_release(filter->mask_source_source);
	}

	if (filter->hotkey != OBS_INVALID_HOTKEY_PAIR_ID) {
		obs_hotkey_pair_unregister(filter->hotkey);
	}

	da_free(filter->offset);
	da_free(filter->kernel);

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

static void composite_blur_rename(void *data, calldata_t *call_data)
{
	composite_blur_filter_data_t *filter = data;
	const char *new_name = calldata_string(call_data, "new_name");

	dstr_copy(&filter->filter_name, new_name);
	struct dstr enable;
	struct dstr disable;

	dstr_init_copy(&enable, obs_module_text("CompositeBlurFilter.Enable"));
	dstr_init_copy(&disable,
		       obs_module_text("CompositeBlurFilter.Disable"));
	dstr_cat(&enable, " ");
	dstr_cat(&enable, new_name);
	dstr_cat(&disable, " ");
	dstr_cat(&disable, new_name);
	obs_hotkey_pair_set_descriptions(filter->hotkey, enable.array,
					 disable.array);
	dstr_free(&enable);
	dstr_free(&disable);
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

	filter->temporal_current_weight =
		1.0f - (float)obs_data_get_double(settings, "temporal_current_weight") * 0.94f;

	filter->temporal_clear_threshold = (float)obs_data_get_double(settings, "temporal_clear_threshold") / 100.0f;

	if (filter->pixelate_type != filter->pixelate_type_last) {
		filter->pixelate_type_last = filter->pixelate_type;
		filter->reload = true;
	}

	filter->pixelate_smoothing_pct =
		(float)obs_data_get_double(settings, "pixelate_smoothing_pct");


	filter->pixelate_tessel_center.x = (float)obs_data_get_double(settings, "pixelate_origin_x");
	filter->pixelate_tessel_center.y = (float)obs_data_get_double(settings, "pixelate_origin_y");

	filter->pixelate_animate = obs_data_get_bool(settings, "pixelate_animate");
	filter->pixelate_animation_speed = (float)obs_data_get_double(settings, "pixelate_animation_speed")/100.0f;
	filter->pixelate_animation_time = (float)obs_data_get_double(settings, "pixelate_time");

	const double theta =
		M_PI *
		(float)obs_data_get_double(settings, "pixelate_rotation") /
		180.0;
	filter->pixelate_cos_theta = (float)cos(theta);
	filter->pixelate_sin_theta = (float)sin(theta);
	filter->pixelate_sin_rtheta = (float)sin(-theta);
	filter->pixelate_cos_rtheta = (float)cos(-theta);

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
	filter->mask_crop_feathering = (float)obs_data_get_double(
		settings, "effect_mask_crop_feathering");
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

	if (filter->mask_source_source) {
		obs_weak_source_release(filter->mask_source_source);
	}

	const char *mask_source_name =
		obs_data_get_string(settings, "effect_mask_source_source");

	filter->has_mask_source = mask_source_name && strlen(mask_source_name);

	if (filter->has_mask_source) {
		dstr_copy(&filter->mask_source_name, mask_source_name);
	} else {
		dstr_copy(&filter->mask_source_name, "");
	}

	obs_source_t *mask_source =
			filter->has_mask_source
			? obs_get_source_by_name(mask_source_name)
			: NULL;

	if (mask_source) {
		filter->mask_source_source =
			obs_source_get_weak_source(mask_source);
		obs_source_release(mask_source);
	} else {
		filter->mask_source_source = NULL;
	}

	const char *mask_image_file =
		obs_data_get_string(settings, "effect_mask_source_file");

	if (filter->mask_image == NULL) {
		filter->mask_image = bzalloc(sizeof(gs_image_file_t));
	} else {
		obs_enter_graphics();
		gs_image_file_free(filter->mask_image);
		obs_leave_graphics();
	}
	if (strlen(mask_image_file)) {
		gs_image_file_init(filter->mask_image, mask_image_file);
		obs_enter_graphics();
		gs_image_file_init_texture(filter->mask_image);
		obs_leave_graphics();
	}
	filter->mask_source_multiplier = (float)obs_data_get_double(
		settings, "effect_mask_source_filter_multiplier");

	filter->mask_source_invert =
		obs_data_get_bool(settings, "effect_mask_source_invert");

	filter->mask_circle_center_x = (float)obs_data_get_double(
		settings, "effect_mask_circle_center_x");
	filter->mask_circle_center_y = (float)obs_data_get_double(
		settings, "effect_mask_circle_center_y");
	filter->mask_circle_radius = (float)obs_data_get_double(
		settings, "effect_mask_circle_radius");
	filter->mask_circle_feathering = (float)obs_data_get_double(
		settings, "effect_mask_circle_feathering");
	filter->mask_circle_inv =
		obs_data_get_bool(settings, "effect_mask_circle_invert");

	filter->mask_rect_center_x = (float)obs_data_get_double(
		settings, "effect_mask_rect_center_x");
	filter->mask_rect_center_y = (float)obs_data_get_double(
		settings, "effect_mask_rect_center_y");
	filter->mask_rect_width =
		(float)obs_data_get_double(settings, "effect_mask_rect_width");
	filter->mask_rect_height =
		(float)obs_data_get_double(settings, "effect_mask_rect_height");
	filter->mask_rect_corner_radius = (float)obs_data_get_double(
		settings, "effect_mask_rect_corner_radius");
	filter->mask_rect_feathering = (float)obs_data_get_double(
		settings, "effect_mask_rect_feathering");
	filter->mask_rect_inv =
		obs_data_get_bool(settings, "effect_mask_rect_invert");

	filter->radius = (float)obs_data_get_double(settings, "radius");
	filter->passes = (int)obs_data_get_int(settings, "passes");
	filter->kawase_passes =
		(float)obs_data_get_double(settings, "kawase_passes");

	filter->center_x = (float)obs_data_get_double(settings, "center_x");
	filter->center_y = (float)obs_data_get_double(settings, "center_y");
	filter->inactive_radius = (float)obs_data_get_double(settings, "inactive_radius");

	filter->angle = (float)obs_data_get_double(settings, "angle");
	filter->tilt_shift_center =
		(float)obs_data_get_double(settings, "tilt_shift_center");
	filter->tilt_shift_width =
		(float)obs_data_get_double(settings, "tilt_shift_width");
	filter->tilt_shift_angle =
		(float)obs_data_get_double(settings, "tilt_shift_angle");

	filter->vector_blur_channel = (int)obs_data_get_int(settings, "vector_blur_channel");
	filter->vector_blur_amount = (float)obs_data_get_double(settings, "vector_blur_amount");
	filter->vector_blur_smoothing = (float)obs_data_get_double(settings, "vector_blur_smoothing");

	filter->vector_blur_type = (int)obs_data_get_int(settings, "vector_gradient_type");

	const char* vector_source_name =
		obs_data_get_string(settings, "vector_source");
	obs_source_t* vector_source =
		(vector_source_name && strlen(vector_source_name))
		? obs_get_source_by_name(vector_source_name)
		: NULL;
	if (filter->vector_blur_source)
		obs_weak_source_release(filter->vector_blur_source);
	if (vector_source) {
		filter->vector_blur_source =
			obs_source_get_weak_source(vector_source);
		obs_source_release(vector_source);
	}
	else {
		filter->vector_blur_source = NULL;
	}


	const char *source_name = obs_data_get_string(settings, "background");

	filter->has_background_source = (source_name && strlen(source_name));

	obs_source_t *source = filter->has_background_source
				       ? obs_get_source_by_name(source_name)
				       : NULL;

	if (filter->has_background_source) {
		dstr_copy(&filter->background_source_name, source_name);
	} else {
		dstr_copy(&filter->background_source_name, "");
	}

	if (filter->background) {
		obs_weak_source_release(filter->background);
	}
	if (source) {
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
	// Use the OBS default effect file as our effect.
	gs_effect_t *pass_through = obs_get_base_effect(OBS_EFFECT_DEFAULT);

	// Set up our color space info.
	const enum gs_color_space preferred_spaces[] = {
		GS_CS_SRGB,
		GS_CS_SRGB_16F,
		GS_CS_709_EXTENDED,
	};

	const enum gs_color_space source_space = obs_source_get_color_space(
		obs_filter_get_target(filter->context),
		OBS_COUNTOF(preferred_spaces), preferred_spaces);

	const enum gs_color_format format =
		gs_get_format_from_space(source_space);

	// Set up our input_texrender to catch the output texture.
	filter->input_texrender =
		create_or_reset_texrender(filter->input_texrender);

	// Start the rendering process with our correct color space params,
	// And set up your texrender to recieve the created texture.
	if (obs_source_process_filter_begin_with_color_space(
		    filter->context, format, source_space,
		    OBS_ALLOW_DIRECT_RENDERING) &&
	    gs_texrender_begin(filter->input_texrender, filter->width,
			       filter->height)) {

		set_blending_parameters();
		gs_ortho(0.0f, (float)filter->width, 0.0f,
			 (float)filter->height, -100.0f, 100.0f);
		// The incoming source is pre-multiplied alpha, so use the
		// OBS default effect "DrawAlphaDivide" technique to convert
		// the colors back into non-pre-multiplied space.
		obs_source_process_filter_tech_end(filter->context,
						   pass_through, filter->width,
						   filter->height,
						   "DrawAlphaDivide");
		gs_texrender_end(filter->input_texrender);
		gs_blend_state_pop();
	}
}

static void draw_output_to_source(composite_blur_filter_data_t *filter)
{
	const enum gs_color_space preferred_spaces[] = {
		GS_CS_SRGB,
		GS_CS_SRGB_16F,
		GS_CS_709_EXTENDED,
	};

	const enum gs_color_space source_space = obs_source_get_color_space(
		obs_filter_get_target(filter->context),
		OBS_COUNTOF(preferred_spaces), preferred_spaces);

	const enum gs_color_format format =
		gs_get_format_from_space(source_space);

	if (!obs_source_process_filter_begin_with_color_space(
		    filter->context, format, source_space,
		    OBS_ALLOW_DIRECT_RENDERING)) {
		return;
	}

	gs_texture_t *texture =
		gs_texrender_get_texture(filter->output_texrender);
	gs_effect_t *pass_through = filter->output_effect;

	if (filter->param_output_image) {
		gs_effect_set_texture(filter->param_output_image, texture);
	}

	gs_blend_state_push();
	gs_blend_function_separate(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA,
				   GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);

	obs_source_process_filter_end(filter->context, pass_through,
				      filter->width, filter->height);
	gs_blend_state_pop();
}

static void composite_blur_video_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);
	struct composite_blur_filter_data *filter = data;

	if (filter->rendered) {
		draw_output_to_source(filter);
		return;
	}

	if (filter->rendering || filter->width == 0 || filter->height == 0) {
		obs_source_skip_video_filter(filter->context);
		return;
	}

	filter->rendering = true;

	if (filter->video_render) {
		// 1. Get the input source as a texture renderer
		//    accessed as filter->input_texrender after call
		get_input_source(filter);

		// 2. Apply effect to texture, and render texture to video
		filter->video_render(filter);

		// 3. Apply mask to texture if one is selected by the user.
		if (filter->mask_type != EFFECT_MASK_TYPE_NONE) {
			// Swap output and render
			apply_effect_mask(filter);
		}

		// 4. Draw result (filter->output_texrender) to source
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
	case EFFECT_MASK_TYPE_CIRCLE:
		apply_effect_mask_circle(filter);
		break;
	case EFFECT_MASK_TYPE_RECT:
		apply_effect_mask_rect(filter);
		break;
	case EFFECT_MASK_TYPE_IMAGE:
		apply_effect_mask_source(filter);
		break;
	}
}

static void apply_effect_mask_source(composite_blur_filter_data_t *filter)
{
	// Get source
	gs_texture_t *alpha_texture = NULL;
	gs_texrender_t *source_render = NULL;
	if (filter->mask_type == EFFECT_MASK_TYPE_SOURCE) {
		obs_source_t *source =
			filter->mask_source_source
				? obs_weak_source_get_source(
					  filter->mask_source_source)
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
		alpha_texture = gs_texrender_get_texture(source_render);
	} else if (filter->mask_type == EFFECT_MASK_TYPE_IMAGE &&
		   filter->mask_image) {
		alpha_texture = filter->mask_image->texture;
	}

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
		gs_texrender_destroy(source_render);
		return;
	}
	gs_eparam_t *image = gs_effect_get_param_by_name(effect, "image");
	gs_effect_set_texture(image, texture);

	if (filter->param_filtered_image) {
		gs_effect_set_texture(filter->param_filtered_image,
				      filtered_texture);
	}

	if (filter->param_mask_source_alpha_source) {
		gs_effect_set_texture(filter->param_mask_source_alpha_source,
				      alpha_texture);
	}

	if (filter->param_mask_source_invert) {
		gs_effect_set_bool(filter->param_mask_source_invert,
				   filter->mask_source_invert);
	}

	// TODO- Move weights calculation to update.
	//       Move vec4 weights into data structure.
	struct vec4 weights;
	weights.x = filter->mask_source_filter_red;
	weights.y = filter->mask_source_filter_green;
	weights.z = filter->mask_source_filter_blue;
	weights.w = filter->mask_source_filter_alpha;
	if (filter->param_mask_source_rgba_weights) {
		gs_effect_set_vec4(filter->param_mask_source_rgba_weights,
				   &weights);
	}

	if (filter->param_mask_source_multiplier) {
		gs_effect_set_float(filter->param_mask_source_multiplier,
				    filter->mask_source_multiplier);
	}
	set_blending_parameters();

	filter->output_texrender =
		create_or_reset_texrender(filter->output_texrender);

	if (gs_texrender_begin(filter->output_texrender, filter->width,
			       filter->height)) {
		gs_ortho(0.0f, (float)filter->width, 0.0f,
			 (float)filter->height, -100.0f, 100.0f);
		while (gs_effect_loop(effect, "Draw"))
			gs_draw_sprite(texture, 0, filter->width,
				       filter->height);
		gs_texrender_end(filter->output_texrender);
	}
	texture = gs_texrender_get_texture(filter->output_texrender);
	gs_texrender_destroy(source_render);
	gs_blend_state_pop();
}

static void apply_effect_mask_rect(composite_blur_filter_data_t *filter)
{
	float right = (100.0f - filter->mask_rect_center_x -
		       filter->mask_rect_width / 2.0f) /
		      100.0f;
	float left =
		(filter->mask_rect_center_x - filter->mask_rect_width / 2.0f) /
		100.0f;
	float top =
		(filter->mask_rect_center_y - filter->mask_rect_height / 2.0f) /
		100.0f;
	float bot = (100.0f - filter->mask_rect_center_y -
		     filter->mask_rect_height / 2.0f) /
		    100.0f;

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

	if (filter->param_filtered_image) {
		gs_effect_set_texture(filter->param_filtered_image,
				      filtered_texture);
	}

	struct vec2 scale;
	scale.x = 1.0f / (float)fmax(1.0f - right - left, 1.e-6f);
	scale.y = 1.0f / (float)fmax(1.0f - bot - top, 1.e-6f);
	if (filter->param_mask_crop_scale) {
		gs_effect_set_vec2(filter->param_mask_crop_scale, &scale);
	}

	struct vec2 box_ar;
	box_ar.x = (1.0f - right - left) * filter->width /
		   (float)fmin(filter->width, filter->height);
	box_ar.y = (1.0f - bot - top) * filter->height /
		   (float)fmin(filter->width, filter->height);
	if (filter->param_mask_crop_box_aspect_ratio) {
		gs_effect_set_vec2(filter->param_mask_crop_box_aspect_ratio,
				   &box_ar);
	}

	struct vec2 offset;
	offset.x = 1.0f - right - left > 0.0f ? left : -1000.0f;
	offset.y = 1.0f - bot - top > 0.0f ? top : -1000.0f;
	if (filter->param_mask_crop_offset) {
		gs_effect_set_vec2(filter->param_mask_crop_offset, &offset);
	}

	bool invert_v = filter->mask_rect_inv;
	if (filter->param_mask_crop_invert) {
		gs_effect_set_bool(filter->param_mask_crop_invert, invert_v);
	}

	float radius = filter->mask_rect_corner_radius / 100.0f *
		       (float)fmin(box_ar.x, box_ar.y);
	if (filter->param_mask_crop_corner_radius) {
		gs_effect_set_float(filter->param_mask_crop_corner_radius,
				    radius);
	}

	float feathering = filter->mask_rect_feathering / 100.0f;
	if (filter->param_mask_crop_feathering) {
		gs_effect_set_float(filter->param_mask_crop_feathering,
				    feathering);
	}
	set_blending_parameters();

	filter->output_texrender =
		create_or_reset_texrender(filter->output_texrender);

	if (gs_texrender_begin(filter->output_texrender, filter->width,
			       filter->height)) {
		gs_ortho(0.0f, (float)filter->width, 0.0f,
			 (float)filter->height, -100.0f, 100.0f);
		while (gs_effect_loop(effect, "Draw"))
			gs_draw_sprite(texture, 0, filter->width,
				       filter->height);
		gs_texrender_end(filter->output_texrender);
	}
	texture = gs_texrender_get_texture(filter->output_texrender);
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

	if (filter->param_filtered_image) {
		gs_effect_set_texture(filter->param_filtered_image,
				      filtered_texture);
	}

	struct vec2 scale;
	scale.x = 1.0f / (float)fmax(1.0f - right - left, 1.e-6f);
	scale.y = 1.0f / (float)fmax(1.0f - bot - top, 1.e-6f);
	if (filter->param_mask_crop_scale) {
		gs_effect_set_vec2(filter->param_mask_crop_scale, &scale);
	}

	struct vec2 box_ar;
	box_ar.x = (1.0f - right - left) * filter->width /
		   (float)fmin(filter->width, filter->height);
	box_ar.y = (1.0f - bot - top) * filter->height /
		   (float)fmin(filter->width, filter->height);
	if (filter->param_mask_crop_box_aspect_ratio) {
		gs_effect_set_vec2(filter->param_mask_crop_box_aspect_ratio,
				   &box_ar);
	}

	struct vec2 offset;
	offset.x = 1.0f - right - left > 0.0f ? left : -1000.0f;
	offset.y = 1.0f - bot - top > 0.0f ? top : -1000.0f;
	if (filter->param_mask_crop_offset) {
		gs_effect_set_vec2(filter->param_mask_crop_offset, &offset);
	}

	bool invert_v = filter->mask_crop_invert;
	if (filter->param_mask_crop_invert) {
		gs_effect_set_bool(filter->param_mask_crop_invert, invert_v);
	}

	float radius = filter->mask_crop_corner_radius / 100.0f *
		       (float)fmin(box_ar.x, box_ar.y);
	if (filter->param_mask_crop_corner_radius) {
		gs_effect_set_float(filter->param_mask_crop_corner_radius,
				    radius);
	}

	float feathering = filter->mask_crop_feathering / 100.0f;
	if (filter->param_mask_crop_feathering) {
		gs_effect_set_float(filter->param_mask_crop_feathering,
				    feathering);
	}
	set_blending_parameters();

	filter->output_texrender =
		create_or_reset_texrender(filter->output_texrender);

	if (gs_texrender_begin(filter->output_texrender, filter->width,
			       filter->height)) {
		gs_ortho(0.0f, (float)filter->width, 0.0f,
			 (float)filter->height, -100.0f, 100.0f);
		while (gs_effect_loop(effect, "Draw"))
			gs_draw_sprite(texture, 0, filter->width,
				       filter->height);
		gs_texrender_end(filter->output_texrender);
	}
	texture = gs_texrender_get_texture(filter->output_texrender);
	gs_blend_state_pop();
}

static void apply_effect_mask_circle(composite_blur_filter_data_t *filter)
{
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

	if (filter->param_filtered_image) {
		gs_effect_set_texture(filter->param_filtered_image,
				      filtered_texture);
	}

	struct vec2 center;
	center.x = filter->mask_circle_center_x / 100.0f;
	center.y = filter->mask_circle_center_y / 100.0f;
	if (filter->param_mask_circle_center) {
		gs_effect_set_vec2(filter->param_mask_circle_center, &center);
	}

	struct vec2 uv_scale;
	uv_scale.x = filter->width / (float)fmin(filter->width, filter->height);
	uv_scale.y =
		filter->height / (float)fmin(filter->width, filter->height);
	if (filter->param_mask_circle_uv_scale) {
		gs_effect_set_vec2(filter->param_mask_circle_uv_scale,
				   &uv_scale);
	}

	bool invert = filter->mask_circle_inv;
	if (filter->param_mask_circle_inv) {
		gs_effect_set_bool(filter->param_mask_circle_inv, invert);
	}

	float radius = filter->mask_circle_radius / 100.0f;
	if (filter->param_mask_circle_radius) {
		gs_effect_set_float(filter->param_mask_circle_radius, radius);
	}
	float feathering = filter->mask_circle_feathering / 100.0f;
	if (filter->param_mask_circle_feathering) {
		gs_effect_set_float(filter->param_mask_circle_feathering,
				    feathering);
	}
	set_blending_parameters();

	filter->output_texrender =
		create_or_reset_texrender(filter->output_texrender);

	if (gs_texrender_begin(filter->output_texrender, filter->width,
			       filter->height)) {
		gs_ortho(0.0f, (float)filter->width, 0.0f,
			 (float)filter->height, -100.0f, 100.0f);
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
	composite_blur_filter_data_t *filter = data;

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
	obs_property_list_add_int(blur_algorithms,
				  obs_module_text(ALGO_TEMPORAL_LABEL),
				  ALGO_TEMPORAL);

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
	obs_property_list_add_int(pixelate_types,
				  obs_module_text(PIXELATE_TYPE_VORONOI_LABEL),
				  PIXELATE_TYPE_VORONOI);
	obs_property_list_add_int(pixelate_types,
				  obs_module_text(PIXELATE_TYPE_RHOMBOID_LABEL),
				  PIXELATE_TYPE_RHOMBOID);
	obs_property_list_add_int(pixelate_types,
				  obs_module_text(PIXELATE_TYPE_TRIAKIS_LABEL),
				  PIXELATE_TYPE_TRIAKIS);

	obs_property_set_modified_callback(pixelate_types,
		setting_pixelate_animate_modified);

	obs_property_t* p = obs_properties_add_float_slider(
		props, "radius", obs_module_text("CompositeBlurFilter.Radius"),
		0.0, 80.1, 0.1);
	obs_property_float_set_suffix(p, "px");

	obs_properties_t* vector_blur = obs_properties_create();

	obs_property_t* vector_source = obs_properties_add_list(
		vector_blur, "vector_source",
		obs_module_text("CompositeBlurFilter.VectorBlur.Source"),
		OBS_COMBO_TYPE_EDITABLE, OBS_COMBO_FORMAT_STRING);
	obs_enum_sources(add_source_to_list, vector_source);
	obs_enum_scenes(add_source_to_list, vector_source);
	obs_property_list_insert_string(
		vector_source, 0,
		"",
		"");

	obs_property_t* vector_channel = obs_properties_add_list(
		vector_blur, "vector_blur_channel",
		obs_module_text("CompositeBlurFilter.VectorBlur.Channel"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(vector_channel,
		obs_module_text(GRADIENT_CHANNEL_RED_LABEL),
		GRADIENT_CHANNEL_RED);
	obs_property_list_add_int(vector_channel,
		obs_module_text(GRADIENT_CHANNEL_GREEN_LABEL),
		GRADIENT_CHANNEL_GREEN);
	obs_property_list_add_int(vector_channel,
		obs_module_text(GRADIENT_CHANNEL_BLUE_LABEL),
		GRADIENT_CHANNEL_BLUE);
	obs_property_list_add_int(vector_channel,
		obs_module_text(GRADIENT_CHANNEL_ALPHA_LABEL),
		GRADIENT_CHANNEL_ALPHA);
	obs_property_list_add_int(vector_channel,
		obs_module_text(GRADIENT_CHANNEL_LUMINANCE_LABEL),
		GRADIENT_CHANNEL_LUMINANCE);
	obs_property_list_add_int(vector_channel,
		obs_module_text(GRADIENT_CHANNEL_SATURATION_LABEL),
		GRADIENT_CHANNEL_SATURATION);

	obs_property_set_modified_callback2(vector_channel,
		vector_channel_modified, data);

	obs_property_t* vector_gradient_type = obs_properties_add_list(
		vector_blur, "vector_gradient_type",
		obs_module_text("CompositeBlurFilter.VectorBlur.GradientType"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(vector_gradient_type,
		obs_module_text(GRADIENT_TYPE_SOBEL_LABEL),
		GRADIENT_TYPE_SOBEL);
	obs_property_list_add_int(vector_gradient_type,
		obs_module_text(GRADIENT_TYPE_CENTRAL_LIMIT_DIFF_LABEL),
		GRADIENT_TYPE_CENTRAL_LIMIT_DIFF);
	obs_property_list_add_int(vector_gradient_type,
		obs_module_text(GRADIENT_TYPE_FORWARD_DIFF_LABEL),
		GRADIENT_TYPE_FORWARD_DIFF);

	p = obs_properties_add_float_slider(
		vector_blur, "vector_blur_amount",
		obs_module_text("CompositeBlurFilter.VectorBlur.Amount"),
		-100.0, 100.0, 0.1);
	obs_property_float_set_suffix(p, "%");

	obs_properties_add_float_slider(
		vector_blur, "vector_blur_smoothing",
		obs_module_text("CompositeBlurFilter.VectorBlur.Smoothing"),
		0.0, 1000.0, 0.1);

	obs_properties_add_group(props, "vector_group",
		obs_module_text("CompositeBlurFilter.VectorBlur"),
		OBS_GROUP_NORMAL, vector_blur);

	obs_properties_add_float_slider(
		props, "temporal_current_weight", obs_module_text("CompositeBlurFilter.Temporal.Amount"),
		0.0, 1.0, 0.01);

	obs_property_t* clear_threshold = obs_properties_add_float_slider(
		props, "temporal_clear_threshold", obs_module_text("CompositeBlurFilter.Temporal.ClearThreshold"),
		0.0, 100.0, 0.1);

	obs_property_float_set_suffix(clear_threshold, "%");

	p = obs_properties_add_float_slider(
		props, "pixelate_smoothing_pct",
		obs_module_text("CompositeBlurFilter.Pixelate.Smoothing"), 0.0,
		100.0, 0.1);
	obs_property_float_set_suffix(p, "%");

	p = obs_properties_add_float_slider(
		props, "pixelate_origin_x",
		obs_module_text("CompositeBlurFilter.Pixelate.Origin_X"),
		-6000.0, 6000.0, 0.1);
	obs_property_float_set_suffix(p, "px");

	p = obs_properties_add_float_slider(
		props, "pixelate_origin_y",
		obs_module_text("CompositeBlurFilter.Pixelate.Origin_Y"),
		-6000.0, 6000.0, 0.1);
	obs_property_float_set_suffix(p, "px");

	p = obs_properties_add_float_slider(
		props, "pixelate_rotation",
		obs_module_text("CompositeBlurFilter.Pixelate.Rotation"),
		-360.0, 360.0, 0.1);
	obs_property_float_set_suffix(p, "deg");

	obs_property_t *animate_p = obs_properties_add_bool(
		props, "pixelate_animate",
		obs_module_text("CompositeBlurFilter.Pixelate.Animate")
	);

	obs_property_set_modified_callback(animate_p,
		setting_pixelate_animate_modified);

	p = obs_properties_add_float(
		props, "pixelate_time",
		obs_module_text("CompositeBlurFilter.Pixelate.Time"),
		0.0, 1000000.0, 0.01
	);
	obs_property_float_set_suffix(p, "s");

	obs_properties_add_float_slider(
		props, "pixelate_animation_speed",
		obs_module_text("CompositeBlurFilter.Pixelate.AnimationSpeed"),
		0.0, 1000.0, 0.1
	);

	obs_properties_add_int_slider(
		props, "passes",
		obs_module_text("CompositeBlurFilter.Box.Passes"), 1, 5, 1);

	p = obs_properties_add_float_slider(
		props, "kawase_passes",
		obs_module_text("CompositeBlurFilter.DualKawase.Passes"), 0.0,
		1025.0, 0.01);
	obs_property_float_set_suffix(p, "px");

	p = obs_properties_add_float_slider(
		props, "angle", obs_module_text("CompositeBlurFilter.Angle"),
		-360.0, 360.1, 0.1);
	obs_property_float_set_suffix(p, "deg");

	obs_properties_t *center_coords = obs_properties_create();

	p = obs_properties_add_float_slider(
		center_coords, "center_x",
		obs_module_text("CompositeBlurFilter.Center.X"), -3840.0,
		7680.0, 1.0);
	obs_property_float_set_suffix(p, "px");

	p = obs_properties_add_float_slider(
		center_coords, "center_y",
		obs_module_text("CompositeBlurFilter.Center.Y"), -2160.0,
		4320.0, 1.0);
	obs_property_float_set_suffix(p, "px");

	p = obs_properties_add_float_slider(
		center_coords, "inactive_radius",
		obs_module_text("CompositeBlurFilter.Center.InactiveRadius"), 0.0,
		2500.0, 1.0);
	obs_property_float_set_suffix(p, "px");

	obs_properties_add_group(
		props, "center_coordinate",
		obs_module_text("CompositeBlurFilter.CenterCoordinate"),
		OBS_GROUP_NORMAL, center_coords);

	obs_properties_t *tilt_shift_bounds = obs_properties_create();
	obs_properties_add_float_slider(
		tilt_shift_bounds, "tilt_shift_center",
		obs_module_text("CompositeBlurFilter.TiltShift.Center"), 0.0,
		1.0, 0.01);

	p = obs_properties_add_float_slider(
		tilt_shift_bounds, "tilt_shift_angle",
		obs_module_text("CompositeBlurFilter.TiltShift.Angle"), -180.0,
		181.0, 0.1);
	obs_property_float_set_suffix(p, "deg");

	obs_properties_add_float_slider(
		tilt_shift_bounds, "tilt_shift_width",
		obs_module_text("CompositeBlurFilter.TiltShift.Width"), 0.0,
		1.01, 0.01);

	obs_properties_add_group(
		props, "tilt_shift_bounds",
		obs_module_text("CompositeBlurFilter.TiltShift"),
		OBS_GROUP_NORMAL, tilt_shift_bounds);

	p = obs_properties_add_list(
		props, "background",
		obs_module_text("CompositeBlurFilter.Background"),
		OBS_COMBO_TYPE_EDITABLE, OBS_COMBO_FORMAT_STRING);
	obs_enum_sources(add_source_to_list, p);
	obs_enum_scenes(add_source_to_list, p);
	obs_property_list_insert_string(
		p, 0, obs_module_text("CompositeBlurFilter.Background.None"),
		"");

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
	obs_property_list_add_int(effect_mask_list,
				  obs_module_text(EFFECT_MASK_TYPE_IMAGE_LABEL),
				  EFFECT_MASK_TYPE_IMAGE);
	obs_property_list_add_int(effect_mask_list,
				  obs_module_text(EFFECT_MASK_TYPE_RECT_LABEL),
				  EFFECT_MASK_TYPE_RECT);
	obs_property_list_add_int(
		effect_mask_list,
		obs_module_text(EFFECT_MASK_TYPE_CIRCLE_LABEL),
		EFFECT_MASK_TYPE_CIRCLE);

	obs_property_set_modified_callback(effect_mask_list,
					   setting_effect_mask_modified);

	obs_properties_t *effect_mask_circle = obs_properties_create();

	obs_properties_add_float_slider(
		effect_mask_circle, "effect_mask_circle_center_x",
		obs_module_text(
			"CompositeBlurFilter.EffectMask.Circle.CenterX"),
		-500.01, 600.01, 0.01);

	obs_properties_add_float_slider(
		effect_mask_circle, "effect_mask_circle_center_y",
		obs_module_text(
			"CompositeBlurFilter.EffectMask.Circle.CenterY"),
		-500.01, 600.01, 0.01);

	obs_properties_add_float_slider(
		effect_mask_circle, "effect_mask_circle_radius",
		obs_module_text("CompositeBlurFilter.EffectMask.Circle.Radius"),
		0.0, 500.01, 0.01);

	obs_properties_add_float_slider(
		effect_mask_circle, "effect_mask_circle_feathering",
		obs_module_text(
			"CompositeBlurFilter.EffectMask.Circle.Feathering"),
		0.0, 100.01, 0.01);

	obs_properties_add_bool(
		effect_mask_circle, "effect_mask_circle_invert",
		obs_module_text("CompositeBlurFilter.EffectMask.Invert"));

	obs_properties_add_group(
		props, "effect_mask_circle",
		obs_module_text(
			"CompositeBlurFilter.EffectMask.CircleParameters"),
		OBS_GROUP_NORMAL, effect_mask_circle);

	obs_properties_t *effect_mask_rect = obs_properties_create();

	obs_properties_add_float_slider(
		effect_mask_rect, "effect_mask_rect_center_x",
		obs_module_text("CompositeBlurFilter.EffectMask.Rect.CenterX"),
		-500.01, 600.01, 0.01);

	obs_properties_add_float_slider(
		effect_mask_rect, "effect_mask_rect_center_y",
		obs_module_text("CompositeBlurFilter.EffectMask.Rect.CenterY"),
		-500.01, 600.01, 0.01);

	obs_properties_add_float_slider(
		effect_mask_rect, "effect_mask_rect_width",
		obs_module_text("CompositeBlurFilter.EffectMask.Rect.Width"),
		0.0, 500.01, 0.01);

	obs_properties_add_float_slider(
		effect_mask_rect, "effect_mask_rect_height",
		obs_module_text("CompositeBlurFilter.EffectMask.Rect.Height"),
		0.0, 500.01, 0.01);

	obs_properties_add_float_slider(
		effect_mask_rect, "effect_mask_rect_corner_radius",
		obs_module_text(
			"CompositeBlurFilter.EffectMask.Rect.CornerRadius"),
		0.0, 100.01, 0.01);

	obs_properties_add_float_slider(
		effect_mask_rect, "effect_mask_rect_feathering",
		obs_module_text(
			"CompositeBlurFilter.EffectMask.Rect.Feathering"),
		0.0, 100.01, 0.01);

	obs_properties_add_bool(
		effect_mask_rect, "effect_mask_rect_invert",
		obs_module_text("CompositeBlurFilter.EffectMask.Invert"));

	obs_properties_add_group(
		props, "effect_mask_rect",
		obs_module_text(
			"CompositeBlurFilter.EffectMask.RectParameters"),
		OBS_GROUP_NORMAL, effect_mask_rect);

	obs_properties_t *effect_mask_source = obs_properties_create();

	obs_properties_add_path(
		effect_mask_source, "effect_mask_source_file",
		obs_module_text("CompositeBlurFilter.EffectMask.Source.File"),
		OBS_PATH_FILE,
		"Textures (*.bmp *.tga *.png *.jpeg *.jpg *.gif);;", NULL);

	obs_property_t *effect_mask_source_source = obs_properties_add_list(
		effect_mask_source, "effect_mask_source_source",
		obs_module_text("CompositeBlurFilter.EffectMask.Source.Source"),
		OBS_COMBO_TYPE_EDITABLE, OBS_COMBO_FORMAT_STRING);
	obs_enum_sources(add_source_to_list, effect_mask_source_source);
	obs_enum_scenes(add_source_to_list, effect_mask_source_source);
	obs_property_list_insert_string(
		effect_mask_source_source, 0,
		obs_module_text("CompositeBlurFilter.EffectMask.Source.None"),
		"");

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

	obs_properties_add_float_slider(
		effect_mask_crop, "effect_mask_crop_feathering",
		obs_module_text("CompositeBlurFilter.EffectMask.Feathering"),
		0.0, 100.01, 0.01);

	obs_properties_add_bool(
		effect_mask_crop, "effect_mask_crop_invert",
		obs_module_text("CompositeBlurFilter.EffectMask.Invert"));

	obs_properties_add_group(
		props, "effect_mask_crop",
		obs_module_text(
			"CompositeBlurFilter.EffectMask.CropParameters"),
		OBS_GROUP_NORMAL, effect_mask_crop);

	obs_properties_add_text(props, "plugin_info", PLUGIN_INFO,
				OBS_TEXT_INFO);

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

static bool setting_pixelate_animate_modified(obs_properties_t* props,
	obs_property_t* p,
	obs_data_t* settings)
{
	UNUSED_PARAMETER(p);
	int pixelate_type = (int)obs_data_get_int(settings, "pixelate_type");
	if (pixelate_type == PIXELATE_TYPE_VORONOI) {
		bool animate = obs_data_get_bool(settings, "pixelate_animate");
		setting_visibility("pixelate_time", !animate, props);
		setting_visibility("pixelate_animation_speed", animate, props);
	}
	else {
		setting_visibility("pixelate_animate", false, props);
		setting_visibility("pixelate_time", false, props);
		setting_visibility("pixelate_animation_speed", false, props);
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
		setting_visibility("effect_mask_circle", false, props);
		setting_visibility("effect_mask_rect", false, props);
		break;
	case EFFECT_MASK_TYPE_CROP:
		setting_visibility("effect_mask_crop", true, props);
		setting_visibility("effect_mask_source", false, props);
		setting_visibility("effect_mask_circle", false, props);
		setting_visibility("effect_mask_rect", false, props);
		break;
	case EFFECT_MASK_TYPE_CIRCLE:
		setting_visibility("effect_mask_crop", false, props);
		setting_visibility("effect_mask_source", false, props);
		setting_visibility("effect_mask_circle", true, props);
		setting_visibility("effect_mask_rect", false, props);
		break;
	case EFFECT_MASK_TYPE_RECT:
		setting_visibility("effect_mask_crop", false, props);
		setting_visibility("effect_mask_source", false, props);
		setting_visibility("effect_mask_circle", false, props);
		setting_visibility("effect_mask_rect", true, props);
		break;
	case EFFECT_MASK_TYPE_SOURCE:
		setting_visibility("effect_mask_crop", false, props);
		setting_visibility("effect_mask_source", true, props);
		setting_visibility("effect_mask_circle", false, props);
		setting_visibility("effect_mask_rect", false, props);
		setting_visibility("effect_mask_source_file", false, props);
		setting_visibility("effect_mask_source_source", true, props);
		{
			obs_property_t *prop =
				obs_properties_get(props, "effect_mask_source");
			obs_property_set_description(
				prop,
				obs_module_text(
					"CompositeBlurFilter.EffectMask.SourceParameters"));
		}
		break;
	case EFFECT_MASK_TYPE_IMAGE:
		setting_visibility("effect_mask_crop", false, props);
		setting_visibility("effect_mask_source", true, props);
		setting_visibility("effect_mask_circle", false, props);
		setting_visibility("effect_mask_rect", false, props);
		setting_visibility("effect_mask_source_file", true, props);
		setting_visibility("effect_mask_source_source", false, props);
		{
			obs_property_t *prop =
				obs_properties_get(props, "effect_mask_source");
			obs_property_set_description(
				prop,
				obs_module_text(
					"CompositeBlurFilter.EffectMask.ImageParameters"));
		}
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
	case EFFECT_MASK_TYPE_CIRCLE:
		load_circle_mask_effect(filter);
		break;
	case EFFECT_MASK_TYPE_RECT:
		load_crop_mask_effect(filter);
		break;
	case EFFECT_MASK_TYPE_IMAGE:
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
		setting_visibility("temporal_current_weight", false, props);
		setting_visibility("temporal_clear_threshold", false, props);
		setting_visibility("passes", false, props);
		setting_visibility("kawase_passes", false, props);
		setting_visibility("blur_type", true, props);
		setting_visibility("pixelate_type", false, props);
		setting_visibility("pixelate_smoothing_pct", false, props);
		setting_visibility("pixelate_rotation", false, props);
		setting_visibility("pixelate_origin_x", false, props);
		setting_visibility("pixelate_origin_y", false, props);
		setting_visibility("pixelate_animate", false, props);
		setting_visibility("pixelate_time", false, props);
		setting_visibility("pixelate_animation_speed", false, props);

		set_blur_radius_settings(
			obs_module_text("CompositeBlurFilter.Radius"), 0.0f,
			80.01f, 0.1f, props);
		set_gaussian_blur_types(props);
		break;
	case ALGO_BOX:
		setting_visibility("radius", true, props);
		setting_visibility("temporal_current_weight", false, props);
		setting_visibility("temporal_clear_threshold", false, props);
		setting_visibility("kawase_passes", false, props);
		setting_visibility("passes", true, props);
		setting_visibility("blur_type", true, props);
		setting_visibility("pixelate_type", false, props);
		setting_visibility("pixelate_smoothing_pct", false, props);
		setting_visibility("pixelate_rotation", false, props);
		setting_visibility("pixelate_origin_x", false, props);
		setting_visibility("pixelate_origin_y", false, props);
		setting_visibility("pixelate_animate", false, props);
		setting_visibility("pixelate_time", false, props);
		setting_visibility("pixelate_animation_speed", false, props);

		set_blur_radius_settings(
			obs_module_text("CompositeBlurFilter.Radius"), 0.0f,
			100.01f, 0.1f, props);
		set_box_blur_types(props);
		break;
	case ALGO_DUAL_KAWASE:
		setting_visibility("radius", false, props);
		setting_visibility("temporal_current_weight", false, props);
		setting_visibility("temporal_clear_threshold", false, props);
		setting_visibility("passes", false, props);
		setting_visibility("kawase_passes", true, props);
		setting_visibility("blur_type", false, props);
		setting_visibility("pixelate_type", false, props);
		setting_visibility("pixelate_smoothing_pct", false, props);
		setting_visibility("pixelate_rotation", false, props);
		setting_visibility("pixelate_origin_x", false, props);
		setting_visibility("pixelate_origin_y", false, props);
		setting_visibility("pixelate_animate", false, props);
		setting_visibility("pixelate_time", false, props);
		setting_visibility("pixelate_animation_speed", false, props);
		set_dual_kawase_blur_types(props);
		obs_data_set_int(settings, "blur_type", TYPE_AREA);
		settings_blur_area(props, settings);
		break;
	case ALGO_PIXELATE:
		setting_visibility("radius", true, props);
		setting_visibility("temporal_current_weight", false, props);
		setting_visibility("temporal_clear_threshold", false, props);
		setting_visibility("passes", false, props);
		setting_visibility("kawase_passes", false, props);
		setting_visibility("blur_type", false, props);
		setting_visibility("pixelate_type", true, props);
		setting_visibility("pixelate_smoothing_pct", true, props);
		setting_visibility("pixelate_rotation", true, props);
		setting_visibility("pixelate_origin_x", true, props);
		setting_visibility("pixelate_origin_y", true, props);
		setting_visibility("pixelate_animate", true, props);
		setting_visibility("pixelate_time", true, props);
		setting_visibility("pixelate_animation_speed", true, props);
		set_blur_radius_settings(
			obs_module_text(
				"CompositeBlurFilter.Pixelate.PixelSize"),
			1.0f, 1024.01f, 0.1f, props);
		set_pixelate_blur_types(props);
		obs_data_set_int(settings, "blur_type", TYPE_AREA);
		settings_blur_area(props, settings);
		setting_pixelate_animate_modified(props, p, settings);
		break;
	case ALGO_TEMPORAL:
		setting_visibility("radius", false, props);
		setting_visibility("temporal_current_weight", true, props);
		setting_visibility("temporal_clear_threshold", true, props);
		setting_visibility("kawase_passes", false, props);
		setting_visibility("passes", false, props);
		setting_visibility("blur_type", false, props);
		setting_visibility("pixelate_type", false, props);
		setting_visibility("pixelate_smoothing_pct", false, props);
		setting_visibility("pixelate_rotation", false, props);
		setting_visibility("pixelate_origin_x", false, props);
		setting_visibility("pixelate_origin_y", false, props);
		setting_visibility("pixelate_animate", false, props);
		setting_visibility("pixelate_time", false, props);
		setting_visibility("pixelate_animation_speed", false, props);
		break;
	}
	return true;
}

static bool setting_blur_types_modified(void *data, obs_properties_t *props,
					obs_property_t *p, obs_data_t *settings)
{
	UNUSED_PARAMETER(p);
	//UNUSED_PARAMETER(data);
	composite_blur_filter_data_t* filter = data;
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
	} else if (blur_type == TYPE_VECTOR) {
		return settings_blur_vector(props, filter);
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
	setting_visibility("vector_group", false, props);
	return true;
}

static bool settings_blur_directional(obs_properties_t *props)
{
	setting_visibility("radius", true, props);
	setting_visibility("angle", true, props);
	setting_visibility("center_coordinate", false, props);
	setting_visibility("background", true, props);
	setting_visibility("tilt_shift_bounds", false, props);
	setting_visibility("vector_group", false, props);
	return true;
}

static bool settings_blur_zoom(obs_properties_t *props)
{
	setting_visibility("radius", true, props);
	setting_visibility("angle", false, props);
	setting_visibility("center_coordinate", true, props);
	setting_visibility("background", true, props);
	setting_visibility("tilt_shift_bounds", false, props);
	setting_visibility("vector_group", false, props);
	return true;
}

static bool settings_blur_tilt_shift(obs_properties_t *props)
{
	setting_visibility("radius", true, props);
	setting_visibility("angle", false, props);
	setting_visibility("center_coordinate", false, props);
	setting_visibility("background", true, props);
	setting_visibility("tilt_shift_bounds", true, props);
	setting_visibility("vector_group", false, props);
	return true;
}

static bool settings_blur_vector(obs_properties_t* props, composite_blur_filter_data_t* filter)
{
	setting_visibility("radius", false , props);
	setting_visibility("angle", false, props);
	setting_visibility("center_coordinate", false, props);
	setting_visibility("background", true, props);
	setting_visibility("tilt_shift_bounds", false, props);
	setting_visibility("vector_group", true, props);
	// TODO: Adjust visibility of our vector settings.
	filter->last_vector_blur_amount = -999999.0;
	return true;
}

bool composite_blur_enable_hotkey(void *data, obs_hotkey_pair_id id,
				  obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	composite_blur_filter_data_t *filter = data;
	if (!pressed)
		return false;

	if (obs_source_enabled(filter->context))
		return false;
	obs_source_set_enabled(filter->context, true);

	return true;
}

bool composite_blur_disable_hotkey(void *data, obs_hotkey_pair_id id,
				   obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	composite_blur_filter_data_t *filter = data;
	if (!pressed)
		return false;
	if (!obs_source_enabled(filter->context))
		return false;
	obs_source_set_enabled(filter->context, false);

	return true;
}

static void composite_blur_video_tick(void *data, float seconds)
{
	composite_blur_filter_data_t *filter = data;
	if (filter->pixelate_animate) {
		filter->time += seconds * filter->pixelate_animation_speed;
	}
	else {
		filter->time = filter->pixelate_animation_time;
	}
	obs_source_t *target = obs_filter_get_target(filter->context);
	if (!target) {
		return;
	}

	if (filter->has_mask_source && !filter->mask_source_source) {
		obs_source_t* mask_source =
			filter->has_mask_source
			? obs_get_source_by_name(filter->mask_source_name.array)
			: NULL;

		if (mask_source) {
			filter->mask_source_source =
				obs_source_get_weak_source(mask_source);
			obs_source_release(mask_source);
		}
		else {
			filter->mask_source_source = NULL;
		}
	}

	if (filter->has_background_source && !filter->background) {
		obs_source_t* background_source =
			filter->has_background_source
			? obs_get_source_by_name(filter->background_source_name.array)
			: NULL;

		if (background_source) {
			filter->background =
				obs_source_get_weak_source(background_source);
			obs_source_release(background_source);
		}
		else {
			filter->background = NULL;
		}
	}

	if (filter->hotkey == OBS_INVALID_HOTKEY_PAIR_ID) {
		obs_source_t *parent = obs_filter_get_parent(filter->context);
		if (parent) {
			const char *filter_name =
				obs_source_get_name(filter->context);
			struct dstr enable;
			struct dstr disable;

			dstr_init_copy(
				&enable,
				obs_module_text("CompositeBlurFilter.Enable"));
			dstr_init_copy(
				&disable,
				obs_module_text("CompositeBlurFilter.Disable"));
			dstr_cat(&enable, " ");
			dstr_cat(&enable, filter_name);
			dstr_cat(&disable, " ");
			dstr_cat(&disable, filter_name);

			filter->hotkey = obs_hotkey_pair_register_source(
				parent, "CompositeBlur.Enable", enable.array,
				"CompositeBlur.Disable", disable.array,
				composite_blur_enable_hotkey,
				composite_blur_disable_hotkey, filter, filter);
			dstr_free(&enable);
			dstr_free(&disable);
		}
	}
	const uint32_t width = (uint32_t)obs_source_get_base_width(target);
	const uint32_t height = (uint32_t)obs_source_get_base_height(target);
	if(filter->width != width || filter->height != height) {
		filter->width = (uint32_t)obs_source_get_base_width(target);
		filter->height = (uint32_t)obs_source_get_base_height(target);
		filter->uv_size.x = (float)filter->width;
		filter->uv_size.y = (float)filter->height;
	}
	obs_data_t* settings = obs_source_get_settings(filter->context);
	if (filter->width > 0 &&
		(float)obs_data_get_double(settings, "pixelate_origin_x") < -1.e8) {
		obs_data_set_double(settings, "pixelate_origin_x", (double)width / 2.0);
		obs_data_set_double(settings, "pixelate_origin_y",
			(double)height / 2.0);
		obs_data_set_double(settings, "center_x", (double)width / 2.0);
		obs_data_set_double(settings, "center_y", (double)height / 2.0);
		filter->center_x = (float)width / 2.0f;
		filter->center_y = (float)height / 2.0f;

		filter->pixelate_tessel_center.x = (float)width / 2.0f;
		filter->pixelate_tessel_center.y = (float)height / 2.0f;
	}

	obs_data_release(settings);

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
	} else if (filter->blur_algorithm == ALGO_TEMPORAL) {
		temporal_setup_callbacks(filter);
	}	

	if (filter->load_effect) {
		filter->load_effect(filter);
		load_composite_effect(filter);
		load_mix_effect(filter);
		load_output_effect(filter);
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
	dstr_free(&filename);
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
	dstr_free(&filename);

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
			if (strcmp(info.name, "filtered_image") == 0) {
				filter->param_filtered_image = param;
			} else if (strcmp(info.name, "scale") == 0) {
				filter->param_mask_crop_scale = param;
			} else if (strcmp(info.name, "offset") == 0) {
				filter->param_mask_crop_offset = param;
			} else if (strcmp(info.name, "box_aspect_ratio") == 0) {
				filter->param_mask_crop_box_aspect_ratio =
					param;
			} else if (strcmp(info.name, "corner_radius") == 0) {
				filter->param_mask_crop_corner_radius = param;
			} else if (strcmp(info.name, "feathering") == 0) {
				filter->param_mask_crop_feathering = param;
			} else if (strcmp(info.name, "inv") == 0) {
				filter->param_mask_crop_invert = param;
			}
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
	dstr_free(&filename);

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
			if (strcmp(info.name, "filtered_image") == 0) {
				filter->param_filtered_image = param;
			} else if (strcmp(info.name, "alpha_source") == 0) {
				filter->param_mask_source_alpha_source = param;
			} else if (strcmp(info.name, "rgba_weights") == 0) {
				filter->param_mask_source_rgba_weights = param;
			} else if (strcmp(info.name, "multiplier") == 0) {
				filter->param_mask_source_multiplier = param;
			} else if (strcmp(info.name, "inv") == 0) {
				filter->param_mask_source_invert = param;
			}
		}
	}
}

static void load_circle_mask_effect(composite_blur_filter_data_t *filter)
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
	dstr_cat(&filename, "/shaders/effect_mask_circle.effect");
	shader_text = load_shader_from_file(filename.array);
	char *errors = NULL;
	dstr_free(&filename);

	obs_enter_graphics();
	filter->effect_mask_effect =
		gs_effect_create(shader_text, NULL, &errors);
	obs_leave_graphics();

	bfree(shader_text);
	if (filter->effect_mask_effect == NULL) {
		blog(LOG_WARNING,
		     "[obs-composite-blur] Unable to load effect_mask_circle.effect file.  Errors:\n%s",
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
			if (strcmp(info.name, "filtered_image") == 0) {
				filter->param_filtered_image = param;
			} else if (strcmp(info.name, "inv") == 0) {
				filter->param_mask_circle_inv = param;
			} else if (strcmp(info.name, "center") == 0) {
				filter->param_mask_circle_center = param;
			} else if (strcmp(info.name, "circle_radius") == 0) {
				filter->param_mask_circle_radius = param;
			} else if (strcmp(info.name, "feathering") == 0) {
				filter->param_mask_circle_feathering = param;
			} else if (strcmp(info.name, "uv_scale") == 0) {
				filter->param_mask_circle_uv_scale = param;
			}
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
	dstr_free(&filename);

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
		}
	}
}

static void load_output_effect(composite_blur_filter_data_t *filter)
{
	if (filter->output_effect != NULL) {
		obs_enter_graphics();
		gs_effect_destroy(filter->output_effect);
		filter->output_effect = NULL;
		obs_leave_graphics();
	}

	char *shader_text = NULL;
	struct dstr filename = {0};
	dstr_cat(&filename, obs_get_module_data_path(obs_current_module()));
	dstr_cat(&filename, "/shaders/render_output.effect");
	shader_text = load_shader_from_file(filename.array);
	char *errors = NULL;
	dstr_free(&filename);

	obs_enter_graphics();
	filter->output_effect = gs_effect_create(shader_text, NULL, &errors);
	obs_leave_graphics();

	bfree(shader_text);
	if (filter->output_effect == NULL) {
		blog(LOG_WARNING,
		     "[obs-composite-blur] Unable to load output.effect file.  Errors:\n%s",
		     (errors == NULL || strlen(errors) == 0 ? "(None)"
							    : errors));
		bfree(errors);
	} else {
		size_t effect_count =
			gs_effect_get_num_params(filter->output_effect);
		for (size_t effect_index = 0; effect_index < effect_count;
		     effect_index++) {
			gs_eparam_t *param = gs_effect_get_param_by_idx(
				filter->output_effect, effect_index);
			struct gs_effect_param_info info;
			gs_effect_get_param_info(param, &info);
			if (strcmp(info.name, "output_image") == 0) {
				filter->param_output_image = param;
			}
		}
	}
}

void get_background(composite_blur_filter_data_t *data)
{
	// Get source
	obs_source_t *source =
		data->background ? obs_weak_source_get_source(data->background)
				 : NULL;

	//gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	if (source) {
		const enum gs_color_space preferred_spaces[] = {
			GS_CS_SRGB,
			GS_CS_SRGB_16F,
			GS_CS_709_EXTENDED,
		};
		const enum gs_color_space space = obs_source_get_color_space(
			data->context, OBS_COUNTOF(preferred_spaces),
			preferred_spaces);
		// const enum gs_color_format format =
		// 	gs_get_format_from_space(space);

		data->background_texrender =
			create_or_reset_texrender(data->background_texrender);
		uint32_t base_width = obs_source_get_base_width(source);
		uint32_t base_height = obs_source_get_base_height(source);
		gs_blend_state_push();
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);
		//set_blending_parameters();
		if (gs_texrender_begin_with_color_space(
			    data->background_texrender, base_width, base_height,
			    space)) {
			const float w = (float)base_width;
			const float h = (float)base_height;
			struct vec4 clear_color;

			vec4_zero(&clear_color);
			gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
			gs_ortho(0.0f, w, 0.0f, h, -100.0f, 100.0f);

			obs_source_video_render(source);
			gs_texrender_end(data->background_texrender);
		}
		gs_blend_state_pop();
		obs_source_release(source);
	}
}

gs_texture_t *blend_composite(gs_texture_t *texture,
			      composite_blur_filter_data_t *data)
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
			data->context, OBS_COUNTOF(preferred_spaces),
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
		//set_blending_parameters();
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
		gs_texture_t *tex = gs_texrender_get_texture(source_render);

		if (data->param_background) {
			gs_effect_set_texture_srgb(data->param_background, tex);
		}
		gs_eparam_t *image =
			gs_effect_get_param_by_name(composite_effect, "image");
		gs_effect_set_texture(image, texture);

		data->composite_render =
			create_or_reset_texrender(data->composite_render);

		//set_blending_parameters();

		//gs_blend_state_push();
		//gs_blend_function(GS_BLEND_ONE, GS_BLEND_SRCALPHA);

		//if (gs_texrender_begin(data->composite_render, data->width,
		//		       data->height)) {
		//	gs_ortho(0.0f, (float)data->width, 0.0f,
		//		 (float)data->height, -100.0f, 100.0f);
		//	while (gs_effect_loop(data->composite_effect, "Draw"))
		//		gs_draw_sprite(texture, 0, data->width,
		//			       data->height);
		//	gs_texrender_end(data->composite_render);
		//}
		//texture = gs_texrender_get_texture(data->composite_render);
		//gs_texrender_destroy(source_render);
		//gs_blend_state_pop();

		const enum gs_color_space context_space =
			obs_source_get_color_space(
				data->context, OBS_COUNTOF(preferred_spaces),
				preferred_spaces);
		const enum gs_color_format context_format =
			gs_get_format_from_space(context_space);

		if (obs_source_process_filter_begin_with_color_space(
			    data->context, context_format, context_space,
			    OBS_ALLOW_DIRECT_RENDERING) &&
		    gs_texrender_begin(data->composite_render, data->width,
				       data->height)) {

			set_blending_parameters();
			//gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
			gs_ortho(0.0f, (float)data->width, 0.0f,
				 (float)data->height, -100.0f, 100.0f);
			obs_source_process_filter_end(data->context,
						      composite_effect,
						      data->width,
						      data->height);
			gs_texrender_end(data->composite_render);
		}
		texture = gs_texrender_get_texture(data->composite_render);
		gs_texrender_destroy(source_render);
		gs_blend_state_pop();
	}
	return texture;
}
