#!/usr/bin/env python3
"""
Squelch envelope simulation — faithful 1:1 replica of C squelch.c.

Parameters match rtl_multi naming:
  rate_out  — output sample rate sent over TCP (-r, default 48k)
  rate_in   — device sample rate used for demodulation (-s, default 12k)
              signal bandwidth is derived from this and modulation mode.
"""

import numpy as np
import matplotlib.pyplot as plt
from scipy.optimize import curve_fit

# --- C reference constants (squelch.c) ---
CUTOFF = 3200.0       # HP filter cutoff Hz
ATTACK_MS = 2.0
DECAY_MS = 100.0
NOISE_SEC = 10.0
BUF_SIZE = 2048

# rtl_multi common rate_out values (TCP stream rate), multiples of 8kHz
rate_outs = list(range(8000, 96001, 8000))

# signal bandwidth multiples of 6kHz up to 60kHz
rate_ins = list(range(6000, 60001, 6000))


def lpf_coef(cutoff, sample_rate):
    """First-order IIR low-pass filter coefficient."""
    rc = 1.0 / (2.0 * np.pi * cutoff)
    dt = 1.0 / sample_rate
    return dt / (rc + dt)


def simulate(rate_out, signal_bw, noise_sec=NOISE_SEC):
    """
    Run band-limited white noise through squelch, return avg envelope.

    rate_out   — TCP sample rate (determines sample grid)
    signal_bw  — bandwidth of demodulated signal in Hz
    """
    # HP filter (same as C)
    dt = 1.0 / rate_out
    rc = 1.0 / (2.0 * np.pi * CUTOFF)
    hp_coef = rc / (rc + dt)

    # envelope follower coefficients
    atk = np.exp(-1.0 / ((ATTACK_MS / 1000.0) * rate_out))
    dcy = np.exp(-1.0 / ((DECAY_MS / 1000.0) * rate_out))

    # LP filter to limit noise bandwidth
    lpf = lpf_coef(signal_bw, rate_out)

    n = int(noise_sec * rate_out)
    x = np.random.uniform(-1.0, 1.0, n)

    # low-pass filter to band-limit noise
    lp_state = 0.0
    for i in range(n):
        lp_state = lp_state + lpf * (x[i] - lp_state)
        x[i] = lp_state

    # HP filter + envelope follower (1:1 with C squelch_process)
    hp_state = 0.0
    hp_prev = 0.0
    envelope = 0.0
    envs = []

    for i in range(n):
        noise = hp_coef * (hp_state + x[i] - hp_prev)
        hp_prev = x[i]
        hp_state = noise

        rect = abs(noise)
        coeff = atk if rect > envelope else dcy
        envelope = coeff * envelope + (1.0 - coeff) * rect

        if (i + 1) % BUF_SIZE == 0:
            envs.append(envelope)

    settle = int(0.5 * rate_out / BUF_SIZE)
    avg = np.mean(envs[settle:])
    return avg


# --- formula models ---
def model_simple(xdata, a, b, c):
    """env = a * (sbw/CUTOFF)^b * (ro/CUTOFF)^c"""
    sbw, ro = xdata[:, 0], xdata[:, 1]
    return a * (sbw / CUTOFF) ** b * (ro / CUTOFF) ** c

def model_sat(xdata, a, b, c, d):
    """Saturating: env = a * (1 - exp(-b * sbw/CUTOFF)) * (ro/CUTOFF)^c + d"""
    sbw, ro = xdata[:, 0], xdata[:, 1]
    return a * (1.0 - np.exp(-b * sbw / CUTOFF)) * (ro / CUTOFF) ** c + d

def model_rat(xdata, a, b, c, d):
    """Rational: env = (a * sbw + b) / (c * ro + d)"""
    sbw, ro = xdata[:, 0], xdata[:, 1]
    return (a * sbw + b) / (c * ro + d)

def model_physics(xdata, a, b):
    """Physics-based: env = a * sqrt(sbw - C*arctan(sbw/C)) * (ro/C)^b"""
    sbw, ro = xdata[:, 0], xdata[:, 1]
    return a * np.sqrt(np.maximum(sbw - CUTOFF * np.arctan(sbw / CUTOFF), 0)) * (ro / CUTOFF) ** b

def model_d2c_power(xdata, a, b, c):
    """d=2c, power amp: env = a * (sbw/C)^b * ratio^c / (1 + ratio^(2c))"""
    sbw, ro = xdata[:, 0], xdata[:, 1]
    ratio = np.clip(2.0 * sbw / ro, 0.001, 10.0)
    return a * (sbw / CUTOFF) ** b * ratio ** c / (1.0 + ratio ** (2.0 * c))

def model_d2c_sat(xdata, a, b, c):
    """d=2c, saturating amp: env = a * (1-exp(-b*sbw/C)) * ratio^c / (1 + ratio^(2c))"""
    sbw, ro = xdata[:, 0], xdata[:, 1]
    ratio = np.clip(2.0 * sbw / ro, 0.001, 10.0)
    return a * (1.0 - np.exp(-b * sbw / CUTOFF)) * ratio ** c / (1.0 + ratio ** (2.0 * c))

