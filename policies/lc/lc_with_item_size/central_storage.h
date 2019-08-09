#ifndef _L_CENTRAL_STORAGE_H_
#define _L_CENTRAL_STORAGE_H_

/* right now I am only a counter. */
typedef struct _LCentralStorage_t {
    int64_t communications;
    int64_t fetches;
    int64_t fetch_amount;
    int64_t returns;
    int64_t return_amount;
}LCentralStorage_t;

#endif /* _L_CENTRAL_STORAGE_H_ */
