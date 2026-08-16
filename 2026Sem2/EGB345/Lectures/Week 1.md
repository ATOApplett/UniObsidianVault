## Brief Overview

1. **Control engineering = measure → compare → correct using feedback.**
2. EGB345 focuses on the process: **physical → mathematical model → analysis/design → physical solution**.
3. You must learn to solve **new problems**, not memorise worked examples.
4. The three main control goals are:
    - **Stability**
    - **Low steady-state error**
    - **Good transient response**
5. Active learning and self-testing are much more effective than simply rereading notes.
6. MATLAB, maths, and problem-solving practice are essential for success in this unit.
7. **Always connect mathematical results back to the real physical system.**

---
## 1. What is Control Engineering?

- **Control engineering** uses control theory to design systems with desired behaviours.
- Sensors measure the system's output.
- The measured output is fed back to controllers/actuators to correct the system's behaviour.
- Control is fundamental to **robotics, automation, aircraft, vehicles, and many other engineering systems**.

### Basic control loop

**Reference/desired input → System → Output**  
with **feedback** from the output used to correct the input.

Key components:

1. System
2. Inputs and outputs/signals
3. Feedback path

---

## 2. Main Purpose of EGB345

The unit teaches you to:

- Develop mathematical models of:
    - Mechanical systems
    - Electrical systems
    - Electromechanical systems
- Analyse systems for:
    - **Stability**
    - **Error**
    - **Time response**
- Design practical:
    - Controllers
    - Compensators
    - Feedback control systems

### Core engineering process

> **Physical world → Mathematical/abstract model → Analyse/design solution → Physical implementation**

This **physical → abstract → physical** process is one of the most important skills in the unit.

---

## 3. Prerequisite Knowledge

### Mathematics

You should be comfortable with:

- Laplace transforms
- Linear ODEs
- Signals in the:
    - Time domain
    - sss-domain
- Stability
- Time-response analysis
- Circuit analysis
- Trigonometry
- Solving simultaneous equations

### MATLAB

You should know:

- Signals as MATLAB quantities
- Creating MATLAB functions
- `for` loops
- Vectors
- Plotting
- Data manipulation

---

## 4. Unit Difficulty and Expectations

- EGB345 is an **intermediate-level control unit**.
- It is the beginning of the control branch and leads into:
    - **EGH445 Modern Control**
    - **EGH446 Autonomous Systems**
- Control is one of the more **conceptually abstract** areas of engineering.
- Strong maths and programming skills are important.
- **Do not skip MATLAB practice**—it will make later control units much harder.

---

## 5. Unit Topics

### Weeks 1–6

- Introduction to control systems
- Mathematical modelling
- Electrical systems
- Time response
- Mechanical systems
- Motors
- Reduction of subsystems
- Stability, transient response, and error

### Weeks 7–11

- Root locus
- Control system design using root locus
- Further controller design

### Week 12

- State-space models

---

## 6. Assessment

|Assessment|Weight|
|---|---|
|Tutorial Portfolio 1|10%|
|Servo Motor System Identification Report|10%|
|Tutorial Portfolio 2|10%|
|Servo Motor Control Design Report|20%|
|Final Examination|50%|

### Important assessment principle

The unit tests whether you can **solve new control problems**, not simply memorise examples.

---

## 7. High-Competency Assessment

For tutorial tasks:

- Work is generally marked **correct or incorrect**.
- **No partial credit** is awarded for an incomplete solution.

A correct solution must:

1. Have the correct final answer.
2. Correctly extract the problem from the description.
3. Choose an appropriate mathematical method.
4. Perform the mathematics correctly.
5. Interpret the mathematical result back into the physical situation.

### Key lesson

A correct answer alone is not enough—you need to show the complete reasoning process.

---

## 8. How to Study Effectively

### Avoid rote learning

Simply reading or copying solutions is not enough.

Better methods:

- Create your own summaries.
- Explain concepts in your own words.
- Compare and contrast concepts.
- Solve problems without looking at solutions.
- Test yourself regularly.

### Best general study approach

**Study → Test yourself → Test yourself again → Assessment**

Active recall and self-testing are more effective than repeatedly rereading notes.

### Important

Making mistakes can be useful for learning. Attempting difficult problems and identifying errors helps develop stronger understanding.

