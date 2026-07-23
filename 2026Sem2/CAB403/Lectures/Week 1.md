**Course overview**

- CAB403 covers two intertwined topics: **operating systems concepts** and **systems programming in C**. Lectures lean towards OS-theory, practicals lean towards C-programming.
- Learning outcomes: understand OS architecture, apply best practice for safety-critical systems, recognize societal/safety impacts of programming failures, and write low-level systems programs.
- **Inverted classroom model**: pre-recorded videos cover material all semester; live lectures are for Q&A, deeper dives, and demos (recorded via Echo360/Zoom).

**Logistics**

- **Discord** is the main support channel (email `cab403@qut.edu.au` only if you need direct staff attention).
- **Assessment**: Problem Solving Task (10%, individual, due Week 5), Applied Project (50%, individual, due Week 13), Final Exam (40%). Late work without an approved extension gets 0%.
- Extensions, academic concessions, disability support, and deferred exams are available through standard QUT channels.
- Two public holidays affect scheduling: Brisbane Royal Show (Week 4, no lecture) and King's Birthday (Week 11).
- **Generative AI policy**: treated like consulting another person — fine for learning concepts, but you must not submit AI-generated code directly.
- Recommended (not required) textbook: Silberschatz, Galvin & Gagne, _Operating System Concepts_ (9th or 10th ed.), plus free O'Reilly resources via QUT library.

**Introduction to Operating Systems**

- An OS is the intermediary between users and hardware — a **resource allocator** and **control program**.
- No single universal definition; commonly "the one program always running" = the **kernel**; everything else is a system or application program.
- Covered: bootstrap/firmware startup, concurrent I/O via device controllers and interrupts, interrupt handling and DMA (direct memory access) for high-speed I/O, and **dual-mode operation** (user mode vs. kernel mode) protecting the system via privileged instructions and timers.

**Introduction to C Programming**

- C, created by Dennis Ritchie (1969–73), is deeply tied to UNIX and hugely influential on later languages.
- Basics demoed: Hello World, variables, loops (`for`/`while`/`do-while`), text I/O (`printf`/`scanf`).
- **Key theme: C is not a safe language.** No bounds checking, prone to undefined behaviour, and errors (e.g., out-of-range array access) can silently produce unpredictable results — sometimes only surfacing after deployment. This demands discipline and careful debugging (tools like `gdb`, `valgrind`).