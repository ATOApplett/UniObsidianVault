 

**Total runtime target: 3:00** — timings below are guides; aim to stay within ±10s of each segment.

## Segment 1 — Hook & Problem Setup (0:00–0:20)

  

**Visual:** Show Figure 1 (the SEA schematic) full-screen — motor circuit on the left, gearbox in the middle, spring-load-damper on the right.

  

**Say:**

> "This is a Series Elastic Actuator — it drives a load through a spring so the motor's inertia is decoupled from the load. Our goal: find the transfer function θ_L(s)/E_a(s) — how load angle responds to input voltage."

  

**On-screen text overlay:** `Goal: θ_L(s) / E_a(s)`

  

---

  

## Segment 2 — Two Degrees of Freedom & Reflection (0:20–0:50)

  

**Visual:** Highlight θ_m(s) at the motor shaft and θ_L(s) at the load, then show the gearbox reflection formulas as text.

  

**Say:**

> "We have two degrees of freedom: the motor shaft angle and the load angle. Since they're separated by a gearbox, we reflect the motor's inertia and torque across to the load side using the gear ratio N1 over N2."

  

**On-screen text overlay (keep brief, ~4s each):**

$$T_e(s)=\frac{N_2}{N_1}T_m(s) \qquad \theta_e(s)=\frac{N_1}{N_2}\theta_m(s) \qquad J_e(s)=\left(\frac{N_2}{N_1}\right)^2J_as^2$$

  

---

  

## Segment 3 — Two Torque-Balance Equations (0:50–1:30)

  

**Visual:** Split screen or sequential reveal — motor-shaft equation, then load-shaft equation.

  

**Say:**

> "We write torque balance at the motor shaft — that's equation 1 — and at the load shaft — equation 2. The coupling spring links them together."

  

**On-screen text (show both, ~15s each):**

  

Eqn (1) — motor shaft:

$$T_m(s) = J_as^2\theta_m(s) + k\left(\frac{N_1}{N_2}\right)^2\theta_m(s) - \frac{N_1}{N_2}k\,\theta_L(s)$$

  

Eqn (2) — load shaft:

$$0 = (J_Ls^2+D_2s+K_2+k)\theta_L(s) - k\frac{N_1}{N_2}\theta_m(s)$$

  

**Tip:** verbally flag the *one* difference from a simpler version — "notice the load side has both a spring K2 *and* a damper to the wall — that's what adds K2 into this bracket."

  

---

  

## Segment 4 — Eliminate θ_L(s), Solve for θ_m(s) (1:30–2:00)

  

**Visual:** Show algebra collapsing — arrow animation from eqn (2) → isolated θ_L(s) → substituted into eqn (1).

  

**Say:**

> "We isolate θ_L from equation 2, substitute it into equation 1, and simplify — this eliminates θ_L and leaves torque purely in terms of θ_m."

  

**On-screen text (final simplified line only — skip the intermediate algebra on screen, just narrate it happened):**

$$T_m(s) = \left(J_as^2+kn^2\frac{Q(s)}{P(s)}\right)\theta_m(s)$$

  

---

  

## Segment 5 — THE KEY CORRECTION MOMENT: KVL vs KCL (2:00–2:30)

  

**This is your focal teaching point — give it the most visual weight.**

  

**Visual:** Show the motor circuit close-up (R, e_a, i_a, back-EMF). Put a big **KVL** label, cross out a **KCL** label if you want a "correction" visual gag.

  

**Say:**

> "Now for the electrical side — this step is *voltage summation around the loop*, so it's Kirchhoff's Voltage Law, KVL — not KCL. We sum the source voltage against the resistor drop and the back-EMF."

  

**On-screen text:**

$$E_a(s) = Ri_a(s) + V_b(s) = \frac{R}{K_t}T_m(s) + sK_b\theta_m(s)$$

  

**Optional callout box (2–3 seconds):** *"Common mix-up: this is a voltage loop equation → KVL, even though some course notes label it 'KCL' by convention."*

  

---

  

## Segment 6 — Combine Everything into the Final Transfer Function (2:30–2:55)

  

**Visual:** Show the final boxed transfer function with numerator/denominator terms color-coded (numerator = blue, denominator = orange) to visually separate them.

  

**Say:**

> "Substituting back and clearing fractions gives our final result — a fourth-order transfer function relating load angle to input voltage."

  

**On-screen text (final boxed equation, hold for full remaining time):**

$$\frac{\theta_L(s)}{E_a(s)} = \frac{kn}{as^4+bs^3+cs^2+ds+e}$$

  

*(Optionally show the plugged-in numeric version if space allows — e.g. `0.75 / (0.0018s⁴ + 0.3036s³ + 2.004s² + 48.75s + 5.625)`)*

  

---

  

## Segment 7 — Quick Wrap-Up (2:55–3:00)

  

**Visual:** Return to full schematic, fade to end card.

  

**Say:**

> "Two torque-balance equations, one gearbox reflection, and a KVL loop equation — that's the full path from voltage input to load displacement."

  

---

  

## Production Checklist

  

- [ ] Keep each on-screen equation visible for at least 4–5 seconds before cutting — viewers need time to read, not just hear

- [ ] Use consistent color-coding for $\theta_m$ (motor-side) vs $\theta_L$ (load-side) terms throughout, so the eye can track which equation governs which part of the system

- [ ] Don't narrate every algebra step out loud — verbally summarize ("we substitute and simplify") while showing only the *before* and *after* lines on screen; showing full intermediate algebra will blow the 3-minute budget

- [ ] Reserve the most screen-time and clearest callout for the KVL correction (Segment 5) since that's the specific point you want to land

- [ ] End on the boxed final transfer function — that's the payoff the viewer came for

  

## Suggested Pacing Summary

  

| Segment | Time | Duration |

|---|---|---|

| 1. Hook & setup | 0:00–0:20 | 20s |

| 2. Reflection formulas | 0:20–0:50 | 30s |

| 3. Two torque equations | 0:50–1:30 | 40s |

| 4. Eliminate θ_L | 1:30–2:00 | 30s |

| 5. **KVL correction (key point)** | 2:00–2:30 | 30s |

| 6. Final transfer function | 2:30–2:55 | 25s |

| 7. Wrap-up | 2:55–3:00 | 5s |