#ifndef _STATIC_H_
#define _STATIC_H_

#define NUM_SLAB_CLASS 1

size_t slab_class_to_chunk_size( int32_t slab_class )
{
    return 0;
}

/* --- timers --- */

int64_t global_timer = 0;
int64_t local_timers[NUM_SLAB_CLASS+1] = {0};

/* record access time */
uint32_t *accesses;

void reset_all_timers()
{
    global_timer = 0;
    memset( local_timers, 0, sizeof(int64_t) * (NUM_SLAB_CLASS+1) );
}

void reset_global_timer()
{
    global_timer = 0;
}

void reset_local_timers( int32_t slab_class )
{
    local_timers[slab_class] = 0;
}

/* --- mapping relations from key to slab class or lease  --- */
int32_t * key_slab_map = NULL;
int32_t * key_lease_map = NULL;

int32_t search_lease_time_for_key ( int32_t key_id )
{
    return key_lease_map[key_id];
}

int32_t search_slab_class_for_key ( int32_t key_id )
{
    return key_slab_map[key_id];
}

/* store how many items each slab class has */

#endif /* _STATIC_H_ */
