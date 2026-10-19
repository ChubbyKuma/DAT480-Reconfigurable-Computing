# DAT480 Reconfigurable Computing

Course project for **DAT480: Reconfigurable Computing** at Chalmers University of Technology.

This project implements a hardware-accelerated pattern matching engine (Deep Packet Inspection / NIDS) on an **AMD Alveo U55C FPGA** (`xcu55c-fsvh2892-2L-e`) using **Vitis HLS** and the **Vitis Network Example (VNX)** UDP/IP streaming framework.

- **Author**: Guanhua Huang ([@ChubbyKuma](https://github.com/ChubbyKuma))
- **Platform**: AMD Xilinx Alveo U55C (Virtex UltraScale+ HBM)
- **Target Frequency**: 300 MHz
- **Hardware Throughput**: 3.23 Gbps (~1.33 bytes/cycle)

---

## Project Overview

The objective of this project is to inspect streaming UDP packet payloads in hardware and detect signature patterns derived from Snort rules at high line rates.

The baseline course assignment specifies scanning **1 byte per cycle**. To improve processing performance, this design implements a **4-byte parallel sliding window** architecture that scans 4 consecutive byte positions every clock cycle with an Initiation Interval of 1 ($II = 1$).

### Key Features

1. **4-Byte Parallel Sliding Window**:
   Instead of a simple 1-byte shift register, the scanner maintains an internal sliding window that shifts by 4 bytes each cycle. Four parallel lanes compare the current window against rule signatures concurrently.

2. **Complete Array Partitioning**:
   The sliding window registers and lookup structures use `#pragma HLS ARRAY_PARTITION complete` to eliminate memory port contention, allowing all 4 comparison lanes to read data in the same clock cycle.

3. **Loop Dependency Breaking (Parallel Fixed-Mapping)**:
   In earlier iterations, packing matching results back into the 512-bit output stream using a loop created a loop-carried dependency, resulting in negative timing slack. This was resolved by implementing a parallel fixed-slot mapping: match results from lanes 0 through 3 are directly mapped to designated 128-bit slots in the 512-bit output packet, removing the dependency chain and achieving timing closure.

---

## Architecture

The top kernel `krnl_proj` connects to the 512-bit AXI4-Stream network interface (`pkt`) and uses `#pragma HLS DATAFLOW` between two main pipeline stages:

```
                  512-bit AXI-Stream (n2k)
                             |
                             v
               +---------------------------+
               |      core_processor       |
               |                           |
               |  - Extract 4-byte chunks  |
               |  - Parallel scanner (x4)  |
               |  - II = 1 inner pipeline  |
               +---------------------------+
                             |
                             | hls::stream<ParallelMatchBatch>
                             v
               +---------------------------+
               |     output_formatter      |
               |                           |
               |  - Fixed-slot mapping     |
               |  - Output AXI-Stream pkt  |
               +---------------------------+
                             |
                             v
                  512-bit AXI-Stream (k2n)
```

- **`core_processor`**: Consumes incoming 512-bit beats from the network. Each 64-byte beat is processed as 16 consecutive 4-byte steps. Each step passes 4 bytes into `update_window_and_check_parallel()`.
- **`scanner`**: Updates the sliding window, decodes characters against the rule set, and writes matches to an internal stream.
- **`output_formatter`**: Reads matches and maps lane results directly into the outgoing 512-bit AXIS packet without dynamic loop packing.

---

## Synthesis Results

Synthesized with **Vitis HLS 2023.2** for target device `xcu55c-fsvh2892-2L-e`:

| Metric | Result | Target / Budget |
| :--- | :--- | :--- |
| **Clock Period** | 3.33 ns (300 MHz) | 3.33 ns |
| **Worst Negative Slack (WNS)** | **+0.07 ns** | >= 0 ns (Met) |
| **Initiation Interval (II)** | **1** | 1 |
| **LUT Utilization** | 103,836 (7.96%) | 1,303,680 |
| **FF Utilization** | 81,869 (3.14%) | 2,607,360 |
| **BRAM Utilization** | 8 (0.40%) | 2,016 |
| **URAM Utilization** | 0 (0.00%) | 960 |

Full synthesis metrics and loop reports are documented in [csynth.rpt](csynth.rpt).

---

## Hardware Measurements

On-board testing was conducted on an Alveo U55C card using the PYNQ framework and VNX benchmark traffic:

- **Measured Median Throughput**: **3.23 Gbps**
- **Effective Processing Rate**: **~1.33 bytes per clock cycle**
- **Theoretical 1 B/cycle Baseline**: 2.40 Gbps at 300 MHz
- **Speedup over Baseline**: ~134%

The project report with detailed discussion and measurement plots is available in [DAT480_report.pdf](DAT480_report.pdf).

---

## Repository Structure

```
DAT480-Reconfigurable-Computing/
├── config_files/
│   ├── connectivity_basic_if0.ini
│   ├── connectivity_basic_if1.ini
│   ├── connectivity_basic_if3.ini
│   ├── connectivity_benchmark_if0.ini
│   ├── connectivity_benchmark_if1.ini
│   ├── connectivity_benchmark_if3.ini
│   ├── connectivity_benchmark_project_if0.ini
│   └── connectivity_project_if0.ini
├── Notebooks/
│   ├── dask_pynq.py          # Multi-node Dask / PYNQ utilities
│   ├── README.md             # PYNQ environment setup
│   ├── throughput.ipynb      # Line-rate hardware throughput test
│   ├── vnx_utils.py          # Network link & socket helper routines
│   └── vnx-basic.ipynb       # Basic packet test notebook
├── src/
│   ├── krnl_proj.cpp         # Top HLS kernel and dataflow stages
│   ├── krnl_proj.h           # Kernel function prototypes & AXIS packet types
│   ├── scanner.cpp           # 4-byte parallel pattern scanner implementation
│   └── scanner.h             # Scanner interface and window size definitions
├── csynth.rpt                # Vitis HLS synthesis summary
├── DAT480_report.pdf         # Course final report
└── README.md                 # Project documentation
```

---

## Build & Usage

### 1. Compile the Kernel (Vitis HLS)
Run synthesis using the Vitis HLS flow:
```bash
vitis_hls -f run_hls.tcl
```

### 2. Link with VNX (Vitis v++)
Generate the final `.xclbin` bitstream targeting Alveo U55C:
```bash
v++ -l -t hw \
    --platform xilinx_u55c_gen3x16_xdma_3_202210_1 \
    --config config_files/connectivity_project_if0.ini \
    krnl_proj.xo -o krnl_proj.xclbin
```

### 3. Run Verification & Benchmarks (Host)
Launch JupyterLab in the `Notebooks/` directory:
```bash
cd Notebooks
jupyter lab
```
Run `throughput.ipynb` to download the overlay to the Alveo card and measure UDP streaming throughput.
