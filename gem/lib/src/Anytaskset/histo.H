#ifndef _HISTO_HPP_
#define _HISTO_HPP_

#include<stdint.h>
#include<string.h>
#include<assert.h>
#include<iostream>
#include<iomanip>

using namespace std;
namespace histo 
{
    typedef uint32_t (*DOMAIN_VALUE_TO_INDEX_FN) (uint64_t);
    typedef uint64_t (*DOMAIN_INDEX_TO_VALUE_FN) (uint32_t);

    template<int32_t BUCKETS, DOMAIN_VALUE_TO_INDEX_FN domain_value_to_index_fn, DOMAIN_INDEX_TO_VALUE_FN domain_index_to_value_fn>
    struct histogram {
	uint64_t                 buckets[BUCKETS];
	double                   cdf[BUCKETS];
	uint64_t                 totcnt;

	// constructor
	histogram (void) {
	    totcnt = 0;
	    memset ((void *)buckets, 0, sizeof (buckets));
	    memset ((void *)cdf, 0, sizeof (cdf));
	}

	inline uint32_t domain_value_to_index (uint64_t value) {
	    return domain_value_to_index_fn (value);
	}

	inline uint64_t domain_index_to_value (uint32_t index) {
	    return domain_index_to_value_fn (index);
	}
	
	inline void put_value (uint64_t value) {
	    uint32_t index = domain_value_to_index_fn (value);
	    buckets[index]++;
	}

	uint64_t calc_totcnt (void) {
	    for (int32_t index = 0; index < BUCKETS; index++)
		totcnt += buckets[index];
	    return totcnt;
	}

	inline uint64_t& operator[](uint32_t index) {
	    return buckets[index];
	}

	inline double query_pdf (uint32_t index) {
	    return (double)buckets[index]/(double)totcnt;
	}
	
	inline double query_cdf (uint32_t index) {
	    return cdf[index];
	}

	void build_cdf (bool reverse = false) {
	    uint64_t   count = 0;
	    int32_t    index;
	    
	    calc_totcnt ();

	    if (reverse) {
		for (index = BUCKETS - 1; index >= 0; index--) {
		    count += buckets[index];
		    cdf[index] = (double)count/(double)totcnt;
		}
	    } else {
		for (index = 0; index < BUCKETS; index++) {
		    count += buckets[index];
		    cdf[index] = (double)count/(double)totcnt;
		}
	    }
	}

	void print (ofstream& os) {
	  calc_totcnt();
	    os << "# HISTOGRAM TOTAL COUNT " << totcnt << endl;
	    os << "# NUMBER OF BINS: " << BUCKETS << endl;
	    for (unsigned i = 0; i <= domain_value_to_index(totcnt); i++) {
		os << "BIN: " << setw(5) << i << "   ";
		os << "VAL: " << setw(12) << domain_index_to_value (i) << "   "; 
		os << "COUNT: " << buckets[i];
		os << endl;
	    }
	}
    };

    template<uint32_t BUCKETS, uint32_t SUBLOG_BITS>
    inline uint32_t sublog_value_to_index (uint64_t value) {
	if (value < (1<<SUBLOG_BITS))
	    return value;

	unsigned index;
	int msb, shift;

#if defined (__i386__) && defined (__GNUC__)
	UINT32 hi = value >> 32;
	if (hi) {
	    __asm__ (
		"bsr %1, %0;"
		: "=r"(msb)
		: "m"(hi)
		: "eax"
	    );
	    msb += 32;
	} else {
	    UINT32 t = value;
	    __asm__ (
		"bsr %1, %0;"
		: "=r"(msb)
		: "m"(t)
		: "eax"
	   );
	}
#elif defined (__x86_64__) &&  defined (__GNUC__)
	__asm__ (
	    "rex.w bsr %1, %0;"
	    : "=r"(msb)
	    : "m"(value)
	    : "eax"
	);
#else
	cerr << "Wrong architecture or compiler!" << endl;
	assert(0);
#endif 
	shift = msb - SUBLOG_BITS;
	index = value >> shift;
	index &= (1<<SUBLOG_BITS) - 1;
	index = ((shift + 1) << SUBLOG_BITS) + index;
	return index;
    }

    template<uint32_t BUCKETS, uint32_t SUBLOG_BITS>
    inline uint64_t sublog_index_to_value (uint32_t index) {
	uint32_t shift = index >> SUBLOG_BITS;
	uint32_t t     = index & ((1<<SUBLOG_BITS) - 1);
	
	if (!shift)
	    return index;
	else 
	    return (uint64_t)((1<<SUBLOG_BITS) + t) << (shift - 1);
    }

    // An example use of the histogram template:
    // 
    // const  uint32_t              SUBLOG_BITS = 8;
    // const  uint32_t              HIST_BUCKETS = (65-SUBLOG_BITS)*(1<<SUBLOG_BITS);
    // histogram<HIST_BUCKETS, sublog_value_to_index<HIST_BUCKETS, SUBLOG_BITS>, sublog_index_to_value<HIST_BUCKETS, SUBLOG_BITS> >  
    //                            reuse_time_hist;

}
#endif
