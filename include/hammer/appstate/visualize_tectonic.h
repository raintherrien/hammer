#ifndef HAMMER_APPSTATE_VISUALIZE_TECTONIC_H_
#define HAMMER_APPSTATE_VISUALIZE_TECTONIC_H_

#include "hammer/cli.h"
#include "hammer/gui.h"
#include "hammer/worldgen/tectonic.h"

#define PROGRESS_STR_MAX_LEN 64

struct appstate_visualize_tectonic {
	struct rtargs        args;
	struct tectonic_opts tectonic_opts;
	struct lithosphere   lithosphere;
	gui_btn_state        cancel_btn_state;
	GLuint               tectonic_img;
	char                 progress_str[PROGRESS_STR_MAX_LEN];
};

struct appstate appstate_visualize_tectonic_alloc(struct rtargs);

#endif /* HAMMER_APPSTATE_VISUALIZE_TECTONIC_H_ */
