#include "hammer/window.h"
#include "hammer/error.h"
#include "hammer/vector.h"
#include <errno.h>

struct window window;

#ifdef HAMMER_DEBUG_OPENGL
static void
print_GLenum(GLenum e)
{
#define PRINT_ENUM_CASE(e) case e: fprintf(stderr, "[OPENGL] \t" #e "\n"); break
	switch (e) {
	PRINT_ENUM_CASE(GL_INVALID_ENUM);
	PRINT_ENUM_CASE(GL_INVALID_VALUE);
	PRINT_ENUM_CASE(GL_INVALID_OPERATION);
	PRINT_ENUM_CASE(GL_STACK_OVERFLOW);
	PRINT_ENUM_CASE(GL_STACK_UNDERFLOW);
	PRINT_ENUM_CASE(GL_OUT_OF_MEMORY);
	PRINT_ENUM_CASE(GL_INVALID_FRAMEBUFFER_OPERATION);
	PRINT_ENUM_CASE(GL_CONTEXT_LOST);
	PRINT_ENUM_CASE(GL_TABLE_TOO_LARGE);
	PRINT_ENUM_CASE(GL_DEBUG_SEVERITY_HIGH);
	PRINT_ENUM_CASE(GL_DEBUG_SEVERITY_MEDIUM);
	PRINT_ENUM_CASE(GL_DEBUG_SEVERITY_LOW);
	PRINT_ENUM_CASE(GL_DEBUG_SEVERITY_NOTIFICATION);
	PRINT_ENUM_CASE(GL_DEBUG_SOURCE_API);
	PRINT_ENUM_CASE(GL_DEBUG_SOURCE_WINDOW_SYSTEM);
	PRINT_ENUM_CASE(GL_DEBUG_SOURCE_SHADER_COMPILER);
	PRINT_ENUM_CASE(GL_DEBUG_SOURCE_THIRD_PARTY);
	PRINT_ENUM_CASE(GL_DEBUG_SOURCE_APPLICATION);
	PRINT_ENUM_CASE(GL_DEBUG_SOURCE_OTHER);
	PRINT_ENUM_CASE(GL_DEBUG_TYPE_ERROR);
	PRINT_ENUM_CASE(GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR);
	PRINT_ENUM_CASE(GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR);
	PRINT_ENUM_CASE(GL_DEBUG_TYPE_PORTABILITY);
	PRINT_ENUM_CASE(GL_DEBUG_TYPE_PERFORMANCE);
	PRINT_ENUM_CASE(GL_DEBUG_TYPE_MARKER);
	PRINT_ENUM_CASE(GL_DEBUG_TYPE_PUSH_GROUP);
	PRINT_ENUM_CASE(GL_DEBUG_TYPE_POP_GROUP);
	PRINT_ENUM_CASE(GL_DEBUG_TYPE_OTHER);
	}
#undef PRINT_ENUM_CASE
}

static void
gl_debug_callback(
	GLenum        src,
	GLenum        type,
	GLuint        id,
	GLenum        severity,
	GLsizei       len,
	const GLchar *msg,
	const void   *userParam
) {
	(void) id;
	(void) userParam;
	fprintf(stderr, "[OPENGL] %.*s\n", len, msg);
	print_GLenum(severity);
	print_GLenum(src);
	print_GLenum(type);
}
#endif

static void
opengl_init(void)
{
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

#ifdef HAMMER_DEBUG_OPENGL
	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback(gl_debug_callback, NULL);
	/* Report all but GL_DEBUG_SEVERITY_NOTIFICATION */
	glDebugMessageControl(GL_DONT_CARE, /* source */
	                      GL_DONT_CARE, /* type */
	                      GL_DONT_CARE, /* severity */
	                      0,            /* message id count */
	                      NULL,         /* message ids ptr */
	                      GL_TRUE);     /* enabled */
	glDebugMessageControl(GL_DONT_CARE,
	                      GL_DONT_CARE,
	                      GL_DEBUG_SEVERITY_NOTIFICATION,
	                      0,
	                      NULL,
	                      GL_FALSE);
#endif
}

void
window_create(void)
{
	int r = SDL_Init(SDL_INIT_VIDEO);
	if (r) {
		errno = r;
		xpanicva("Error initializing SDL %s", SDL_GetError());
	}

	Uint32 flags  = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
	window.width  = 800;
	window.height = 600;
	window.handle = SDL_CreateWindow("Hammer",
	                                 SDL_WINDOWPOS_UNDEFINED,
	                                 SDL_WINDOWPOS_UNDEFINED,
	                                 window.width,
	                                 window.height,
	                                 flags);
	if (!window.handle) {
		errno = -1;
		xpanicva("Error creating SDL window %s", SDL_GetError());
	}

	/* OpenGL 4.3 core profile */
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	window.glcontext = SDL_GL_CreateContext(window.handle);
	if (!window.glcontext) {
		errno = -1;
		xpanicva("Error creating OpenGL context %s", SDL_GetError());
	}

	if (SDL_GL_SetSwapInterval(-1) == -1) {
		errno = -1;
		xperrorva("Error adaptive vsync unsupported %s", SDL_GetError());
	}

	glewExperimental = GL_TRUE;
	GLenum glewrcode = glewInit();
	if (glewrcode != GLEW_OK) {
		errno = glewrcode;
		xpanicva("Error initializing GLEW %s", glewGetErrorString(glewrcode));
	}

	opengl_init();
	glViewport(0, 0, window.width, window.height);

	window.keydown = SDL_GetKeyboardState(NULL);
}

void
window_destroy(void)
{
	vector_free(&window.frame_events);
	SDL_GL_DeleteContext(window.glcontext);
	SDL_DestroyWindow(window.handle);
	SDL_Quit();
}

int
window_startframe(void)
{
	size_t old_frame = window.current_frame % FRAMES_IN_FLIGHT;
	size_t new_frame = (++ window.current_frame) % FRAMES_IN_FLIGHT;

	/* Finish last frame, create fence */
	SDL_GL_SwapWindow(window.handle);
	window.frames[old_frame].fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

	/* Wait for next frame in ring buffer to complete */
	GLsync recycling_fence = window.frames[new_frame].fence;
	if (recycling_fence) {
		GLenum status = GL_TIMEOUT_EXPIRED;
		while (status == GL_TIMEOUT_EXPIRED) {
			status = glClientWaitSync(recycling_fence,
						  GL_SYNC_FLUSH_COMMANDS_BIT,
						  (GLuint64)-1);
			if (status == GL_WAIT_FAILED)
				xpanic("Error waiting on frame fence");
		}
		glDeleteSync(recycling_fence);
	}

	/* Begin this frame */
	window.frames[new_frame].id = new_frame;
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	int quit = 0;

	window.motion_x = 0;
	window.motion_y = 0;

	vector_clear(&window.frame_events);

	SDL_Event e;
	while (SDL_PollEvent(&e)) {
		vector_push(&window.frame_events, e);
		switch (e.type) {
		case SDL_QUIT:
			quit = 1;
			break;
		case SDL_WINDOWEVENT:
			if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
				glViewport(0, 0, e.window.data1, e.window.data2);
			break;
		case SDL_MOUSEMOTION:
			window.motion_x += e.motion.xrel;
			window.motion_y += e.motion.yrel;
			break;
		}
	}

	/*
	 * TODO: Returns {0,0}, until a mouse event occurs.
	 * We should throw away these values until that happens.
	 */
	SDL_GetMouseState(&window.mouse_x, &window.mouse_y);

	return quit;
}
