#ifndef HAMMER_FILE_H_
#define HAMMER_FILE_H_

/*
 * Returns the contents of the file on success, otherwise sets errno and
 * returns NULL.
 */
char *read_file(const char *filename);

#endif /* HAMMER_FILE_H_ */
