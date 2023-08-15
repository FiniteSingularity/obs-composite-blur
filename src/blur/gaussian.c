#include "gaussian.h"

void blur_types(struct composite_blur_filter_data *data) {}

void render_video_gaussian(struct composite_blur_filter_data *data)
{
	if (strcmp(data->blur_type, "Area") == 0) {
		gaussian_area_blur(data);
	} else if (strcmp(data->blur_type, "Directional") == 0) {
		gaussian_directional_blur(data);
	}
}

/*
 *  Performs an area blur using the gaussian kernel.  Blur is
 *  equal in both x and y directions.
 */
static void gaussian_area_blur(struct composite_blur_filter_data *data)
{
	gs_effect_t *effect = data->effect;
	gs_effect_t *composite_effect = data->composite_effect;

	gs_texture_t *texture = gs_texrender_get_texture(data->input_texrender);

	if (!effect || !texture) {
		return;
	}

	texture = blend_composite(texture, data);

	data->render2 = create_or_reset_texrender(data->render2);

	gs_eparam_t *image = gs_effect_get_param_by_name(effect, "image");
	gs_effect_set_texture(image, texture);

	gs_eparam_t *weight = gs_effect_get_param_by_name(effect, "weight");

	gs_effect_set_val(weight, data->kernel.array,
			  data->kernel.num * sizeof(float));

	gs_eparam_t *offset = gs_effect_get_param_by_name(effect, "offset");
	gs_effect_set_val(offset, data->offset.array,
			  data->offset.num * sizeof(float));

	const int k_size = (int)data->kernel_size;
	gs_eparam_t *kernel_size =
		gs_effect_get_param_by_name(effect, "kernel_size");
	gs_effect_set_int(kernel_size, k_size);

	gs_eparam_t *texel_step =
		gs_effect_get_param_by_name(effect, "texel_step");
	struct vec2 direction;

	// 1. First pass- apply 1D blur kernel to horizontal dir.

	direction.x = 1.0f / data->width;
	direction.y = 0.0f;
	gs_effect_set_vec2(texel_step, &direction);

	set_blending_parameters();
	//set_render_parameters();

	if (gs_texrender_begin(data->render2, data->width, data->height)) {
		while (gs_effect_loop(effect, "Draw"))
			gs_draw_sprite(texture, 0, data->width, data->height);
		gs_texrender_end(data->render2);
	}

	// 2. Save texture from first pass in variable "texture"
	texture = gs_texrender_get_texture(data->render2);

	// 3. Second Pass- Apply 1D blur kernel vertically.
	image = gs_effect_get_param_by_name(effect, "image");
	gs_effect_set_texture(image, texture);

	direction.x = 0.0f;
	direction.y = 1.0f / data->height;
	gs_effect_set_vec2(texel_step, &direction);

	data->output_texrender =
		create_or_reset_texrender(data->output_texrender);

	if (gs_texrender_begin(data->output_texrender, data->width,
			       data->height)) {
		while (gs_effect_loop(effect, "Draw"))
			gs_draw_sprite(texture, 0, data->width, data->height);
		gs_texrender_end(data->output_texrender);
	}

	gs_blend_state_pop();
}

/*
 *  Performs a directional blur using the gaussian kernel.  Blur is
 *  equal in both x and y directions.
 */
static void gaussian_directional_blur(struct composite_blur_filter_data *data)
{
	gs_effect_t *effect = data->effect;
	gs_effect_t *composite_effect = data->composite_effect;

	gs_texture_t *texture = gs_texrender_get_texture(data->input_texrender);

	if (!effect || !texture) {
		return;
	}

	texture = blend_composite(texture, data);

	gs_eparam_t *image = gs_effect_get_param_by_name(effect, "image");
	gs_effect_set_texture(image, texture);

	gs_eparam_t *weight = gs_effect_get_param_by_name(effect, "weight");

	gs_effect_set_val(weight, data->kernel.array,
			  data->kernel.num * sizeof(float));

	gs_eparam_t *offset = gs_effect_get_param_by_name(effect, "offset");
	gs_effect_set_val(offset, data->offset.array,
			  data->offset.num * sizeof(float));

	const int k_size = (int)data->kernel_size;
	gs_eparam_t *kernel_size =
		gs_effect_get_param_by_name(effect, "kernel_size");
	gs_effect_set_int(kernel_size, k_size);

	gs_eparam_t *texel_step =
		gs_effect_get_param_by_name(effect, "texel_step");
	struct vec2 direction;

	// 1. Single pass- blur only in one direction
	float rads = -data->angle * (M_PI / 180.0f);
	direction.x = (float)cos(rads) / data->width;
	direction.y = (float)sin(rads) / data->height;
	gs_effect_set_vec2(texel_step, &direction);

	set_blending_parameters();
	//set_render_parameters();

	data->output_texrender =
		create_or_reset_texrender(data->output_texrender);

	if (gs_texrender_begin(data->output_texrender, data->width,
			       data->height)) {
		while (gs_effect_loop(effect, "Draw"))
			gs_draw_sprite(texture, 0, data->width, data->height);
		gs_texrender_end(data->output_texrender);
	}

	gs_blend_state_pop();
}