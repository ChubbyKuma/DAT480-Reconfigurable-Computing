#ifndef SCANNER_H
#define SCANNER_H

#include "patterns.h"
#include "ap_int.h"


#define PARALLEL_BYTES 4

#define WINDOW_SIZE (PATTERN_MAX_LEN + PARALLEL_BYTES)

/**
 * @brief 
 * @param input_bytes  
 * @param reset        
 * @param match_ids    
 */
void update_window_and_check_parallel(
    unsigned char input_bytes[PARALLEL_BYTES],
    bool reset,
    ap_uint<16> match_ids[PARALLEL_BYTES]
);

#endif