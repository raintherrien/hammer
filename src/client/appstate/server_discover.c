#include "hammer/client/appstate/transition_table.h"
#include "hammer/client/glthread.h"
#include "hammer/client/gui.h"
#include "hammer/client/window.h"
#include "hammer/error.h"
#include "hammer/net.h"
#include "hammer/server_status.h"
#include "hammer/time.h"
#include <deadlock/dl.h>

static struct {
	dltask task;
	gui_btn_state cancel_btn_state;
	time_ns query_server_status_ns;
	enum server_status status;
} server_discover;

static void server_discover_frame_async(DL_TASK_ARGS);
static int  server_discover_gl_setup(void *);
static int  server_discover_gl_frame(void *);

dltask *
appstate_server_discover_enter(void *_)
{
	server_discover.task = DL_TASK_INIT(server_discover_frame_async);
	server_discover.cancel_btn_state = 0;
	server_discover.status = SERVER_STATUS_UNKNOWN;

	/* Query the server's status */
	server_discover.query_server_status_ns = now_ns();
	client_write(NETMSG_TYPE_CLIENT_QUERY_SERVER_STATUS, NULL, 0);

	glthread_execute(server_discover_gl_setup, NULL);

	return &server_discover.task;
}

void
appstate_server_discover_exit(dltask *_)
{
	(void) _; /* Nothing to clean up */
}

static void
server_discover_frame_async(DL_TASK_ARGS)
{
	DL_TASK_ENTRY_VOID;

	if (glthread_execute(server_discover_gl_frame, NULL) ||
	    server_discover.cancel_btn_state == GUI_BTN_RELEASED)
	{
		transition(&client_appstate_mgr, CLIENT_APPSTATE_TRANSITION_MAIN_MENU_OPEN, NULL);
		return;
	}

	/* Respond to any messages */
	enum netmsg_type type;
	size_t sz;
	if (client_peek(&type, &sz)) {
		switch (type) {
		case NETMSG_TYPE_SERVER_RESPONSE_SERVER_STATUS:
			client_read(&server_discover.status, sizeof(server_discover.status));
			time_ns response_ns = now_ns() - server_discover.query_server_status_ns;
			xpinfova("Response to server status query received after %llums", (response_ns) / 1000000);
			switch (server_discover.status) {
			case SERVER_STATUS_CONFIG:
				xpinfo("Server in configurable state");
				xpinfo("Connecting...");
				transition(&client_appstate_mgr, CLIENT_APPSTATE_TRANSITION_CONFIGURE_SERVER, NULL);
				return;

			case SERVER_STATUS_GENERATION:
				xpinfo("Server in generation state");
				break;

			case SERVER_STATUS_RUNNING:
				xpinfo("Server in running state");
				break;

			default:
				errno = EINVAL;
				xperror("Server is in an unknown state");
				break;
			}
			break;

		default:
			errno = EINVAL;
			xperrorva("unexpected netmsg type: %d", type);
			client_discard(sz);
			break;
		}
	}
}

static int
server_discover_gl_setup(void *_)
{
	glClearColor(49 / 255.0f, 59 / 255.0f, 58 / 255.0f, 1);
	return 0;
}

static int
server_discover_gl_frame(void *_)
{
	unsigned font_size = 24;
	unsigned border_padding = 32;
	unsigned element_padding = 8;

	const struct text_opts normal_text_opts = {
		TEXT_OPTS_DEFAULTS,
		.size = font_size
	};

	const struct text_opts bold_text_opts = {
		TEXT_OPTS_DEFAULTS,
		.size = font_size,
		.weight = 255
	};

	const struct text_opts err_text_opts = {
		TEXT_OPTS_DEFAULTS,
		.color = 0xff0000ff,
		.size = font_size
	};

	const struct btn_opts btn_opts = {
		BTN_OPTS_DEFAULTS,
		.width = gui_char_width(font_size) * 8,
		.height = font_size + 16,
		.size = font_size
	};

	gui_container stack;
	gui_stack_init(&stack, (struct stack_opts) {
		STACK_OPTS_DEFAULTS,
		.vpadding = element_padding,
		.xoffset = border_padding,
		.yoffset = border_padding,
		.zoffset = 1
	});
	gui_container_push(&stack);
	gui_text("Discovering server", bold_text_opts);
	gui_stack_break(&stack);
	server_discover.cancel_btn_state = gui_btn(server_discover.cancel_btn_state, "Cancel", btn_opts);
	gui_stack_break(&stack);
	switch (server_discover.status) {
	case SERVER_STATUS_CONFIG:
		gui_text("Connecting to server in configurable state", normal_text_opts);
		break;
	case SERVER_STATUS_GENERATION:
		gui_text("Server is generating; Don't know how to connect to that!", err_text_opts);
		break;
	case SERVER_STATUS_RUNNING:
		gui_text("Server is running; Don't know how to connect to that!", err_text_opts);
		break;
	default:
		gui_text("Server is in an unknown state!", err_text_opts);
		break;
	}
	gui_container_pop();

	return window_submitframe();
}
