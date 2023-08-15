#include <obs-module.h>
#include <plugin-support.h>

extern struct obs_source_info obs_composite_blur;

OBS_DECLARE_MODULE();

OBS_MODULE_USE_DEFAULT_LOCALE("obs-composite-blur", "en-US");

OBS_MODULE_AUTHOR("FiniteSingularity");

bool obs_module_load(void)
{
	const char *root_path = obs_get_module_data_path(obs_current_module());
	obs_log(LOG_INFO, "Loaded- Composite Blur Plugin (version %s)",
		PLUGIN_VERSION);
	obs_register_source(&obs_composite_blur);

	return true;
}

void obs_module_unload(void) {}
