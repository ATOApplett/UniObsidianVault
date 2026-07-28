#### Admin

- **Unit Coordinator:** Timothy Chappell ([cab401@qut.edu.au](mailto:cab401@qut.edu.au), S Block Level 10, Room 1010)
- **Assessment:**
    - Oral Presentation – 20%, Weeks 6–12, individual — research & present a parallel programming language/tool
    - Parallelization Assignment – 50%, due end of Week 13, individual — parallelize a chosen application
    - Final Exam – 30%, during exam period
- **Assumed knowledge:** strong C++, Java, or C# (course uses all three)
- **Public holidays:** Royal Brisbane Show (Wed Aug 12, Wk 4 – no live prac that week); King's Birthday (Mon Oct 5, Wk 11 – no live lecture)
- **Homework:** investigate your own PC/laptop's CPU (cores, hyperthreading, cache levels, clock speed, memory channels/latency) — report back next week

#### The Free Lunch Is Over (Herb Sutter, 2005)

- Key idea: CPU manufacturers hit physical limits on boosting single-core performance via clock speed/instruction throughput
- Industry shifted to **hyperthreading and multicore** architectures instead
- This marks a **fundamental turning point**: software must now be written to exploit concurrency to get speed gains ("Andy giveth, and Bill taketh away")

#### Von Neumann Architecture (foundations)

- Components: **CPU** (Control Unit, Program Counter, Instruction Register, ALU), **Memory** (data + instructions), **Bus**, Input/Output devices
- **Fetch–Decode–Execute cycle:**
    - **Fetch:** get next instruction from memory (PC increments)
    - **Decode:** determine operation type — Load (mem→reg), Store (reg→mem), Arithmetic (reg+reg→reg), Branch (address→PC)
    - **Execute:** ALU does arithmetic; bus/memory handles load/store

#### CPU Clocking

- Clock period must allow: signal propagation across the circuit (~30cm/ns) + transistors to settle
- Over-clocking risks: incorrect computation results, overheating (heat ∝ clock speed)

#### The Von Neumann Bottleneck

- Memory sits off-chip, clocked much slower than CPU → load/store can stall the CPU
- Two key memory metrics: **throughput** (data/sec) and **latency** (request→response time)

#### Caching

- Stores frequently used data closer to CPU for better speed
- Loads whole **cache lines** (e.g. 64 bytes) exploiting locality of reference
- Replacement policy needed when cache full (e.g. **LRU**)
- Mapping schemes: **fully associative**, **direct mapped**, **N-way set associative**
- Multiple cache levels (L1 smallest/fastest → L2 → L3)

#### Superscalar Processors

- Execute multiple instructions simultaneously (**Instruction-Level Parallelism, ILP**)
- Multiple functional units (ALUs, FPUs, SSE)
- Pipelined execution; hardware detects instruction dependencies → enables **out-of-order execution**
- Needs branch prediction/speculative execution to keep pipeline full
- Tradeoff: more performance vs. more chip area/power/complexity; relates to CISC vs RISC debate

#### Moore's Law

- Gordon Moore (1965): transistor count on a chip doubles ~every 2 years
- Historically enabled higher clock speeds and more complex hardware optimizations (out-of-order execution, branch prediction)
- Also boosted camera megapixels, memory capacity — cost stayed flat/dropped
- Raises the question of a future "technological singularity"

#### The End of Moore's Law?

- Transistor density still rising, but **clock speeds have plateaued**
- Miniaturization limits: speed-of-light signal limits, atomic-scale wire limits, **quantum tunnelling** issues
- Heat generation limits: heat ∝ clock speed (fastest CPUs need liquid nitrogen cooling)
- Possible future directions: 3D chips, new materials, quantum computing

#### Using Extra Silicon (given clock speed can't rise)

- Options: more cache, more complex execution hardware (limited benefit due to ILP limits), **or add more cores per chip**

#### Multicore Chips

- Multiple CPUs (cores) per chip
- Cores can share the **same instruction stream** on different data (GPU-style / SIMD) or, more commonly, run **independent instruction streams**
- Inter-core communication: message passing (network-on-chip) or (more commonly) **shared memory**

#### Cache Coherence (multicore issue)

- When one core writes to its private cache, matching lines in other private caches must be **invalidated**
- Achieved via **snooping** or **directory-based** protocols
- Cache types: **inclusive** (L1 data also in L2/L3) vs **exclusive** (data in only one level)

#### Software Abstractions: Process & Thread

- A **process** contains one or more **threads**; threads share the process's heap (shared address space) but have their own stack and PC
- OS schedules threads onto CPUs; performs context switches, manages virtual memory
- Threads map naturally onto multicore/shared-memory hardware — each thread can run on its own core simultaneously; more threads than cores → time-slicing

#### Simultaneous Multithreading (SMT)

- A single core runs multiple threads at once without context switching
- Intel: **HyperThreading**; AMD: **SMT**
- Each thread gets its own register set/PC; functional units are shared
- Example: Intel Core i7 quad-core + HyperThreading = 4 cores × 2 threads = "8 virtual cores"