def model_d2c_arctan(xdata, a, c):
    """d=2c, arctan amp: env = a * sqrt(sbw-C*arctan(sbw/C)) * ratio^c / (1 + ratio^(2c))"""
    sbw, ro = xdata[:, 0], xdata[:, 1]
    ratio = np.clip(2.0 * sbw / ro, 0.001, 10.0)
    arctan_term = np.sqrt(np.maximum(sbw - CUTOFF * np.arctan(sbw / CUTOFF), 0))
    return a * arctan_term * ratio ** c / (1.0 + ratio ** (2.0 * c))

def model_d2c_log(xdata, a, b, c):
    """d=2c, log amp: env = a * log(1+sbw/C)^b * ratio^c / (1 + ratio^(2c))"""
    sbw, ro = xdata[:, 0], xdata[:, 1]
    ratio = np.clip(2.0 * sbw / ro, 0.001, 10.0)
    return a * np.log(1.0 + sbw / CUTOFF) ** b * ratio ** c / (1.0 + ratio ** (2.0 * c))

def model_d2c_sqrt(xdata, a, b, c):
    """d=2c, sqrt amp: env = a * sqrt(sbw/C)^b * ratio^c / (1 + ratio^(2c))"""
    sbw, ro = xdata[:, 0], xdata[:, 1]
    ratio = np.clip(2.0 * sbw / ro, 0.001, 10.0)
    return a * np.sqrt(sbw / CUTOFF) ** b * ratio ** c / (1.0 + ratio ** (2.0 * c))

def model_ratio_free(xdata, a, b, c, d):
    """Unconstrained: env = a * ratio^b / (1 + ratio^c) * (ro/C)^d"""
    sbw, ro = xdata[:, 0], xdata[:, 1]
    ratio = np.clip(2.0 * sbw / ro, 0.001, 10.0)
    return a * ratio ** b / (1.0 + ratio ** c) * (ro / CUTOFF) ** d

MODELS = [
    ("d2c_power", model_d2c_power, [0.5, 0.3, 0.5]),
    ("d2c_sat", model_d2c_sat, [0.5, 1.0, 0.5]),
    ("d2c_arctan", model_d2c_arctan, [0.01, 0.5]),
    ("d2c_log", model_d2c_log, [0.5, 1.0, 0.5]),
    ("d2c_sqrt", model_d2c_sqrt, [0.5, 0.5, 0.5]),
    ("ratio_free", model_ratio_free, [0.5, 0.5, 2.0, 0.2]),
]


# --- run all combos ---
print(f"{'rate_out':>8} | {'signal_bw':>9} | {'AvgEnv':>8} | {'BW/Nyq':>7} | {'Cutoff/BW':>9}")
print("-" * 58)

rows = []
for ro in rate_outs:
    nyq = ro / 2.0
    for sbw in rate_ins:
        avg = simulate(ro, sbw)
        bw_nyq = sbw / nyq
        cut_bw = CUTOFF / sbw
        print(f"{ro:>8} | {sbw:>9} | {avg:>8.4f} | {bw_nyq:>7.4f} | {cut_bw:>9.4f}")
        rows.append((ro, sbw, avg))

# --- re-fit models on full 120-point grid ---
sim_sbw = np.array([r[1] for r in rows])
sim_ro = np.array([r[0] for r in rows])
sim_env = np.array([r[2] for r in rows])
sim_xdata = np.column_stack((sim_sbw, sim_ro))

best_name, best_fn, best_popt, best_err = None, None, None, 999
for name, fn, p0 in MODELS:
    try:
        popt, _ = curve_fit(fn, sim_xdata, sim_env, p0=p0, maxfev=50000)
        pred = fn(sim_xdata, *popt)
        max_err = np.max(np.abs(sim_env - pred))
        mean_err = np.mean(np.abs(sim_env - pred))
        print(f"{name:>14}: max_err={max_err:.4f}  mean_err={mean_err:.4f}  params={[f'{p:.4f}' for p in popt]}")
        if max_err < best_err:
            best_name, best_fn, best_popt, best_err = name, fn, popt, max_err
    except Exception as e:
        print(f"{name:>14}: failed ({e})")

print(f"\nBest model: {best_name}")
pstr = [f"{p:.4f}" for p in best_popt]
print(f"Parameters: {', '.join(pstr)}")

pred_env = best_fn(sim_xdata, *best_popt)
residuals = sim_env - pred_env

worst_idx = np.argsort(np.abs(residuals))[::-1][:5]
print("\nWorst cells:")
for idx in worst_idx:
    ro, sbw, avg = rows[idx]
    print(f"  ro={ro:>6}  sbw={sbw:>6}  sim={avg:.4f}  pred={pred_env[idx]:.4f}  err={residuals[idx]:+.4f}")

