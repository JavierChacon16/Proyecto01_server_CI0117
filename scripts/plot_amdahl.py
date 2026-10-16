#!/usr/bin/env python3
import csv
import statistics
import os
import matplotlib.pyplot as plt

# Detectar la raiz del repo (un nivel arriba de scripts/)
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_DIR = os.path.dirname(SCRIPT_DIR)

CSV_PATH = os.path.join(REPO_DIR, "evidencia/local/bench_reps.csv")
DOCS_DIR = os.path.join(REPO_DIR, "docs")

os.makedirs(DOCS_DIR, exist_ok=True)

# --- 1. Leer CSV ---
data = {}
with open(CSV_PATH) as f:
    for row in csv.DictReader(f):
        t = int(row["hilos"])
        data.setdefault(t, []).append(float(row["total_s"]))

Ts = sorted(data.keys())
medianas = {t: statistics.median(data[t]) for t in Ts}

# --- 2. Calcular S, E, e ---
S = {t: medianas[1] / medianas[t] for t in Ts}
E = {t: S[t] / t for t in Ts}
e = {t: (1/S[t] - 1/t) / (1 - 1/t) for t in Ts if t > 1}

# --- 3. Estimar p y predecir Amdahl ---
p = 3.918275 / 3.923503
def amdahl(n): return 1 / ((1 - p) + p/n)
S_A = {t: amdahl(t) for t in Ts}

# --- 4. Imprimir medianas para verificar ---
print("Medianas calculadas:")
for t in Ts:
    print(f"  T={t}: {medianas[t]:.4f}")
print(f"p = {p:.4f}")

# --- 5. Grafico 1: Speedup ---
plt.figure(figsize=(6, 4))
plt.plot(Ts, [S[t] for t in Ts], "o-", label="Medido")
plt.plot(Ts, [S_A[t] for t in Ts], "s--", label=f"Amdahl (p={p:.4f})")
plt.axvline(4, color="gray", ls=":", alpha=0.5)
plt.xlabel("Hilos"); plt.ylabel("Aceleracion S(T)")
plt.title("Aceleracion: 2048x2048, 200 pasos")
plt.legend(); plt.grid(True, alpha=0.3)
plt.tight_layout()
plt.savefig(os.path.join(DOCS_DIR, "fig_amdahl.pdf"))

# --- 6. Grafico 2: Eficiencia ---
plt.figure(figsize=(6, 4))
plt.plot(Ts, [E[t] for t in Ts], "o-")
plt.xlabel("Hilos"); plt.ylabel("Eficiencia E(T)")
plt.title("Eficiencia"); plt.grid(True, alpha=0.3)
plt.tight_layout()
plt.savefig(os.path.join(DOCS_DIR, "fig_eficiencia.pdf"))

# --- 7. Grafico 3: Karp-Flatt ---
plt.figure(figsize=(6, 4))
plt.plot(list(e.keys()), list(e.values()), "o-")
plt.xlabel("Hilos"); plt.ylabel("e(T)")
plt.title("Fraccion serial efectiva (Karp-Flatt)")
plt.grid(True, alpha=0.3)
plt.tight_layout()
plt.savefig(os.path.join(DOCS_DIR, "fig_karpflatt.pdf"))

print("Graficos guardados en docs/")
