#ifndef HAMMER_CLIENT_GLTHREAD_H_
#define HAMMER_CLIENT_GLTHREAD_H_

#include <GL/glew.h>

typedef int(*glthread_callback)(void *);

void glthread_create(void);
void glthread_destroy(void);
int  glthread_execute(glthread_callback fn, void *arg);

#endif /* HAMMER_CLIENT_GLTHREAD_H_ */
