Task 1.2 - 1.4 Mathematical
- task 1.3 is underdamped

1.7 Comments to make:

- Open loop is a ramp, as predicted analytically.
    
- For K = 1, expect about 3% overshoot and Ts of about 1.74 s.
    
- For KTask1, expect about 5% overshoot and the same Ts, because the real part of the poles is always −α/2 = −2.3. Only the imaginary part changes with K.
    
- Note any small difference in stepinfo: it measures Ts to a 2% band, and the formula is an approximation.

_Quantitative_

- Overshoot matches closely in both closed-loop cases.
- Settling time is slightly longer in simulation than the formula predicts. The formula is a 2nd-order approximation, and `stepinfo` uses an exact 2% band.
- The final value is 1 in both closed-loop cases, as predicted by T(0) = 1.

_Qualitative_

- Feedback converts an unbounded ramp response into a stable response that settles at the reference.
- Raising K from 1 to 1.145 raises overshoot from about 3% to 5%, because ζ falls from 0.74 to 0.69.
- Both closed-loop systems have the same real part of the poles (−2.3), so their settling times are nearly equal. Only the imaginary part, and hence the oscillation, changes.
- Zero steady-state error to a step comes from the integrator in the plant.

Tasks 2.1 and 2.2
- analytical work, shown in doc


