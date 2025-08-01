#pragma once

#include <obs-module.h>
#include <util/base.h>
#include <util/dstr.h>
#include <util/darray.h>
#include <util/platform.h>
#include <graphics/image-file.h>

#include <stdio.h>

#include "version.h"
#include "obs-utils.h"

#define PLUGIN_INFO                                                                                                 \
	"<a href=\"https://github.com/finitesingularity/obs-composite-blur/\">Composite Blur</a> (" PROJECT_VERSION \
	") by <a href=\"https://twitch.tv/finitesingularity\">FiniteSingularity</a>"

#define ALGO_NONE 0
#define ALGO_NONE_LABEL "None"
#define ALGO_GAUSSIAN 1
#define ALGO_GAUSSIAN_LABEL "CompositeBlurFilter.Algorithm.Gaussian"
#define ALGO_BOX 2
#define ALGO_BOX_LABEL "CompositeBlurFilter.Algorithm.Box"
#define ALGO_DUAL_KAWASE 3
#define ALGO_DUAL_KAWASE_LABEL "CompositeBlurFilter.Algorithm.DualKawase"
#define ALGO_PIXELATE 4
#define ALGO_PIXELATE_LABEL "CompositeBlurFilter.Algorithm.Pixelate"
#define ALGO_TEMPORAL 5
#define ALGO_TEMPORAL_LABEL "CompositeBlurFilter.Algorithm.Temporal"

#define TYPE_NONE 0
#define TYPE_NONE_LABEL "None"
#define TYPE_AREA 1
#define TYPE_AREA_LABEL "CompositeBlurFilter.Type.Area"
#define TYPE_DIRECTIONAL 2
#define TYPE_DIRECTIONAL_LABEL "CompositeBlurFilter.Type.Directional"
#define TYPE_ZOOM 3
#define TYPE_ZOOM_LABEL "CompositeBlurFilter.Type.Zoom"
#define TYPE_MOTION 4
#define TYPE_MOTION_LABEL "CompositeBlurFilter.Type.Motion"
#define TYPE_TILTSHIFT 5
#define TYPE_TILTSHIFT_LABEL "CompositeBlurFilter.Type.TiltShift"
#define TYPE_VECTOR 6
#define TYPE_VECTOR_LABEL "CompositeBlurFilter.Type.Vector"

#define PIXELATE_TYPE_SQUARE 0
#define PIXELATE_TYPE_SQUARE_LABEL "CompositeBlurFilter.Pixelate.Square"
#define PIXELATE_TYPE_HEXAGONAL 1
#define PIXELATE_TYPE_HEXAGONAL_LABEL "CompositeBlurFilter.Pixelate.Hexagonal"
#define PIXELATE_TYPE_TRIAKIS 2
#define PIXELATE_TYPE_TRIAKIS_LABEL "CompositeBlurFilter.Pixelate.Triakis"
#define PIXELATE_TYPE_CIRCLE 3
#define PIXELATE_TYPE_CIRCLE_LABEL "CompositeBlurFilter.Pixelate.Circle"
#define PIXELATE_TYPE_TRIANGLE 4
#define PIXELATE_TYPE_TRIANGLE_LABEL "CompositeBlurFilter.Pixelate.Triangle"
#define PIXELATE_TYPE_VORONOI 5
#define PIXELATE_TYPE_VORONOI_LABEL "CompositeBlurFilter.Pixelate.Voronoi"
#define PIXELATE_TYPE_RHOMBOID 6
#define PIXELATE_TYPE_RHOMBOID_LABEL "CompositeBlurFilter.Pixelate.Rhomboid"

#define EFFECT_MASK_TYPE_NONE 0
#define EFFECT_MASK_TYPE_NONE_LABEL "CompositeBlurFilter.EffectMask.None"
#define EFFECT_MASK_TYPE_CROP 1
#define EFFECT_MASK_TYPE_CROP_LABEL "CompositeBlurFilter.EffectMask.Crop"
#define EFFECT_MASK_TYPE_RECT 2
#define EFFECT_MASK_TYPE_RECT_LABEL "CompositeBlurFilter.EffectMask.Rectangle"
#define EFFECT_MASK_TYPE_CIRCLE 3
#define EFFECT_MASK_TYPE_CIRCLE_LABEL "CompositeBlurFilter.EffectMask.Circle"
#define EFFECT_MASK_TYPE_SOURCE 4
#define EFFECT_MASK_TYPE_SOURCE_LABEL "CompositeBlurFilter.EffectMask.Source"
#define EFFECT_MASK_TYPE_IMAGE 5
#define EFFECT_MASK_TYPE_IMAGE_LABEL "CompositeBlurFilter.EffectMask.Image"

