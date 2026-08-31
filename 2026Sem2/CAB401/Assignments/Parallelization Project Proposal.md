### TEMPLATE
1. A brief description of the sequential application that you have selected to parallelize. What does it do? Where did you find it? 


2. Discuss why you think the proposed application performs sufficient computation so that parallelizing it will potentially produce a noticeable difference in perceived execution time. 


3. What parallel hardware and parallelization language/framework are you considering? E.g. targeting NVidia GPU programmed using CUDA. How many cores does it have? (Note, you need more that 4 virtual cores to achieve an excellent for Optimal Speedup assessment criteria).

### MandelBrot 
1. Description of the sequential application

- The application generates a **Mandelbrot set image**: for every pixel in a 2D grid, it maps the pixel to a complex number _c_, then iterates the function z = z² + c starting from z = 0, counting how many iterations it takes before |z| exceeds a bailout radius (or hitting a max iteration cap).
- The iteration count for each pixel determines its colour, producing the characteristic fractal image.
- Sequentially, this is done with a **nested loop over every pixel (rows × columns)**, with an inner loop running the escape-time iteration for that pixel — a fully sequential, single-threaded C/C++ program.
- Found as a standard reference sequential implementation on GitHub (e.g. searching "mandelbrot set sequential C"), commonly used as a teaching example for parallel/HPC courses — [replace with the actual repo/source you used, e.g. author, link, license].

2. . Sufficient computation to justify parallelization

- Total work scales with **image resolution × max iterations per pixel** — e.g. a 4000×4000 image with a max iteration cap of 1000–2000 requires potentially billions of floating-point iterations.
- Each pixel's iteration count varies (points inside the set run to the full iteration cap; points outside escape early), giving **load imbalance** — a realistic and interesting parallelization challenge, not just a trivial split.
- At high resolution and iteration depth, sequential runtime is measured in **tens of seconds to minutes** on a standard CPU — a clearly perceptible delay, not a sub-second toy problem.
- Deep zoom regions (e.g. zooming into the boundary of the set) require even higher iteration counts for visual accuracy, further increasing computational load and making the benefit of parallelization more pronounced.
- I benchmarked the sequential version at [insert your actual measured runtime, resolution, and max-iteration parameters here] to confirm it's substantial enough to show a measurable speedup.

3. Parallel hardware and framework

- **Target hardware**: NVIDIA GPU (e.g. [insert your specific GPU model, such as an RTX 3060/4070, or QUT lab GPU]), which has **[insert actual CUDA core count, e.g. 3584 CUDA cores]** — well above the 4-virtual-core threshold needed for an "excellent" Optimal Speedup rating.
- **Framework**: CUDA (C/C++ with the CUDA toolkit), since the problem is **embarrassingly parallel** — each pixel's escape-time calculation is fully independent of every other pixel, making it an ideal fit for a GPU's SIMT (single-instruction, multiple-thread) execution model.
- Plan: launch one CUDA thread per pixel (or per small block of pixels), with each thread independently running the escape-time iteration and writing its result to a global output buffer/image.
- **Fallback/comparison option**: if GPU access is limited, a multi-core CPU version using OpenMP (row- or pixel-based work distribution across threads) could be used as a secondary comparison, on a machine with [insert actual physical/virtual core count] cores.
- Access: via [QUT lab machines / personal GPU / cloud GPU instance — specify which you're actually using], confirmed to have more than 4 (virtual) cores available for the assessment's Optimal Speedup criterion.

