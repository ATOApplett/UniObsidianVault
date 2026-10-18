
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

2.3
_Quantitative_

- The smallest K gives no overshoot but is far slower, about 4 times the settling time of the others.
- The designed K gives the intended 5% overshoot.
- Doubling K from the design gives 17% overshoot, with Ts about the same (1.73 s against 1.80 s), as the common real part predicts.

_Qualitative_

- Overdamped (low K): smooth and monotonic, but slow, because the dominant pole is near the origin.
- Underdamped (high K): faster rise and visible oscillation, with the oscillation frequency increasing with K.
- All three settle at 1, because the integrator in the plant gives zero steady-state step error whatever K is.
- The design is sensitive to gain error: a ±20% error moves overshoot from 5% to about 2% or 8%.

edit code such that it only uses 5 (0.2 - 1.2)
## EGB345 Servo Motor Control Design - Task 2 (investigation of gain K)

`clear; clc; close all`

`km = 9.7;`

`alpha = 4.6;`

`s = tf('s');`

`G = km/(s*(s+alpha));`

`zeta5 = -log(0.05)/sqrt(pi^2 + log(0.05)^2);`

`KTask1 = 1.135; % value from your Task 1 design`

`%% Task 2.1 : closed-loop poles for the chosen K values --------------------`

`factors = [0.2 0.3 0.4 0.6 0.8 1.0 1.2 1.4 1.6 1.8 2.0]; % multiples of KTask1`

`Kvals = factors*KTask1;`

`Kcrit = alpha^2/(4*km); % critical damping gain`

`fprintf('Critical damping at K = %.3f (= %.3f x KTask1)\n\n', Kcrit, Kcrit/KTask1);`

`fprintf('%-6s %-8s %-26s %-7s %s\n','K/KT','K','poles','zeta','type');`

`for i = 1:numel(Kvals)`

`K = Kvals(i);`

`p = roots([1 alpha K*km]); % analytic: s^2 + alpha s + K km = 0`

`zeta = alpha/(2*sqrt(K*km));`

`if K < Kcrit, typ = 'overdamped'; else, typ = 'underdamped'; end`

`fprintf('%-6.1f %-8.3f %-26s %-7.3f %s\n', factors(i), K, ...`

`num2str(round(p.',3)), zeta, typ);`

`end`

`%% Task 2.2 : pole plot to CHECK your hand-drawn s-plane graph -------------`

`% (The sheet requires a HAND-DRAWN graph; this is only a reference.)`

`figure; hold on; grid on`

`for i = 1:numel(Kvals)`

`p = roots([1 alpha Kvals(i)*km]);`

`plot(real(p), imag(p), 'rx', 'MarkerSize', 9, 'LineWidth', 2);`

`end`

`plot([0 -alpha], [0 0], 'kx', 'MarkerSize', 12, 'LineWidth', 3); % open-loop poles`

`xline(-alpha/2, '--');`

`xlabel('Real axis'); ylabel('Imaginary axis');`

`title('Closed-loop poles for K = 0.2 to 2.0 x K_{Task1} (reference only)');`

`%% Task 2.3 : step responses for KTask1, smallest K and largest K ----------`

`Ksel = [min(Kvals) KTask1 max(Kvals)]; % 0.2, 1.0, 2.0 x KTask1`

`names = {'0.2 K_{Task1}','K_{Task1}','2.0 K_{Task1}'};`

`figure; hold on`

`for i = 1:3`

`step(feedback(Ksel(i)*G, 1), 8);`

`end`

`grid on; legend(names, 'Location', 'southeast');`

`title('Task 2.3: closed-loop unit step responses');`

`saveas(gcf, 'task2_3_step_responses.png');`

`for i = 1:3`

`fprintf('\n%s (K = %.3f):\n', names{i}, Ksel(i));`

`disp(stepinfo(feedback(Ksel(i)*G, 1)));`

`end`

