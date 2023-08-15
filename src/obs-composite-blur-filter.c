#include <obs-composite-blur-filter.h>

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

void calculate_kernel(float radius, struct composite_blur_filter_data *filter)
{
	const size_t max_size = 128;
	const float max_radius = 250.0;
	const float min_radius = 0.0;
	size_t d_kernel_size = 0;

	fDarray d_weights;
	da_init(d_weights);

	fDarray weights;
	da_init(weights);

	radius *= 3.0f;
	radius = max(min(radius, max_radius), min_radius);

	// 1. Calculate discrete weights
	const float bins_per_pixel =
		((2.f * (float)kernel_size - 1.f)) / (1.f + 2.f * radius);
	size_t current_bin = 0;
	float fractional_bin = 0.5f;
	float ceil_radius = (radius - (float)floor(radius)) < 0.001f
				    ? radius
				    : (float)ceil(radius);
	float fractional_extra = 1.0f - (ceil_radius - radius);

	for (int i = 0; i <= (int)ceil_radius; i++) {
		float cur_radius = (float)i;
		float fractional_pixel = i < (int)ceil_radius ? 1.0f
					 : fractional_extra < 0.002f
						 ? 1.0f
						 : fractional_extra;
		float bpp_mult = i == 0 ? 0.5f : 1.0f;
		float weight =
			1.0f / bpp_mult * fractional_bin * kernel[current_bin];
		float remaining_bins =
			bpp_mult * fractional_pixel * bins_per_pixel -
			fractional_bin;
		while ((int)floor(remaining_bins) > 0) {
			current_bin++;
			weight += 1.0f / bpp_mult * kernel[current_bin];
			remaining_bins -= 1.f;
		}
		current_bin++;
		if (remaining_bins > 1.e-6f) {
			weight += 1.0f / bpp_mult * kernel[current_bin] *
				  remaining_bins;
			fractional_bin = 1.0f - remaining_bins;
		} else {
			fractional_bin = 1.0f;
		}
		if (weight > 1.0001f || weight < 0.0f) {
			obs_log(LOG_WARNING,
				"   === BAD WEIGHT VALUE FOR GAUSSIAN === [%d] %f",
				weights.num + 1, weight);
			weight = 0.0;
		}
		da_push_back(d_weights, &weight);
	}

	fDarray offsets;
	da_init(offsets);

	fDarray d_offsets;
	da_init(d_offsets);

	// 2. Calculate discrete offsets
	for (int i = 0; i <= (int)ceil_radius; i++) {
		float val = (float)i;
		da_push_back(d_offsets, &val);
	}

	// 3. Calculate linear sampled weights and offsets
	da_push_back(weights, &d_weights.array[0]);
	da_push_back(offsets, &d_offsets.array[0]);

	for (size_t i = 1; i < d_weights.num - 1; i += 2) {
		const float weight =
			d_weights.array[i] + d_weights.array[i + 1];
		da_push_back(weights, &weight);
		const float offset =
			(d_offsets.array[i] * d_weights.array[i] +
			 d_offsets.array[i + 1] * d_weights.array[i + 1]) /
			weight;
		da_push_back(offsets, &offset);
	}
	if (d_weights.num % 2 == 0) {
		const float weight = d_weights.array[d_weights.num - 1];
		const float offset = d_offsets.array[d_offsets.num - 1];
		da_push_back(weights, &weight);
		da_push_back(offsets, &offset);
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
}

static const char *composite_blur_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("CompositeBlurFilter");
}