#define EFFECT_MASK_SOURCE_FILTER_ALPHA 0
#define EFFECT_MASK_SOURCE_FILTER_ALPHA_LABEL \
	"CompositeBlurFilter.EffectMask.Source.Alpha"
#define EFFECT_MASK_SOURCE_FILTER_GRAYSCALE 1
#define EFFECT_MASK_SOURCE_FILTER_GRAYSCALE_LABEL \
	"CompositeBlurFilter.EffectMask.Source.Grayscale"
#define EFFECT_MASK_SOURCE_FILTER_LUMINOSITY 2
#define EFFECT_MASK_SOURCE_FILTER_LUMINOSITY_LABEL \
	"CompositeBlurFilter.EffectMask.Source.Luminosity"
#define EFFECT_MASK_SOURCE_FILTER_SLIDERS 3
#define EFFECT_MASK_SOURCE_FILTER_SLIDERS_LABEL \
	"CompositeBlurFilter.EffectMask.Source.Sliders"

#define GRADIENT_TYPE_SOBEL 0
#define GRADIENT_TYPE_SOBEL_LABEL \
	"CompositeBlurFilter.VectorBlur.GradientType.Sobel"
#define GRADIENT_TYPE_FORWARD_DIFF 1
#define GRADIENT_TYPE_FORWARD_DIFF_LABEL \
	"CompositeBlurFilter.VectorBlur.GradientType.ForwardDifference"
#define GRADIENT_TYPE_CENTRAL_LIMIT_DIFF 2
#define GRADIENT_TYPE_CENTRAL_LIMIT_DIFF_LABEL \
	"CompositeBlurFilter.VectorBlur.GradientType.CentralLimitDifference"

#define GRADIENT_CHANNEL_RED 0
#define GRADIENT_CHANNEL_RED_LABEL \
	"CompositeBlurFilter.VectorBlur.Channel.Red"
#define GRADIENT_CHANNEL_GREEN 1
#define GRADIENT_CHANNEL_GREEN_LABEL \
	"CompositeBlurFilter.VectorBlur.Channel.Green"
#define GRADIENT_CHANNEL_BLUE 2
#define GRADIENT_CHANNEL_BLUE_LABEL \
	"CompositeBlurFilter.VectorBlur.Channel.Blue"
#define GRADIENT_CHANNEL_ALPHA 3
#define GRADIENT_CHANNEL_ALPHA_LABEL \
	"CompositeBlurFilter.VectorBlur.Channel.Alpha"
#define GRADIENT_CHANNEL_LUMINANCE 4
#define GRADIENT_CHANNEL_LUMINANCE_LABEL \
	"CompositeBlurFilter.VectorBlur.Channel.Luminance"
#define GRADIENT_CHANNEL_SATURATION 5
#define GRADIENT_CHANNEL_SATURATION_LABEL \
	"CompositeBlurFilter.VectorBlur.Channel.Saturation"

typedef DARRAY(float) fDarray;

struct composite_blur_filter_data;
typedef struct composite_blur_filter_data composite_blur_filter_data_t;

struct composite_blur_filter_data {
	obs_source_t *context;
	struct dstr filter_name;

	// Effects
	gs_effect_t *effect;
	gs_effect_t *effect_2;
	gs_effect_t *composite_effect;
	gs_effect_t *pixelate_effect;
	gs_effect_t *mix_effect;
	gs_effect_t *effect_mask_effect;
	gs_effect_t *output_effect;
	gs_effect_t *gradient_effect;
	gs_effect_t *gv_effect;

	// Render pipeline
	bool input_rendered;
	gs_texrender_t *input_texrender;
	bool output_rendered;
	gs_texrender_t *output_texrender;
	// Frame Buffers
	gs_texrender_t *render;
	gs_texrender_t *render2;
	gs_texrender_t *background_texrender;
	// Renderer for composite render step
	gs_texrender_t *composite_render;

	// Renderers for vector blur
	gs_texrender_t* vb_gradient;
	gs_texrender_t* vb_smoothed_gradient;

