#ifndef _L_ATTRS_H_
#define _L_ATTRS_H_

/* the optimization of if-statement branches */
#define LIKELY(x)       __builtin_expect((x),1)
#define UNLIKELY(x)     __builtin_expect((x),0)

/**
 * An attribute that will cause a variable or field to be aligned so that
 * it doesn't have false sharing with anything at a smaller memory address.
 */
#define ATTR_ALIGN_TO_AVOID_FALSE_SHARING __attribute__((__aligned__(128)))

#endif /* _L_ATTRS_H_ */
