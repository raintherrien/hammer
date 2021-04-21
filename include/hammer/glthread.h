#ifndef HAMMER_GLTHREAD_H_
#define HAMMER_GLTHREAD_H_

typedef int(*glthread_callback)(void *);

void glthread_create(void);
void glthread_destroy(void);
int  glthread_execute(glthread_callback fn, void *arg);

#endif /* HAMMER_GLTHREAD_H_ */
