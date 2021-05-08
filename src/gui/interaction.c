#include "hammer/gui.h"
#include "hammer/window.h"
#include <assert.h>

gui_btn_state
gui_btn(gui_btn_state   prior_state,
        const char     *text,
        struct btn_opts opts)
{
	if (prior_state) {
		uint32_t swap = opts.background;
		opts.background = opts.foreground;
		opts.foreground = swap;
	}

	float half_height = opts.height / 2;
	float vcenter = opts.yoffset + half_height;

	gui_container s = gui_current_container_get_state();
	gui_text_center(text, opts.width, (struct text_opts) {
		TEXT_OPTS_DEFAULTS,
		.color   = opts.foreground,
		.style   = opts.style,
		.size    = opts.size,
		.weight  = opts.weight,
		.xoffset = opts.xoffset,
		.yoffset = vcenter - opts.size / 2,
		.zoffset = opts.zoffset + 0.5f
	});
	gui_current_container_set_state(s);
	float container_offset[3];
	gui_current_container_get_offsets(container_offset);
	gui_rect((struct rect_opts) {
		RECT_OPTS_DEFAULTS,
		.color   = opts.background,
		.xoffset = opts.xoffset,
		.yoffset = vcenter - half_height,
		.zoffset = opts.zoffset,
		.width   = opts.width,
		.height  = opts.height
	});

	if (window.mouse_x >= container_offset[0] + opts.xoffset &&
	    window.mouse_x <  container_offset[0] + opts.xoffset + opts.width &&
            window.mouse_y >= container_offset[1] + opts.yoffset &&
            window.mouse_y <  container_offset[1] + opts.yoffset + opts.height)
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
			if (window.unhandled_mouse_press[MOUSEBL]) {
				gui_element_with_focus = NULL;
				window.unhandled_mouse_press[MOUSEBL] = 0;
				return GUI_BTN_PRESSED;
			}
		}
	}
	return 0;
}

int
gui_check(int               prior_state,
          struct check_opts opts)
{
	float half_height = opts.height / 2;
	float vcenter = opts.yoffset + half_height;
	if (prior_state) {
		gui_container s = gui_current_container_get_state();
		gui_text_center("X", opts.width, (struct text_opts) {
			TEXT_OPTS_DEFAULTS,
			.color   = opts.foreground,
			.style   = opts.style,
			.size    = opts.size,
			.weight  = opts.weight,
			.xoffset = opts.xoffset,
			.yoffset = vcenter - opts.size / 2,
			.zoffset = opts.zoffset + 0.5f
		});
		gui_current_container_set_state(s);
	}
	float container_offset[3];
	gui_current_container_get_offsets(container_offset);
	gui_rect((struct rect_opts) {
		RECT_OPTS_DEFAULTS,
		.color   = opts.background,
		.xoffset = opts.xoffset,
		.yoffset = vcenter - half_height,
		.zoffset = opts.zoffset,
		.width   = opts.width,
		.height  = opts.height
	});

	if (window.mouse_x >= container_offset[0] + opts.xoffset &&
	    window.mouse_x <  container_offset[0] + opts.xoffset + opts.width &&
            window.mouse_y >= container_offset[1] + opts.yoffset &&
            window.mouse_y <  container_offset[1] + opts.yoffset + opts.height &&
            window.unhandled_mouse_press[MOUSEBL])
	{
		gui_element_with_focus = NULL;
		window.unhandled_mouse_press[MOUSEBL] = 0;
		return !prior_state;
	}
	return prior_state;
}

void
gui_edit(char            *text,
         size_t           maxlen,
         struct edit_opts opts)
{
	float container_offset[3];
	gui_current_container_get_offsets(container_offset);
	size_t l = strlen(text);
	if (window.mouse_x >= container_offset[0] + opts.xoffset &&
	    window.mouse_x <  container_offset[0] + opts.xoffset + opts.width &&
            window.mouse_y >= container_offset[1] + opts.yoffset &&
            window.mouse_y <  container_offset[1] + opts.yoffset + opts.size &&
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
unfocus: ;
	gui_container s = gui_current_container_get_state();
	gui_text(text, (struct text_opts) {
		TEXT_OPTS_DEFAULTS,
		.xoffset = -1 * opts.width,
		.color   = opts.foreground,
		.style   = opts.style,
		.size    = opts.size,
		.weight  = opts.weight,
		.xoffset = opts.xoffset,
		.yoffset = opts.yoffset,
		.zoffset = opts.zoffset + 0.5f /* GLSL epsilon */
	});
	gui_current_container_set_state(s);
	gui_rect((struct rect_opts) {
		RECT_OPTS_DEFAULTS,
		.color   = opts.background,
		.xoffset = opts.xoffset,
		.yoffset = opts.yoffset,
		.zoffset = opts.zoffset,
		.width   = opts.width,
		.height  = opts.size
	});
}

void
gui_window_init(gui_container *container, struct window_opts opts)
{
	container->type = GUI_WINDOW;
	container->window = (gui_window) {
		.hpadding = opts.hpadding,
		.vpadding = opts.vpadding,
		.xoffset = opts.xoffset,
		.yoffset = opts.yoffset,
		.zoffset = opts.zoffset
	};
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
gui_stack_break(gui_container *container)
{
	assert(container->type == GUI_STACK);
	container->stack.yoffset += container->stack.line_height;
	container->stack.yoffset += container->stack.vpadding;
	container->stack.line_width = 0;
	container->stack.line_height = 0;
}

void
gui_container_handle(gui_container *c)
{
	switch (c->type) {
	case GUI_WINDOW:
		/* TODO: Window interaction, etc. */
		return;
	default: /*case GUI_STACK:*/
		return;
	}
}

void
gui_container_push(gui_container *c)
{
	c->parent = window.current_container;
	window.current_container = c;
}

void
gui_container_pop(void)
{
	gui_container_handle(window.current_container);
	gui_container *parent = window.current_container->parent;
	window.current_container->parent = NULL;
	window.current_container = parent;
}

void
gui_current_container_add_element(float width, float height)
{
	gui_container *container = window.current_container;
	switch (container->type) {
	case GUI_WINDOW:
		break;
	default: /*case GUI_STACK:*/
		container->stack.line_width += width;
		container->stack.line_width += container->stack.hpadding;
		if (height > container->stack.line_height)
			container->stack.line_height = height;
	}
}

void
gui_current_container_get_offsets(float xyz[3])
{
	gui_container *container = window.current_container;
	switch (container->type) {
	case GUI_WINDOW:
		xyz[0] = container->stack.xoffset;
		xyz[1] = container->stack.yoffset;
		xyz[2] = container->stack.zoffset;
		break;
	default: /*case GUI_STACK:*/
		xyz[0] = container->stack.xoffset + container->stack.line_width;
		xyz[1] = container->stack.yoffset;
		xyz[2] = container->stack.zoffset;
	}
}

gui_container
gui_current_container_get_state(void)
{
	return *window.current_container;
}

void
gui_current_container_set_state(gui_container c)
{
	*window.current_container = c;
}