	obs_hotkey_pair_id hotkey;

	bool rendering;
	bool reload;
	bool rendered;

	float time;

	// Blur Filter Common
	int blur_algorithm;
	int blur_algorithm_last;
	int blur_type;
	int blur_type_last;

	gs_eparam_t *param_uv_size;
	struct vec2 uv_size;
	gs_eparam_t *param_radius;
	float radius;
	float radius_last;
	float inactive_radius;
	gs_eparam_t *param_texel_step;
	struct vec2 texel_step;

	// Gaussian Blur
	gs_eparam_t *param_kernel_size;
	size_t kernel_size;
	gs_eparam_t *param_offset;
	fDarray offset;
	gs_eparam_t *param_weight;
	fDarray kernel;
	gs_eparam_t *param_kernel_texture;
	gs_texture_t *kernel_texture;
	gs_eparam_t *param_gradient_image;
	gs_eparam_t *param_gradient_channel;
	gs_eparam_t *param_gradient_uv_size;
	gs_eparam_t *param_gradient_map;
	int vector_blur_channel;
	float vector_blur_amount;
	float vector_blur_smoothing;
	int vector_blur_type;
	float last_vector_blur_amount;
	obs_weak_source_t* vector_blur_source;
	gs_eparam_t* param_inactive_radius;

	// Box Blur
	int passes;

	// Kawase Blur
	float kawase_passes;

	// Pixelate Blur
	int pixelate_type;
	int pixelate_type_last;
	struct vec2 pixelate_tessel_center;
	float pixelate_tessel_rot;
	float pixelate_smoothing_pct;
	float pixelate_cos_theta;
	float pixelate_sin_theta;
	float pixelate_cos_rtheta;
	float pixelate_sin_rtheta;
	bool pixelate_animate;
	float pixelate_animation_speed;
	float pixelate_animation_time;
	gs_eparam_t *param_pixel_size;
	gs_eparam_t *param_pixel_center;
	gs_eparam_t *param_pixel_rot;
	gs_eparam_t *param_pixel_cos_theta;
	gs_eparam_t *param_pixel_sin_theta;
	gs_eparam_t *param_pixel_cos_rtheta;
	gs_eparam_t *param_pixel_sin_rtheta;
	gs_eparam_t* param_pixel_time;
	gs_texrender_t *pixelate_texrender;

	// Radial Blur
	gs_eparam_t *param_radial_center;
	float center_x;
	float center_y;

	// Motion/Directional Blur
	float angle;

	// Tilt-Shift
	gs_eparam_t *param_focus_width;
	float tilt_shift_width;
	gs_eparam_t *param_focus_center;
	float tilt_shift_center;
	gs_eparam_t *param_focus_angle;
	float tilt_shift_angle;

	// Temporal
	gs_eparam_t* param_temporal_image;
	gs_eparam_t* param_temporal_prior_image;
	// gs_texture_t* prior_image_texture;
	gs_eparam_t* param_temporal_current_weight;
	gs_eparam_t* param_temporal_clear_threshold;
	float temporal_current_weight;
	float temporal_clear_threshold;
	bool temporal_prior_stored;

	// Compositing
	gs_eparam_t *param_background;
	obs_weak_source_t *background;
	struct dstr background_source_name;
	bool has_background_source;

	// Mask
	int mask_type;
	int mask_type_last;
	gs_eparam_t *param_filtered_image;
	float mask_crop_left;
	float mask_crop_right;
	float mask_crop_top;
	float mask_crop_bot;
	gs_eparam_t *param_mask_crop_scale;
	gs_eparam_t *param_mask_crop_offset;
	gs_eparam_t *param_mask_crop_box_aspect_ratio;
	gs_eparam_t *param_mask_crop_corner_radius;
	float mask_crop_corner_radius;
	gs_eparam_t *param_mask_crop_feathering;
	float mask_crop_feathering;
	gs_eparam_t *param_mask_crop_invert;
	bool mask_crop_invert;
	bool has_mask_source;
	struct dstr mask_source_name;
	int mask_source_filter_type;
	float mask_source_filter_red;
	float mask_source_filter_green;
	float mask_source_filter_blue;
	float mask_source_filter_alpha;
	gs_eparam_t *param_mask_source_alpha_source;
	gs_eparam_t *param_mask_source_rgba_weights;
	gs_eparam_t *param_mask_source_multiplier;
	float mask_source_multiplier;
	gs_eparam_t *param_mask_source_invert;
	bool mask_source_invert;
	obs_weak_source_t *mask_source_source;
	gs_eparam_t *param_mask_circle_center;
	float mask_circle_center_x;
	float mask_circle_center_y;
	gs_eparam_t *param_mask_circle_radius;
	float mask_circle_radius;
	gs_eparam_t *param_mask_circle_feathering;
	float mask_circle_feathering;
	gs_eparam_t *param_mask_circle_inv;
	bool mask_circle_inv;
	gs_eparam_t *param_mask_circle_uv_scale;
	float mask_rect_center_x;
	float mask_rect_center_y;
	float mask_rect_width;
	float mask_rect_height;
	float mask_rect_corner_radius;
	float mask_rect_feathering;
	float mask_rect_inv;
	gs_image_file_t *mask_image;

