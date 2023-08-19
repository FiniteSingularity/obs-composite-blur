#include <obs-module.h>

#include "version.h"

extern struct obs_source_info obs_composite_blur;

OBS_DECLARE_MODULE();

OBS_MODULE_USE_DEFAULT_LOCALE("obs-composite-blur", "en-US");

OBS_MODULE_AUTHOR("FiniteSingularity");

bool obs_module_load(void)
{
	blog(LOG_INFO, "[OBS Composite Blur] loaded version %s",
	     PROJECT_VERSION);
	obs_register_source(&obs_composite_blur);

	return true;
}

void obs_module_unload(void) {}
