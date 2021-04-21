#include "hammer/gui.h"
#include "hammer/window.h"

gui_btn_state
gui_btn(gui_btn_state prior_state,
           const char *text,
           size_t len,
           struct btn_opts opts)
{
	if (prior_state) {
		uint32_t swap = opts.background;
		opts.background = opts.foreground;
		opts.foreground = swap;
	}

	float half_height = opts.height / 2;
	float vcenter = opts.yoffset + half_height;
	gui_rect((struct rect_opts) {
		RECT_OPTS_DEFAULTS,
		.color   = opts.background,
		.xoffset = opts.xoffset,
		.yoffset = vcenter - half_height,
		.zoffset = opts.zoffset,
		.width   = opts.width,
		.height  = opts.height
	});
	gui_text_center(text, len, opts.width, (struct text_opts) {
		TEXT_OPTS_DEFAULTS,
		.color   = opts.foreground,
		.style   = opts.style,
		.size    = opts.size,
		.weight  = opts.weight,
		.xoffset = opts.xoffset,
		.yoffset = vcenter - opts.size / 2,
		.zoffset = opts.zoffset + 0.5f
	});
	if (window.mouse_x >= opts.xoffset &&
	    window.mouse_x < opts.xoffset + opts.width &&
            window.mouse_y >= opts.yoffset &&
            window.mouse_y < opts.yoffset + opts.height)
	{
		/*
		 * State machine designed to mimic traditional UI buttons
		 * where a mouse down focusses the element but the button
		 * isn't triggered until mouse up, allowing the user to cancel
		 * a click by dragging off the button before releasing.
		 */
		switch (prior_state) {
		case GUI_BTN_PRESSED:
		case GUI_BTN_HELD:
			if (window.mouse_held[MOUSEBL])
				return GUI_BTN_HELD;
			else
				return GUI_BTN_RELEASED;
		default:
			if (window.mouse_pressed[MOUSEBL])
				return GUI_BTN_PRESSED;
		}
	}
	return 0;
}

void
gui_edit(char *text, size_t maxlen, struct edit_opts opts)
{
	size_t l = strlen(text);
	if (window.mouse_x >= opts.xoffset &&
	    window.mouse_x < opts.xoffset + opts.width &&
            window.mouse_y >= opts.yoffset &&
            window.mouse_y < opts.yoffset + opts.size &&
            window.mouse_held[MOUSEBL])
	{
		gui_element_with_focus = text;
	}
	if (gui_element_with_focus == text) {
		uint32_t swap = opts.background;
		opts.background = opts.foreground;
		opts.foreground = swap;
		/* Update text */
		for (size_t i = 0; i < window.text_input_len; ++ i) {
			switch (window.text_input[i]) {
			case '\n':
				gui_element_with_focus = NULL;
				goto unfocus;
			case '\b':
				if (l > 0)
					text[-- l] = '\0';
				break;
			default:
				if (l >= maxlen-1)
					break;
				/* printable */
				if (window.text_input[i] >= ' ' &&
				    window.text_input[i] <= '~')
				{
					text[l ++] = window.text_input[i];
					text[l] = '\0';
				}
			}
		}
	}
unfocus:
	gui_rect((struct rect_opts) {
		RECT_OPTS_DEFAULTS,
		.color   = opts.background,
		.xoffset = opts.xoffset,
		.yoffset = opts.yoffset,
		.zoffset = opts.zoffset,
		.width   = opts.width,
		.height  = opts.size
	});
	gui_text(text, l, (struct text_opts) {
		TEXT_OPTS_DEFAULTS,
		.color   = opts.foreground,
		.style   = opts.style,
		.size    = opts.size,
		.weight  = opts.weight,
		.xoffset = opts.xoffset,
		.yoffset = opts.yoffset,
		.zoffset = opts.zoffset + 0.5f
	});
}
