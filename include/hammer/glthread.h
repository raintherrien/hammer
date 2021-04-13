#ifndef HAMMER_GLTHREAD_H_
#define HAMMER_GLTHREAD_H_

typedef void(*glthread_callback)(void *);

int  glthread_create(void);
void glthread_destroy(void);
void glthread_execute(glthread_callback fn, void *arg);

#endif /* HAMMER_GLTHREAD_H_ */
