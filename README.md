# DAT480 - Reconfigurable Computing Project: High-Throughput Hardware Pattern Matching Engine

[![Platform](https://img.shields.io/badge/Platform-AMD%20Xilinx%20Alveo%20U55C-orange.svg)](https://www.xilinx.com/products/boards-and-kits/alveo/u55c.html)
[![Toolchain](https://img.shields.io/badge/Vitis%20HLS-2023.2-blue.svg)](https://www.xilinx.com/products/design-tools/vitis/vitis-hls.html)
[![Framework](https://img.shields.io/badge/Host-PYNQ%20%2F%20VNX-green.svg)](https://pynq.readthedocs.io/)
[![Throughput](https://img.shields.io/badge/Measured%20Throughput-3.23%20Gbps-brightgreen.svg)](#4-hardware-evaluation--performance)
[![Institution](https://img.shields.io/badge/Institution-Chalmers%20University%20of%20Technology-navy.svg)](https://www.chalmers.se/)

## 1. Overview

This repository contains the design, implementation, and evaluation of a high-performance **Hardware Pattern Matching Engine** for Network Intrusion Detection Systems (NIDS), developed for the **DAT480 Reconfigurable Computing** course at **Chalmers University of Technology** (January 2026).

- **Authors**: Guanhua Huang, Oliver Ivarsson, Rohan Paul Jamieson
- **Target Device**: AMD Virtex UltraScale+ HBM FPGA (`xcu55c-fsvh2892-2L-e` / Alveo U55C)
- **Deployment Platform**: Vitis Network Example (VNX) 100GbE UDP/IP subsystem with PYNQ host interface

The objective is to scan streaming UDP packet payloads at line rate to detect malicious signatures (derived from Snort3 rules). While the course baseline requirement specifies processing **1 byte per clock cycle**, this design introduces an innovative **4-Byte Parallel Sliding Window architecture** that processes 4 bytes per cycle with an Initiation Interval ($II = 1$), achieving a measured hardware throughput of **3.23 Gbps** (~1.33 bytes/cycle on physical board).

---

## 2. System Architecture

The accelerator is structured as a two-stage pipelined dataflow kernel (`krnl_proj`) interconnected via streaming FIFOs:

```
+-------------------------------------------------------------------------------------------------+
|                                    krnl_proj (Vitis HLS Kernel)                                |
|                                                                                                 |
|   512-bit UDP Stream (AXIS)                                                                     |
|   ========================> [ Stage 1: Core Processor & Scanner ]                               |
|                                - 16 x 4-Byte Chunk Unrolling per 512-bit beat                   |
|                                - 4-Byte Parallel Sliding Window Register                        |
|                                - Parallel Rule Matching (II=1, complete array partition)        |
|                                             |                                                   |
|                                             | ParallelMatchBatch (hls::stream FIFO, depth=256)   |
|                                             v                                                   |
|                             [ Stage 2: Parallel Output Formatter ]                              |
|                                - Fixed-Slot Parallel Mapping (avoids loop packing dependency)   |
|                                - Positive timing slack (+0.07 ns)                               |
|                                             |                                                   |
|   512-bit Matched Stream                    v                                                   |
|   <=========================================+                                                   |
+-------------------------------------------------------------------------------------------------+
```

### Key Technical Innovations

1. **Parallel Data Extraction**:
   - The 512-bit input stream beat (`pkt`) is decomposed into 16 consecutive 4-byte chunks.
   - An inner pipeline with `#pragma HLS PIPELINE II=1` processes each 4-byte slice simultaneously across 4 parallel lanes.

2. **Parallel Sliding Window with Complete Partitioning**:
   - Implements a shift register advancing by 4 bytes per cycle (`WINDOW_SIZE = PATTERN_MAX_LEN + 4`).
   - Uses `#pragma HLS ARRAY_PARTITION variable=window complete` to eliminate memory bank conflicts and allow all comparator lanes simultaneous single-cycle access.

3. **Loop Dependency Breaking via Fixed-Slot Mapping**:
   - Initial iterative packing loops introduced loop-carried dependencies that degraded timing slack into negative values.
   - Replaced with a **Parallel Fixed-Mapping strategy** where matches in lanes [0..3] are directly addressed into designated 128-bit slots within the 512-bit egress packet, guaranteeing zero timing closure penalties.

---

## 3. Synthesis & Resource Utilization

Synthesized using **AMD Vitis HLS 2023.2** targeting the `xcu55c-fsvh2892-2L-e`:

| Metric | Result | Target / Budget | Status |
| :--- | :---: | :---: | :---: |
| **Timing Slack** | **+0.07 ns** | $\ge 0.00\text{ ns}$ | **Met (Zero Violation)** |
| **Clock Period** | 3.33 ns (300 MHz) | 3.33 ns | **Met** |
| **Pipeline Initiation Interval** | **$II = 1$** | $II = 1$ | **Optimal** |
| **Look-Up Tables (LUT)** | 103,836 (7%) | ~1.3M | Excellent Headroom |
| **Flip-Flops (FF)** | 81,869 (3%) | ~2.6M | Minimal Footprint |
| **Block RAM (BRAM)** | 8 (<1%) | ~4,000 | Negligible |
| **UltraRAM (URAM)** | 0 (0%) | 960 | None Required |

Detailed synthesis logs can be inspected in [`csynth.rpt`](csynth.rpt).

---

## 4. Hardware Evaluation & Performance

The design was tested on physical hardware using the **AMD Alveo U55C** accelerator card coupled with the VNX 100GbE networking stack and PYNQ:

- **Measured Median Throughput**: **3.23 Gbps**
- **Effective Processing Rate**: ~**1.33 bytes per clock cycle** (substantially exceeding the 1-byte/cycle baseline)
- **Validation**: Full packet verification across benchmark traffic distributions in Jupyter companion notebooks.

For in-depth analysis and experimental curves, refer to the full report: [`DAT480_report.pdf`](DAT480_report.pdf).

---

## 5. Repository Structure

```text
.
├── config_files/               # Vitis linker and connectivity configuration files
│   ├── connectivity_project_if0.ini
│   └── connectivity_benchmark_*.ini
├── Notebooks/                  # PYNQ host evaluation & benchmarking suite
│   ├── throughput.ipynb        # Hardware throughput evaluation notebook
│   ├── vnx-basic.ipynb         # Basic VNX network packet validation
│   ├── dask_pynq.py            # Distributed multi-FPGA helper
│   ├── vnx_utils.py            # VNX network helper library
│   └── README.md               # Host environment setup guide
├── src/                        # Vitis HLS C++ kernel implementation
│   ├── krnl_proj.cpp           # Top kernel & dataflow pipeline (core + formatter)
│   ├── krnl_proj.h             # Kernel interface & 512-bit packet definitions
│   ├── scanner.cpp             # 4-byte parallel sliding window pattern scanner
│   └── scanner.h               # Scanner configurations & window dimensions
├── csynth.rpt                  # Full Vitis HLS synthesis summary report
├── DAT480_report.pdf           # Project technical report submitted for DAT480
└── README.md                   # Repository documentation
```

---

## 6. Getting Started

### Prerequisites
- **FPGA Synthesis**: AMD Vitis Unified Software Platform 2023.2+
- **Host System**: Linux (Ubuntu 20.04/22.04 LTS recommended) with Xilinx XRT installed
- **Python / Host Environment**: Python 3.8+ with `pynq` and `jupyterlab`

### Building the Kernel
```bash
# Compile C++ HLS kernel to Xilinx Object (.xo)
vitis_hls -f run_hls.tcl

# Link kernel with VNX shell targeting Alveo U55C
v++ -l -t hw --platform xilinx_u55c_gen3x16_xdma_3_202210_1 \
    --config config_files/connectivity_project_if0.ini \
    krnl_proj.xo -o krnl_proj.xclbin
```

### Running the Benchmarks
1. Start JupyterLab from the `Notebooks` directory:
   ```bash
   cd Notebooks
   jupyter lab
   ```
2. Open [`Notebooks/throughput.ipynb`](Notebooks/throughput.ipynb).
3. Follow the notebook instructions to download the overlay (`.xclbin`) to the Alveo card and initiate line-rate traffic benchmarking.

---

## 7. License & Acknowledgments

This project was developed as part of **DAT480: Reconfigurable Computing** at Chalmers University of Technology.
Notebook infrastructure and network subsystems build upon [Xilinx / AMD Vitis Network Example (VNX)](https://github.com/Xilinx/xup_vitis_network_example).