static void *composite_blur_create(obs_data_t *settings, obs_source_t *source)
{
	struct composite_blur_filter_data *filter =
		bzalloc(sizeof(struct composite_blur_filter_data));
	filter->context = source;
	filter->radius = 0.0;
	filter->angle = 0.0;
	filter->blur_algorithm = NULL;
	filter->blur_type = NULL;
	filter->rendering = false;
	filter->reload = true;
	filter->param_uv_size = NULL;
	filter->param_dir = NULL;
	filter->param_radius = NULL;
	filter->param_background = NULL;
	filter->video_render = NULL;

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
	if (filter->composite_effect) {
		gs_effect_destroy(filter->composite_effect);
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

	if (filter->reload) {
		filter->reload = false;
		composite_blur_reload_effect(filter);
		obs_source_update_properties(filter->context);
	}

	filter->blur_algorithm =
		obs_data_get_string(settings, "blur_algorithm");

	filter->blur_type = obs_data_get_string(settings, "blur_type");

	filter->radius = (float)obs_data_get_double(settings, "radius");
	calculate_kernel(filter->radius, filter);

	filter->angle = (float)obs_data_get_double(settings, "angle");

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
	filter->video_render = render_video_gaussian;
}

static void get_input_source(struct composite_blur_filter_data *filter)
{
	gs_effect_t *pass_through = obs_get_base_effect(OBS_EFFECT_DEFAULT);

	filter->input_texrender =
		create_or_reset_texrender(filter->input_texrender);
	if (obs_source_process_filter_begin(filter->context, GS_RGBA,
					    OBS_ALLOW_DIRECT_RENDERING) &&
	    gs_texrender_begin(filter->input_texrender, filter->width,
			       filter->height)) {

		set_blending_parameters();
		//set_render_parameters();

		gs_ortho(0.0f, (float)filter->width, 0.0f,
			 (float)filter->height, -100.0f, 100.0f);

		obs_source_process_filter_end(filter->context, pass_through,
					      filter->width, filter->height);
		gs_texrender_end(filter->input_texrender);
		gs_blend_state_pop();
	}
}

static void draw_output_to_source(struct composite_blur_filter_data *filter)
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

	if (filter->rendering) {
		obs_source_skip_video_filter(filter->context);
		return;
	}

	filter->rendering = true;

	// 1. Get the input source as a texture renderer:
	get_input_source(filter);

	// 2. Apply effect to texture, and render texture to video
	filter->video_render(filter);

	// 3. Draw result (filter->output_texrender) to source
	draw_output_to_source(filter);

	filter->rendering = false;
}

