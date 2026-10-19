#include "krnl_proj.h"
#include "scanner.h"
#include <ap_int.h>
#include <hls_stream.h>


struct ParallelMatchBatch {
    ap_uint<64> base_byte_position; 
    ap_uint<16> match_ids[PARALLEL_BYTES]; 
    bool end_of_packet;
};


void core_processor(
    hls::stream<pkt> &n2k,
    hls::stream<ParallelMatchBatch> &internal_stream,
    unsigned int num_packets
) {
    uint64_t global_byte_counter = 0;

    for (unsigned int p = 0; p < num_packets; p++) {
        bool packet_active = true;
        bool is_first_beat = true;

        while (packet_active) {
            #pragma HLS LOOP_TRIPCOUNT min=1 max=32
            
            pkt v_in = n2k.read();
            bool is_last_beat = (v_in.last == 1);
            

            for (int i = 0; i < DATA_WIDTH_BYTES; i += PARALLEL_BYTES) {
                #pragma HLS PIPELINE II=1
                

                unsigned char chunk[PARALLEL_BYTES];
                #pragma HLS ARRAY_PARTITION variable=chunk complete
                
                for (int b = 0; b < PARALLEL_BYTES; b++) {
                    #pragma HLS UNROLL
                    chunk[b] = v_in.data.range((i + b) * 8 + 7, (i + b) * 8);
                }


                bool reset_scanner = (is_first_beat && i == 0);
                ap_uint<16> results[PARALLEL_BYTES];
                #pragma HLS ARRAY_PARTITION variable=results complete

                update_window_and_check_parallel(chunk, reset_scanner, results);


                ParallelMatchBatch batch;
                batch.base_byte_position = global_byte_counter;
                batch.end_of_packet = false;
                
                for (int b = 0; b < PARALLEL_BYTES; b++) {
                    #pragma HLS UNROLL
                    batch.match_ids[b] = results[b];
                }
                
                bool any_match = false;
                for(int b=0; b<PARALLEL_BYTES; b++) {
                    #pragma HLS UNROLL
                    if(results[b] != 0) any_match = true;
                }
                
                if (any_match) {
                    internal_stream.write(batch);
                }

                global_byte_counter += PARALLEL_BYTES;
            }

            is_first_beat = false;
            if (is_last_beat) {
                packet_active = false;
                ParallelMatchBatch terminator;
                terminator.end_of_packet = true;
                internal_stream.write(terminator);
            }
        }
    }
}


void output_formatter(
    hls::stream<ParallelMatchBatch> &internal_stream,
    hls::stream<pkt> &k2n,
    unsigned int dest,
    unsigned int num_packets
) {

    
    for (unsigned int p = 0; p < num_packets; p++) {
        bool packet_done = false;
        
        while (!packet_done) {
            #pragma HLS PIPELINE II=1
            
            ParallelMatchBatch batch = internal_stream.read();

            if (batch.end_of_packet) {
                packet_done = true;

                pkt end_pkt;
                end_pkt.data = 0xEE;
                end_pkt.keep = 0xFF;
                end_pkt.last = 1;
                end_pkt.dest = dest;
                k2n.write(end_pkt);
            } else {

                bool any_match = false;
                ap_uint<512> out_data = 0;
                ap_uint<64> out_keep = 0;

                if (batch.match_ids[0] != 0) {
                    out_data.range(63, 0)   = batch.base_byte_position;
                    out_data.range(79, 64)  = batch.match_ids[0];
                    out_keep.range(15, 0)   = 0xFFFF; 
                    any_match = true;
                }
                

                if (batch.match_ids[1] != 0) {
                    out_data.range(191, 128) = batch.base_byte_position + 1;
                    out_data.range(207, 192) = batch.match_ids[1];
                    out_keep.range(31, 16)   = 0xFFFF;
                    any_match = true;
                }

                if (batch.match_ids[2] != 0) {
                    out_data.range(319, 256) = batch.base_byte_position + 2;
                    out_data.range(335, 320) = batch.match_ids[2];
                    out_keep.range(47, 32)   = 0xFFFF;
                    any_match = true;
                }


                if (batch.match_ids[3] != 0) {
                    out_data.range(447, 384) = batch.base_byte_position + 3;
                    out_data.range(463, 448) = batch.match_ids[3];
                    out_keep.range(63, 48)   = 0xFFFF;
                    any_match = true;
                }


                if (any_match) {
                    pkt out;
                    out.data = out_data;
                    out.keep = out_keep; 
                    out.dest = dest;
                    out.last = 0;
                    k2n.write(out);
                }
            }
        }
    }
}

void krnl_proj(hls::stream<pkt> &n2k, hls::stream<pkt> &k2n, unsigned int dest, unsigned int num_packets) {
    #pragma HLS INTERFACE axis port=n2k
    #pragma HLS INTERFACE axis port=k2n
    #pragma HLS INTERFACE s_axilite port=dest bundle=control
    #pragma HLS INTERFACE s_axilite port=num_packets bundle=control
    #pragma HLS INTERFACE s_axilite port=return bundle=control

    #pragma HLS DATAFLOW

    hls::stream<ParallelMatchBatch> match_fifo;
    #pragma HLS STREAM variable=match_fifo depth=256
    
    core_processor(n2k, match_fifo, num_packets);
    output_formatter(match_fifo, k2n, dest, num_packets);
}