---

# 9. Critical Thinking

Critical thinking involves:

- Thinking in a structured way.
- Examining arguments and evidence.
- Evaluating the strength of claims.
- Questioning your own assumptions.
- Being willing to change your opinion when evidence shows you are wrong.

### Strong critical thinking

- Question your own views.
- Understand opposing viewpoints fairly.
- Identify weaknesses in your own reasoning.
- Change your thinking when evidence requires it.

### A useful technique: Compare and Contrast

When studying two concepts:

- **Compare:** What do they have in common?
- **Contrast:** How are they different?

Other useful thinking approaches:

- Sorting and classifying
- Analysing and interpreting
- Decomposing information
- Recomposing information in a more useful form

---

# 10. Bloom's Taxonomy and EGB345

EGB345 focuses on higher-level skills:

**Remember → Understand → Apply → Analyse → Evaluate → Create**

The main focus is on:

- **Analysis**
- **Evaluation**
- **Creation/design**

### This means:

You cannot pass by memorising formulas or tutorial solutions.

You must be able to:

> **Take a new physical problem → model it mathematically → analyse it → design a solution → interpret the result physically.**

---

# 11. Why Study Control?

Control engineering is important because it is used in:

- Robotics
- Automation
- Aircraft
- Vehicles
- Autonomous systems
- Space technology
- Industrial systems

The field is becoming increasingly important as automation and robotics expand.

Control also develops the general engineering ability to move between:

> **Real-world problems ↔ Mathematical models ↔ Real-world solutions**

---

# 12. Brief History of Control

### Ancient / natural control

Examples:

- Biological regulation
- Heartbeat control
- Predator-prey behaviour

### Before 1900

- Control became important during industrialisation.
- Example: **Watt's steam engine speed governor**.

### 1900–1960: Classical Control

- Development of mathematical tools for control system design.
- Applications included:
    - Ship steering
    - Aircraft
    - Missiles
    - Autopilots

### After 1960: Modern Control

- Developed alongside:
    - Electronics
    - The space race
    - Advanced aerospace systems

---

# 13. Important Control Terminology

### Open-loop control

A system operates without using feedback.

**Input → System → Output**

### Closed-loop control

The system uses feedback to compare the output with the desired response.

**Desired input → System → Output → Feedback**

Important terms:

- **Reference input:** Desired response
- **Output:** Actual system response
- **Error:** Difference between desired and actual response
- **Feedback:** Information returned from the output
- **Transient response:** Short-term behaviour after a change
- **Steady-state response:** Long-term behaviour
- **Steady-state error:** Difference remaining after the system settles

---

# 14. The Three Main Goals of a Control Engineer

### 1. Stability

> **Don't blow it up.**

The system must remain stable.

### 2. Low steady-state error

> **Reach the desired response.**

The final output should be as close as possible to the desired value.

### 3. Good transient response

> **Reach the desired response quickly and appropriately.**

The system should respond quickly without excessive oscillation or overshoot.

---

# 15. Partial Fraction Expansion

A useful mathematical technique for control analysis is rewriting a complicated fraction as a sum of simpler fractions.

Example:

s+3(s+1)(s+2)=As+1+Bs+2\frac{s+3}{(s+1)(s+2)} = \frac{A}{s+1}+\frac{B}{s+2}(s+1)(s+2)s+3​=s+1A​+s+2B​

Multiply through by the denominator:

s+3=A(s+2)+B(s+1)s+3=A(s+2)+B(s+1)s+3=A(s+2)+B(s+1)

Expand:

s+3=(A+B)s+(2A+B)s+3=(A+B)s+(2A+B)s+3=(A+B)s+(2A+B)

Compare coefficients:

A+B=1A+B=1A+B=1 2A+B=32A+B=32A+B=3

Solving:

A=2,B=−1A=2,\qquad B=-1A=2,B=−1

Therefore:

s+3(s+1)(s+2)=2s+1−1s+2\boxed{ \frac{s+3}{(s+1)(s+2)} = \frac{2}{s+1}-\frac{1}{s+2} }(s+1)(s+2)s+3​=s+12​−s+21​​

Two methods:

- **Algebraic method**
- **Residue / cover-up method**
---