static obs_properties_t *composite_blur_properties(void *data)
{
	struct composite_blur_filter_data *filter = data;

	obs_properties_t *props = obs_properties_create();
	obs_properties_set_param(props, filter, NULL);

	obs_property_t *blur_algorithms = obs_properties_add_list(
		props, "blur_algorithm",
		obs_module_text("CompositeBlurFilter.BlurAlgorithm"),
		OBS_COMBO_TYPE_EDITABLE, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(blur_algorithms, "Gaussian", "gaussian");
	obs_property_list_add_string(blur_algorithms, "Box", "box");
	obs_property_list_add_string(blur_algorithms, "Multi-Pass Box",
				     "multi_pass_box");
	obs_property_list_add_string(blur_algorithms, "Kawase", "kawase");

	obs_property_t *blur_types = obs_properties_add_list(
		props, "blur_type",
		obs_module_text("CompositeBlurFilter.BlurType"),
		OBS_COMBO_TYPE_EDITABLE, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(blur_types, "Area", "area");
	obs_property_list_add_string(blur_types, "Directional", "directional");
	obs_property_list_add_string(blur_types, "Zoom", "zoom");
	obs_property_list_add_string(blur_types, "Motion", "motion");
	obs_property_set_modified_callback(blur_types,
					   setting_blur_types_modified);

	obs_properties_add_float_slider(
		props, "radius", obs_module_text("CompositeBlurFilter.Radius"),
		0.0, 83.0, 0.1);

	obs_properties_add_float_slider(
		props, "angle", obs_module_text("CompositeBlurFilter.Angle"),
		-360.0, 360.0, 0.1);

	struct dstr sources_name = {0};

	obs_property_t *p = obs_properties_add_list(
		props, "background",
		obs_module_text("CompositeBlurFilter.Background"),
		OBS_COMBO_TYPE_EDITABLE, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(p, "None", "");
	obs_enum_sources(add_source_to_list, p);
	obs_enum_scenes(add_source_to_list, p);

	return props;
}

static bool setting_blur_types_modified(obs_properties_t *props,
					obs_property_t *p, obs_data_t *settings)
{
	const char *blur_type = obs_data_get_string(settings, "blur_type");
	obs_log(LOG_INFO, blur_type);
	if (strcmp(blur_type, "Area") == 0) {
		return settings_blur_area(props);
	} else if (strcmp(blur_type, "Directional") == 0) {
		return settings_blur_directional(props);
	} else if (strcmp(blur_type, "Zoom") == 0) {
		return settings_blur_zoom(props);
	} else if (strcmp(blur_type, "Motion") == 0) {
		return true;
	}
	return true;
}

static bool settings_blur_area(obs_properties_t *props)
{
	obs_property_t *p = obs_properties_get(props, "radius");
	obs_property_set_enabled(p, true);
	obs_property_set_visible(p, true);
	p = obs_properties_get(props, "angle");
	obs_property_set_enabled(p, false);
	obs_property_set_visible(p, false);
	p = obs_properties_get(props, "background");
	obs_property_set_enabled(p, true);
	obs_property_set_visible(p, true);
	return true;
}

static bool settings_blur_directional(obs_properties_t *props)
{
	obs_property_t *p = obs_properties_get(props, "radius");
	obs_property_set_enabled(p, true);
	obs_property_set_visible(p, true);
	p = obs_properties_get(props, "angle");
	obs_property_set_enabled(p, true);
	obs_property_set_visible(p, true);
	p = obs_properties_get(props, "background");
	obs_property_set_enabled(p, true);
	obs_property_set_visible(p, true);
	return true;
}

static bool settings_blur_zoom(obs_properties_t *props)
{
	obs_property_t *p = obs_properties_get(props, "radius");
	obs_property_set_enabled(p, true);
	obs_property_set_visible(p, true);
	p = obs_properties_get(props, "angle");
	obs_property_set_enabled(p, false);
	obs_property_set_visible(p, false);
	p = obs_properties_get(props, "background");
	obs_property_set_enabled(p, true);
	obs_property_set_visible(p, true);
	return true;
}

static void composite_blur_video_tick(void *data, float seconds)
{
	struct composite_blur_filter_data *filter = data;
	obs_source_t *target = obs_filter_get_target(filter->context);
	if (!target) {
		return;
	}
	filter->width = (uint32_t)obs_source_get_base_width(target);
	filter->height = (uint32_t)obs_source_get_base_height(target);
	filter->uv_size.x = (float)filter->width;
	filter->uv_size.y = (float)filter->height;
}

static void
composite_blur_reload_effect(struct composite_blur_filter_data *filter)
{
	obs_data_t *settings = obs_source_get_settings(filter->context);
	filter->param_uv_size = NULL;

	load_blur_effect(filter);
	load_composite_effect(filter);

	obs_data_release(settings);
}

static void load_blur_effect(struct composite_blur_filter_data *filter)
{
	if (filter->effect != NULL) {
		obs_enter_graphics();
		gs_effect_destroy(filter->effect);
		filter->effect = NULL;
		obs_leave_graphics();
	}
	char *shader_text = NULL;
	struct dstr filename = {0};
	dstr_cat(&filename, obs_get_module_data_path(obs_current_module()));
	dstr_cat(&filename, "/shaders/1d_gaussian.effect");
	shader_text = load_shader_from_file(filename.array);
	obs_log(LOG_INFO, shader_text);
	char *errors = NULL;

	obs_enter_graphics();
	filter->effect = gs_effect_create(shader_text, NULL, &errors);
	obs_leave_graphics();

	bfree(shader_text);
	if (filter->effect == NULL) {
		obs_log(LOG_WARNING,
			"[obs-composite-blur] Unable to load .effect file.  Errors:\n%s",
			(errors == NULL || strlen(errors) == 0 ? "(None)"
							       : errors));
		bfree(errors);
	} else {
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

static void load_composite_effect(struct composite_blur_filter_data *filter)
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
	obs_log(LOG_INFO, shader_text);
	char *errors = NULL;

	obs_enter_graphics();
	filter->composite_effect = gs_effect_create(shader_text, NULL, &errors);
	obs_leave_graphics();

	bfree(shader_text);
	if (filter->composite_effect == NULL) {
		obs_log(LOG_WARNING,
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