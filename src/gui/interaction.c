#include "hammer/gui.h"
#include "hammer/window.h"
#include <assert.h>

gui_btn_state
gui_btn(gui_container  *container,
        gui_btn_state   prior_state,
        const char     *text,
        struct btn_opts opts)
{
	float container_offset[3] = { 0, 0, 0 };
	if (container) {
		float w = opts.xoffset + opts.width;
		float h = opts.yoffset + opts.height;
		gui_container_get_offsets(container, container_offset);
		gui_container_add_element(container, w, h);
	}
	opts.xoffset += container_offset[0];
	opts.yoffset += container_offset[1];
	opts.zoffset += container_offset[2];

	if (prior_state) {
		uint32_t swap = opts.background;
		opts.background = opts.foreground;
		opts.foreground = swap;
	}

	float half_height = opts.height / 2;
	float vcenter = opts.yoffset + half_height;
	gui_rect(NULL, (struct rect_opts) {
		RECT_OPTS_DEFAULTS,
		.color   = opts.background,
		.xoffset = opts.xoffset,
		.yoffset = vcenter - half_height,
		.zoffset = opts.zoffset,
		.width   = opts.width,
		.height  = opts.height
	});
	gui_text_center(NULL, text, opts.width, (struct text_opts) {
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

int
gui_check(gui_container    *container,
          int               prior_state,
          struct check_opts opts)
{
	float container_offset[3] = { 0, 0, 0 };
	if (container) {
		float w = opts.xoffset + opts.width;
		float h = opts.yoffset + opts.height;
		gui_container_get_offsets(container, container_offset);
		gui_container_add_element(container, w, h);
	}
	opts.xoffset += container_offset[0];
	opts.yoffset += container_offset[1];
	opts.zoffset += container_offset[2];

	float half_height = opts.height / 2;
	float vcenter = opts.yoffset + half_height;
	gui_rect(NULL, (struct rect_opts) {
		RECT_OPTS_DEFAULTS,
		.color   = opts.background,
		.xoffset = opts.xoffset,
		.yoffset = vcenter - half_height,
		.zoffset = opts.zoffset,
		.width   = opts.width,
		.height  = opts.height
	});
	if (prior_state) {
		gui_text_center(NULL, "X", opts.width, (struct text_opts) {
			TEXT_OPTS_DEFAULTS,
			.color   = opts.foreground,
			.style   = opts.style,
			.size    = opts.size,
			.weight  = opts.weight,
			.xoffset = opts.xoffset,
			.yoffset = vcenter - opts.size / 2,
			.zoffset = opts.zoffset + 0.5f
		});
	}

	if (window.mouse_x >= opts.xoffset &&
	    window.mouse_x < opts.xoffset + opts.width &&
            window.mouse_y >= opts.yoffset &&
            window.mouse_y < opts.yoffset + opts.height &&
            window.mouse_pressed[MOUSEBL])
	{
		return !prior_state;
	}
	return prior_state;
}

void
gui_edit(gui_container   *container,
         char            *text,
         size_t           maxlen,
         struct edit_opts opts)
{
	float container_offset[3] = { 0, 0, 0 };
	if (container) {
		float w = opts.xoffset + opts.width;
		float h = opts.yoffset + opts.size;
		gui_container_get_offsets(container, container_offset);
		gui_container_add_element(container, w, h);
	}
	opts.xoffset += container_offset[0];
	opts.yoffset += container_offset[1];
	opts.zoffset += container_offset[2];

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
	gui_rect(NULL, (struct rect_opts) {
		RECT_OPTS_DEFAULTS,
		.color   = opts.background,
		.xoffset = opts.xoffset,
		.yoffset = opts.yoffset,
		.zoffset = opts.zoffset,
		.width   = opts.width,
		.height  = opts.size
	});
	gui_text(NULL, text, (struct text_opts) {
		TEXT_OPTS_DEFAULTS,
		.color   = opts.foreground,
		.style   = opts.style,
		.size    = opts.size,
		.weight  = opts.weight,
		.xoffset = opts.xoffset,
		.yoffset = opts.yoffset,
		.zoffset = opts.zoffset + 0.5f /* GLSL epsilon */
	});
}

void
gui_stack_init(gui_container *container, struct stack_opts opts)
{
	container->type = GUI_STACK;
	container->stack = (gui_stack) {
		.hpadding = opts.hpadding,
		.vpadding = opts.vpadding,
		.xoffset = opts.xoffset,
		.yoffset = opts.yoffset,
		.zoffset = opts.zoffset,
		.line_width = 0,
		.line_height = 0,
	};
}

void
gui_container_add_element(gui_container *container, float width, float height)
{
	switch (container->type) {
	default: /*case GUI_STACK:*/
		container->stack.line_width += width;
		container->stack.line_width += container->stack.hpadding;
		if (height > container->stack.line_height)
			container->stack.line_height = height;
	}
}

void
gui_stack_break(gui_container *container)
{
	assert(container->type == GUI_STACK);
	container->stack.yoffset += container->stack.line_height;
	container->stack.yoffset += container->stack.vpadding;
	container->stack.line_width = 0;
	container->stack.line_height = 0;
}

void
gui_container_get_offsets(gui_container *container, float xyz[3])
{
	switch (container->type) {
	default: /*case GUI_STACK:*/
		xyz[0] = container->stack.xoffset + container->stack.line_width;
		xyz[1] = container->stack.yoffset;
		xyz[2] = container->stack.zoffset;
	}
}
