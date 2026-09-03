https://canvas.qut.edu.au/courses/24157/pages/egb345-assessment-2-portfolio-problem-solving-task-details?module_item_id=2284032

https://docs.google.com/document/d/1hXerGxpGz3B9lWxOfxsEzH6p7yDSQ8oeOCJdsixaErw/edit?usp=sharing

- [ ]  Portfolio 1 (10%) — due Week 7
	- [x] Week 3
		- [ ] double check first transfer function
		- [x] double check working is the same as workshop
	- [x] Week 4
		- [ ] double check working is the same as workshop
		- [ ] redo working to change J1 and J2 into M1 and M2
	- [x] Week 5
		- [ ] Video explanation
		- [ ] double check working is the same as workshop
	- [x] Week 6 
		- [ ] Find a more suitable working for task 3

Layout:
- Make google doc
- Put screenshot of question
- Upload image of solution
- reference ai for guidance ?

Video Script:

**[0:00–0:20] Hook & Setup**

This is a Series Elastic Actuator — it drives a load through a spring so the motor's inertia is decoupled from the load. Our goal: find the transfer function relating load angle to input voltage.

**[0:20–0:50] Two Degrees of Freedom & Reflection**

We have two degrees of freedom: the motor shaft angle and the load angle. Since they're separated by a gearbox, we reflect the motor's inertia and torque across to the load side using the gear ratio.

**[0:50–1:30] Two Torque-Balance Equations**

We write torque balance at the motor shaft — that's equation one — and at the load shaft — equation two. The coupling spring links them together. Notice the load side has both a spring and a damper connecting to the wall — that's what adds an extra term into this equation compared to a simpler version of this problem.

**[1:30–2:00] Eliminate the Load Angle**

We isolate the load angle from equation two, substitute it into equation one, and simplify. This eliminates the load angle and leaves torque purely in terms of the motor shaft angle.

**[2:00–2:30] The Key Correction — KVL, Not KCL**

Now for the electrical side — this step is voltage summation around the loop, so it's Kirchhoff's Voltage Law, KVL — not KCL. We sum the source voltage against the resistor drop and the back-EMF. This is a common mix-up — even though some course notes label this step "KCL" by convention, it's really a voltage loop equation, so KVL is the correct name for it.

**[2:30–2:55] Final Transfer Function**

Substituting back and clearing fractions gives our final result — a fourth-order transfer function relating load angle to input voltage.

**[2:55–3:00] Wrap-Up**

Two torque-balance equations, one gearbox reflection, and a KVL loop equation — that's the full path from voltage input to load displacement.