	// Output Effect Parameters
	gs_eparam_t *param_output_image;

	uint32_t width;
	uint32_t height;

	uint32_t device_type;

	// Callback Functions
	void (*video_render)(composite_blur_filter_data_t *filter);
	void (*load_effect)(composite_blur_filter_data_t *filter);
	void (*update)(composite_blur_filter_data_t *filter);
};

static const char *composite_blur_name(void *type_data);
static void *composite_blur_create(obs_data_t *settings, obs_source_t *source);
static void composite_blur_defaults(obs_data_t *settings);
static void composite_blur_destroy(void *data);
static uint32_t composite_blur_width(void *data);
static uint32_t composite_blur_height(void *data);
static void composite_blur_rename(void *data, calldata_t *call_data);
static void composite_blur_update(void *data, obs_data_t *settings);
static void composite_blur_video_render(void *data, gs_effect_t *effect);
static void composite_blur_video_tick(void *data, float seconds);
static obs_properties_t *composite_blur_properties(void *data);
static void composite_blur_reload_effect(composite_blur_filter_data_t *filter);
static void load_composite_effect(composite_blur_filter_data_t *filter);
static void load_mix_effect(composite_blur_filter_data_t *filter);
static void load_output_effect(composite_blur_filter_data_t *filter);
extern gs_texture_t *blend_composite(gs_texture_t *texture,
				     composite_blur_filter_data_t *data);

static bool setting_blur_algorithm_modified(void *data, obs_properties_t *props,
					    obs_property_t *p,
					    obs_data_t *settings);
static bool setting_effect_mask_modified(obs_properties_t *props,
					 obs_property_t *p,
					 obs_data_t *settings);
static bool setting_blur_types_modified(void *data, obs_properties_t *props,
					obs_property_t *p,
					obs_data_t *settings);
static bool setting_pixelate_animate_modified(obs_properties_t* props,
					obs_property_t* p,
					obs_data_t* settings);
static void setting_visibility(const char *prop_name, bool visible,
			       obs_properties_t *props);
static void set_blur_radius_settings(const char *name, float min_val,
				     float max_val, float step_size,
				     obs_properties_t *props);
static bool settings_blur_area(obs_properties_t *props, obs_data_t *settings);
static bool settings_blur_directional(obs_properties_t *props);
static bool settings_blur_zoom(obs_properties_t *props);
static bool settings_blur_tilt_shift(obs_properties_t *props);
static bool settings_blur_vector(obs_properties_t* props, composite_blur_filter_data_t* filter);

static void apply_effect_mask(composite_blur_filter_data_t *filter);
static void apply_effect_mask_crop(composite_blur_filter_data_t *filter);
static void apply_effect_mask_source(composite_blur_filter_data_t *filter);
static void apply_effect_mask_circle(composite_blur_filter_data_t *filter);
static void apply_effect_mask_rect(composite_blur_filter_data_t *filter);
static void load_crop_mask_effect(composite_blur_filter_data_t *filter);
static void load_source_mask_effect(composite_blur_filter_data_t *filter);
static void load_circle_mask_effect(composite_blur_filter_data_t *filter);
static void effect_mask_load_effect(composite_blur_filter_data_t *filter);

static bool setting_effect_mask_source_filter_modified(obs_properties_t *props,
						       obs_property_t *p,
						       obs_data_t *settings);
extern void get_background(composite_blur_filter_data_t *data);

extern float (*move_get_transition_filter)(obs_source_t *filter_from,
					   obs_source_t **filter_to);
