#include "scanner.h"


static unsigned char decode_byte(unsigned char raw) {
    #pragma HLS INLINE
    unsigned char idx = 0xFF;
    for (int i = 0; i < NUM_USED_BYTES; i++) {
        #pragma HLS UNROLL
        if (raw == used_bytes[i]) {
            idx = i;
        }
    }
    return idx;
}

void update_window_and_check_parallel(
    unsigned char input_bytes[PARALLEL_BYTES],
    bool reset,
    ap_uint<16> match_ids[PARALLEL_BYTES]
) {
    #pragma HLS PIPELINE II=1
    #pragma HLS INLINE off

    static unsigned char window[WINDOW_SIZE];
    #pragma HLS ARRAY_PARTITION variable=window complete

    #pragma HLS DEPENDENCE variable=window inter false

    unsigned char decoded[PARALLEL_BYTES];
    #pragma HLS ARRAY_PARTITION variable=decoded complete
    
    for (int i = 0; i < PARALLEL_BYTES; i++) {
        #pragma HLS UNROLL
        decoded[i] = decode_byte(input_bytes[i]);
    }


    if (reset) {
        for (int i = 0; i < WINDOW_SIZE; i++) {
            #pragma HLS UNROLL
            window[i] = 0xFF;
        }
    } else {

        for (int i = 0; i < WINDOW_SIZE - PARALLEL_BYTES; i++) {
            #pragma HLS UNROLL
            window[i] = window[i + PARALLEL_BYTES];
        }

        for (int i = 0; i < PARALLEL_BYTES; i++) {
            #pragma HLS UNROLL
            window[WINDOW_SIZE - PARALLEL_BYTES + i] = decoded[i];
        }
    }


    
    for (int lane = 0; lane < PARALLEL_BYTES; lane++) {
        #pragma HLS UNROLL
        
        ap_uint<16> best_id = 0;
        

        int current_end_pos = WINDOW_SIZE - PARALLEL_BYTES + lane;


        for (int p = 0; p < NUM_PATTERNS; p++) {
            #pragma HLS UNROLL factor=16 
            
            int len = rules[p].len;
            bool match = true;
            
            for (int k = 0; k < PATTERN_MAX_LEN; k++) {
                #pragma HLS UNROLL
                if (k < len) {

                    unsigned char w_char = window[current_end_pos - k];
                    unsigned char p_char = rules[p].byte_index[len - 1 - k];
                    
                    if (w_char != p_char) {
                        match = false;
                    }
                }
            }
            
            if (match && len > 0) {
                best_id = (ap_uint<16>)(p + 1);
            }
        }
        match_ids[lane] = best_id;
    }
}