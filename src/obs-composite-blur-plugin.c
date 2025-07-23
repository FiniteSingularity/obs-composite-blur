#include <obs-module.h>

#include "version.h"

extern struct obs_source_info obs_composite_blur;

OBS_DECLARE_MODULE();

OBS_MODULE_USE_DEFAULT_LOCALE("obs-composite-blur", "en-US");

OBS_MODULE_AUTHOR("FiniteSingularity");

bool obs_module_load(void)
{
	blog(LOG_INFO, "[Composite Blur] loaded version %s", PROJECT_VERSION);
	obs_register_source(&obs_composite_blur);

	return true;
}

void obs_module_unload(void) {}

float (*move_get_transition_filter)(obs_source_t *filter_from,
				    obs_source_t **filter_to) = NULL;

void obs_module_post_load()
{
	if (obs_get_module("move-transition") == NULL)
		return;
	proc_handler_t *ph = obs_get_proc_handler();
	struct calldata cd;
	calldata_init(&cd);
	calldata_set_string(&cd, "filter_id", obs_composite_blur.id);
	if (proc_handler_call(ph, "move_get_transition_filter_function", &cd)) {
		move_get_transition_filter = calldata_ptr(&cd, "callback");
	}
	calldata_free(&cd);
}
