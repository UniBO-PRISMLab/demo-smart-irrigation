"""Plot soil moisture calibration readings and normalized percentages.

This script creates two graphs:
  1. Raw ADC values by soil condition with a linear regression trend.
  2. Normalized percent values (0-100) based on the dataset minimum and maximum.

Outputs are saved to the `scripts/images` directory.

Usage:
    python scripts/plot_moisture_calibration.py
"""

from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt

# Measured raw ADC values grouped by descriptive condition.
measurements = {
    "Ultra bagnato": [419, 415],
    "Super bagnato": [510, 515, 522],
    "Bagnato": [645, 630, 600],
    "Piuttosto secco": [675, 700, 705],
    "Completamente secco": [775, 822, 830],
}

condition_order = [
    "Ultra bagnato",
    "Super bagnato",
    "Bagnato",
    "Piuttosto secco",
    "Completamente secco",
]

script_dir = Path(__file__).resolve().parent
images_dir = script_dir / "images"
images_dir.mkdir(exist_ok=True)

# Flatten data for regression.
x = []
y_raw = []
labels = []
for idx, condition in enumerate(condition_order, start=1):
    values = measurements[condition]
    x.extend([idx] * len(values))
    y_raw.extend(values)
    labels.extend([condition] * len(values))

x = np.array(x, dtype=float)
y_raw = np.array(y_raw, dtype=float)

# Regression for raw ADC values.
coeffs_raw = np.polyfit(x, y_raw, 1)
slope_raw, intercept_raw = coeffs_raw

y_fit_raw = slope_raw * x + intercept_raw

ss_res_raw = np.sum((y_raw - y_fit_raw) ** 2)
ss_tot_raw = np.sum((y_raw - np.mean(y_raw)) ** 2)
r_squared_raw = 1 - ss_res_raw / ss_tot_raw

# Normalize values to 0-100 based on dataset min/max.
min_raw = np.min(y_raw)
max_raw = np.max(y_raw)

y_percent = 100 * (y_raw - min_raw) / (max_raw - min_raw)

coeffs_pct = np.polyfit(x, y_percent, 1)
slope_pct, intercept_pct = coeffs_pct

y_fit_pct = slope_pct * x + intercept_pct

ss_res_pct = np.sum((y_percent - y_fit_pct) ** 2)
ss_tot_pct = np.sum((y_percent - np.mean(y_percent)) ** 2)
r_squared_pct = 1 - ss_res_pct / ss_tot_pct

# Plot 1: Raw ADC values with regression.
plt.figure(figsize=(10, 6))
for idx, condition in enumerate(condition_order, start=1):
    condition_values = measurements[condition]
    plt.scatter(
        [idx] * len(condition_values),
        condition_values,
        label=condition,
        s=80,
        edgecolors="k",
        alpha=0.8,
    )

x_line = np.linspace(1, len(condition_order), 100)
y_line_raw = slope_raw * x_line + intercept_raw
plt.plot(x_line, y_line_raw, color="red", linestyle="--", linewidth=2, label="Fit lineare")

plt.xticks(range(1, len(condition_order) + 1), condition_order, rotation=30, ha="right")
plt.xlabel("Condizione del terreno")
plt.ylabel("Valore ADC grezzo")
plt.title("Calibrazione Umidità del Terreno: Valori ADC grezzi con Fit Lineare")
plt.grid(True, alpha=0.3)
plt.legend(loc="upper left", bbox_to_anchor=(1.02, 1), borderaxespad=0.)
annotation_raw = (
    f"ADC = {slope_raw:.1f} * indice_condizione + {intercept_raw:.1f}\n"
    f"R² = {r_squared_raw:.4f}"
)
plt.annotate(annotation_raw, xy=(0.98, 0.05), xycoords="axes fraction", ha="right", va="bottom",
             bbox=dict(boxstyle="round,pad=0.4", fc="white", alpha=0.85))
plt.tight_layout()
raw_output = images_dir / "moisture_calibration_raw.png"
plt.savefig(raw_output, dpi=300)
print(f"Saved raw ADC calibration plot to {raw_output}")

# Plot 2: Normalized percent values with regression.
plt.figure(figsize=(10, 6))
for idx, condition in enumerate(condition_order, start=1):
    condition_values = measurements[condition]
    condition_percent = 100 * (np.array(condition_values, dtype=float) - min_raw) / (max_raw - min_raw)
    plt.scatter(
        [idx] * len(condition_percent),
        condition_percent,
        label=condition,
        s=80,
        edgecolors="k",
        alpha=0.8,
    )

x_line = np.linspace(1, len(condition_order), 100)
y_line_pct = slope_pct * x_line + intercept_pct
plt.plot(x_line, y_line_pct, color="blue", linestyle="--", linewidth=2, label="Fit lineare")

plt.xticks(range(1, len(condition_order) + 1), condition_order, rotation=30, ha="right")
plt.xlabel("Condizione del terreno")
plt.ylabel("Umidità normalizzata (%)")
plt.title("Calibrazione Umidità del Terreno: Scala normalizzata 0-100")
plt.ylim(-5, 105)
plt.grid(True, alpha=0.3)
plt.legend(loc="upper left", bbox_to_anchor=(1.02, 1), borderaxespad=0.)
annotation_pct = (
    f"Percentuale = {slope_pct:.1f} * indice_condizione + {intercept_pct:.1f}\n"
    f"R² = {r_squared_pct:.4f}"
)
plt.annotate(annotation_pct, xy=(0.98, 0.05), xycoords="axes fraction", ha="right", va="bottom",
             bbox=dict(boxstyle="round,pad=0.4", fc="white", alpha=0.85))
plt.tight_layout()
pct_output = images_dir / "moisture_calibration_percent.png"
plt.savefig(pct_output, dpi=300)
print(f"Saved normalized percent calibration plot to {pct_output}")

plt.show()
