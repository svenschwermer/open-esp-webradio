#ifndef COMMON_H_
#define COMMON_H_

#define ARRAY_SIZE(x) ((sizeof(x)) / (sizeof((x)[0])))

static inline int min(int a, int b) { return (a < b) ? a : b; }

#endif /* COMMON_H_ */