# --- save model ---
import json
model_data = {
    "formula": best_name,
    "params": {f"p{i}": float(p) for i, p in enumerate(best_popt)},
    "cutoff": CUTOFF,
    "trained_on": f"rate_out={rate_outs[0]}-{rate_outs[-1]}, signal_bw={rate_ins[0]}-{rate_ins[-1]}, {len(rows)} points",
    "max_error": float(best_err),
    "mean_error": float(np.mean(np.abs(residuals))),
}
if best_name == "saturating":
    model_data["formula_str"] = f"p0·(1-exp(-p1·sbw/{CUTOFF:.0f}))·(ro/{CUTOFF:.0f})^p2+p3"
    model_data["a"] = float(best_popt[0])
    model_data["b"] = float(best_popt[1])
    model_data["c"] = float(best_popt[2])
    model_data["d"] = float(best_popt[3])
elif best_name == "power law":
    model_data["formula_str"] = f"p0·(sbw/{CUTOFF:.0f})^p1·(ro/{CUTOFF:.0f})^p2"
    model_data["a"] = float(best_popt[0])
    model_data["b"] = float(best_popt[1])
    model_data["c"] = float(best_popt[2])
elif best_name == "ratio":
    model_data["formula_str"] = f"a·(2·sbw/ro)^b / (1+(2·sbw/ro)^c) · (ro/{CUTOFF:.0f})^d"
    model_data["a"] = float(best_popt[0])
    model_data["b"] = float(best_popt[1])
    model_data["c"] = float(best_popt[2])
    model_data["d"] = float(best_popt[3])

with open("squelch_model.json", "w") as f:
    json.dump(model_data, f, indent=2)
print(f"\nSaved to squelch_model.json")

# --- build grids ---
ros_with_data = sorted(set(r[0] for r in rows))
sbws_with_data = sorted(set(r[1] for r in rows))

sim_grid = np.full((len(sbws_with_data), len(ros_with_data)), np.nan)
pred_grid = np.full((len(sbws_with_data), len(ros_with_data)), np.nan)
for ro, sbw, avg in rows:
    ri = sbws_with_data.index(sbw)
    ci = ros_with_data.index(ro)
    sim_grid[ri, ci] = avg
    pred_grid[ri, ci] = best_fn(np.array([[sbw, ro]]), *best_popt)[0]

# --- plot ---
fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(18, 8), sharey=True)

# left: simulation
im1 = ax1.imshow(sim_grid, aspect="auto", cmap="viridis", origin="lower",
                 vmin=0.25, vmax=0.60)
ax1.set_xticks(range(len(ros_with_data)))
ax1.set_xticklabels([f"{r/1000:.0f}k" for r in ros_with_data], fontsize=8)
ax1.set_yticks(range(len(sbws_with_data)))
ax1.set_yticklabels([f"{b/1000:.0f}k" for b in sbws_with_data], fontsize=8)
ax1.set_xlabel("rate_out (Hz)")
ax1.set_ylabel("signal_bw (Hz)")
ax1.set_title("Simulated (reference)")

for ri in range(len(sbws_with_data)):
    peak_ci = np.nanargmax(sim_grid[ri, :])
    for ci in range(len(ros_with_data)):
        v = sim_grid[ri, ci]
        if not np.isnan(v):
            if ci == peak_ci:
                ax1.text(ci, ri, f"{v:.3f}", ha="center", va="center",
                         color="red", fontsize=8, fontweight="bold")
            else:
                ax1.text(ci, ri, f"{v:.3f}", ha="center", va="center",
                         color="white" if v < 0.45 else "black", fontsize=7)

plt.colorbar(im1, ax=ax1, label="Avg envelope")

# right: re-fitted model
im2 = ax2.imshow(pred_grid, aspect="auto", cmap="viridis", origin="lower",
                 vmin=0.25, vmax=0.60)
ax2.set_xticks(range(len(ros_with_data)))
ax2.set_xticklabels([f"{r/1000:.0f}k" for r in ros_with_data], fontsize=8)
ax2.set_xlabel("rate_out (Hz)")
ax2.set_title(f"Re-fitted {best_name} (max err={best_err:.3f})")

for ri in range(len(sbws_with_data)):
    peak_ci = np.nanargmax(pred_grid[ri, :])
    for ci in range(len(ros_with_data)):
        v = pred_grid[ri, ci]
        err = abs(sim_grid[ri, ci] - v) if not np.isnan(sim_grid[ri, ci]) else 0
        if ci == peak_ci:
            ax2.text(ci, ri, f"{v:.3f}\n({err:.3f})",
                     ha="center", va="center",
                     color="red", fontsize=8, fontweight="bold")
        else:
            ax2.text(ci, ri, f"{v:.3f}\n({err:.3f})",
                     ha="center", va="center",
                     color="white" if v < 0.45 else "black", fontsize=7)

plt.colorbar(im2, ax=ax2, label="Avg envelope")

fig.suptitle("Squelch envelope: simulation vs re-fitted model\n"
             "(3.2kHz HP cutoff, 2ms attack, 100ms decay)", fontsize=13)
plt.tight_layout()
plt.savefig("squelch_sim.png", dpi=150)
plt.show()
