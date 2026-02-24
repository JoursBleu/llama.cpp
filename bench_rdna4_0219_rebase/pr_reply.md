Thanks for the review! I've addressed all your feedback:

1. **Inlined type guard into `calc_nwarps`** — removed the standalone `mmvq_rdna4_nwarps8_beneficial()` function, moved the switch logic directly into `calc_nwarps()` as suggested.

2. **Hardcoded `8`** directly instead of `MMVQ_RDNA4_NWARPS` macro.

3. **Batch sizes 2-16** show no regression — nwarps=8 only applies to `ncols_dst=1`.

Types that get nwarps=8 on RDNA4 (ncols_dst=1):
`Q4_0, Q4_1, Q5_0, Q5_1, Q8_0, Q2_K, Q4_K, Q5_K, Q6_K, IQ4_NL, IQ4_XS`

Non-whitelisted types use nwarps=1 — zero code path change vs master.

#### Benchmark: Llama 2 7B, AMD Radeon AI PRO R9700 (gfx1201), ROCm 7.2, pp512, fa=1

**ub=1 — side-by-side with your 9060 XT results (commit 49a5ff4, no guard):**

| Type | nwarps=8? | R9700 Speedup | 9060 XT (no guard) | Notes |
|------|-----------|:---:|:---:|-------|
| IQ4_XS | **Yes** | 1.15x | 1.30x | ✅ both benefit |
| Q4_0 | **Yes** | 1.12x | 1.30x | ✅ both benefit |
| IQ4_NL | **Yes** | 1.10x | 1.29x | ✅ both benefit |
| Q5_0 | **Yes** | 1.10x | 1.24x | ✅ both benefit |
| Q5_1 | **Yes** | 1.10x | 1.22x | ✅ both benefit |
| Q4_1 | **Yes** | 1.09x | 1.27x | ✅ both benefit |
| Q5_K_S | **Yes** | 1.08x | 1.22x | ✅ both benefit |
| Q8_0 | **Yes** | 1.07x | 1.15x | ✅ both benefit |
| Q4_K_S | **Yes** | 1.06x | 1.23x | ✅ both benefit |
| Q6_K | **Yes** | 1.06x | 1.19x | ✅ both benefit |
| Q2_K_S | **Yes** | 1.04x | 1.12x | ✅ both benefit |
| Q2_K_M | **Yes** | 1.02x | — | ✅ both benefit |
| IQ1_S | No | 1.03x | 1.14x | ✅ neutral |
| IQ2_XS | No | 1.01x | 0.97x | guard prevents regression |
| IQ2_XXS | No | 1.01x | 0.98x | guard prevents regression |
| IQ3_M | No | 1.01x | 1.00x | ✅ neutral |
| IQ2_S | No | 1.00x | 0.97x | guard prevents regression |
| IQ3_S | No | 1.00x | 0.97x | guard prevents regression |
| IQ3_XS | No | 1.00x | 0.95x | guard prevents 5% regression |
| IQ3_XXS | No | 1.00x | 0.97x | guard prevents regression |
| Q3_K_S | No | 1.00x | 0.89x | guard prevents 11% regression |

<details>
<summary>Full benchmark table (all ub values, pp512)</summary>

| Model | ub | t/s master | t/s PR | Speedup |
|-------|---:|-----------:|-------:|--------:|
| IQ1_S | 1 | 143.80 | 147.66 | 1.03 |
| IQ1_S | 2 | 244.20 | 243.48 | 1.00 |
| IQ1_S | 4 | 398.70 | 396.41 | 0.99 |
| IQ1_S | 8 | 521.21 | 517.72 | 0.99 |
| IQ1_S | 16 | 1056.55 | 1054.25 | 1.00 |
| IQ2_S | 1 | 92.91 | 92.74 | 1.00 |
| IQ2_S | 2 | 167.14 | 166.92 | 1.00 |
| IQ2_S | 4 | 293.25 | 293.13 | 1.00 |
| IQ2_S | 8 | 408.81 | 408.11 | 1.00 |
| IQ2_S | 16 | 695.74 | 696.09 | 1.00 |
| IQ2_XS | 1 | 95.82 | 96.84 | 1.01 |
| IQ2_XS | 2 | 169.97 | 169.79 | 1.00 |
| IQ2_XS | 4 | 292.06 | 292.16 | 1.00 |
| IQ2_XS | 8 | 397.56 | 397.67 | 1.00 |
| IQ2_XS | 16 | 655.90 | 657.87 | 1.00 |
| IQ2_XXS | 1 | 98.31 | 99.31 | 1.01 |
| IQ2_XXS | 2 | 174.78 | 174.47 | 1.00 |
| IQ2_XXS | 4 | 305.79 | 305.14 | 1.00 |
| IQ2_XXS | 8 | 422.95 | 422.49 | 1.00 |
| IQ2_XXS | 16 | 885.90 | 884.71 | 1.00 |
| IQ3_S | 1 | 88.87 | 88.79 | 1.00 |
| IQ3_S | 2 | 165.85 | 166.09 | 1.00 |
| IQ3_S | 4 | 298.04 | 297.90 | 1.00 |
| IQ3_S | 8 | 407.97 | 407.23 | 1.00 |
| IQ3_S | 16 | 878.33 | 878.51 | 1.00 |
| IQ3_M | 1 | 90.53 | 91.80 | 1.01 |
| IQ3_M | 2 | 166.75 | 166.79 | 1.00 |
| IQ3_M | 4 | 289.08 | 288.98 | 1.00 |
| IQ3_M | 8 | 379.06 | 379.33 | 1.00 |
| IQ3_M | 16 | 906.60 | 907.34 | 1.00 |
| IQ3_XS | 1 | 89.28 | 89.35 | 1.00 |
| IQ3_XS | 2 | 166.46 | 166.50 | 1.00 |
| IQ3_XS | 4 | 294.47 | 294.71 | 1.00 |
| IQ3_XS | 8 | 408.10 | 408.80 | 1.00 |
| IQ3_XS | 16 | 884.10 | 883.41 | 1.00 |
| IQ3_XXS | 1 | 90.27 | 90.18 | 1.00 |
| IQ3_XXS | 2 | 165.94 | 165.85 | 1.00 |
| IQ3_XXS | 4 | 291.70 | 291.32 | 1.00 |
| IQ3_XXS | 8 | 406.33 | 407.46 | 1.00 |
| IQ3_XXS | 16 | 836.36 | 835.81 | 1.00 |
| IQ4_NL ★ | 1 | 103.44 | 113.83 | 1.10 |
| IQ4_NL | 2 | 191.68 | 192.36 | 1.00 |
| IQ4_NL | 4 | 374.08 | 374.03 | 1.00 |
| IQ4_NL | 8 | 494.09 | 494.18 | 1.00 |
| IQ4_NL | 16 | 1167.56 | 1167.99 | 1.00 |
| IQ4_XS ★ | 1 | 103.95 | 119.31 | 1.15 |
| IQ4_XS | 2 | 191.12 | 191.29 | 1.00 |
| IQ4_XS | 4 | 371.51 | 372.23 | 1.00 |
| IQ4_XS | 8 | 593.15 | 593.33 | 1.00 |
| IQ4_XS | 16 | 1275.74 | 1282.39 | 1.01 |
| Q2_K_M ★ | 1 | 103.87 | 106.31 | 1.02 |
| Q2_K_M | 2 | 175.57 | 175.47 | 1.00 |
| Q2_K_M | 4 | 272.04 | 273.24 | 1.00 |
| Q2_K_M | 8 | 337.92 | 340.19 | 1.01 |
| Q2_K_M | 16 | 824.14 | 823.49 | 1.00 |
| Q2_K_S ★ | 1 | 126.89 | 132.33 | 1.04 |
| Q2_K_S | 2 | 206.16 | 205.74 | 1.00 |
| Q2_K_S | 4 | 293.33 | 292.87 | 1.00 |
| Q2_K_S | 8 | 333.20 | 332.61 | 1.00 |
| Q2_K_S | 16 | 655.38 | 657.98 | 1.00 |
| Q3_K_S | 1 | 101.63 | 101.53 | 1.00 |
| Q3_K_S | 2 | 172.47 | 171.84 | 1.00 |
| Q3_K_S | 4 | 268.84 | 267.19 | 0.99 |
| Q3_K_S | 8 | 340.80 | 338.35 | 0.99 |
| Q3_K_S | 16 | 890.18 | 890.41 | 1.00 |
| Q4_0 ★ | 1 | 102.17 | 114.00 | 1.12 |
| Q4_0 | 2 | 193.48 | 193.45 | 1.00 |
| Q4_0 | 4 | 374.20 | 374.65 | 1.00 |
| Q4_0 | 8 | 490.88 | 491.06 | 1.00 |
| Q4_0 | 16 | 1104.28 | 1104.14 | 1.00 |
| Q4_1 ★ | 1 | 97.47 | 106.24 | 1.09 |
| Q4_1 | 2 | 182.85 | 182.66 | 1.00 |
| Q4_1 | 4 | 357.16 | 357.00 | 1.00 |
| Q4_1 | 8 | 497.84 | 497.93 | 1.00 |
| Q4_1 | 16 | 1134.97 | 1136.08 | 1.00 |
| Q4_K_S ★ | 1 | 100.86 | 107.39 | 1.06 |
| Q4_K_S | 2 | 178.30 | 178.12 | 1.00 |
| Q4_K_S | 4 | 261.84 | 261.48 | 1.00 |
| Q4_K_S | 8 | 299.35 | 298.62 | 1.00 |
| Q4_K_S | 16 | 1060.37 | 1056.37 | 1.00 |
| Q5_0 ★ | 1 | 89.30 | 98.24 | 1.10 |
| Q5_0 | 2 | 167.96 | 167.66 | 1.00 |
| Q5_0 | 4 | 327.47 | 327.53 | 1.00 |
| Q5_0 | 8 | 468.18 | 468.72 | 1.00 |
| Q5_0 | 16 | 996.39 | 996.21 | 1.00 |
| Q5_1 ★ | 1 | 85.03 | 93.16 | 1.10 |
| Q5_1 | 2 | 161.32 | 161.24 | 1.00 |
| Q5_1 | 4 | 312.29 | 312.64 | 1.00 |
| Q5_1 | 8 | 527.14 | 528.25 | 1.00 |
| Q5_1 | 16 | 823.91 | 823.48 | 1.00 |
| Q5_K_S ★ | 1 | 89.72 | 96.69 | 1.08 |
| Q5_K_S | 2 | 162.12 | 162.18 | 1.00 |
| Q5_K_S | 4 | 251.13 | 251.20 | 1.00 |
| Q5_K_S | 8 | 292.07 | 291.69 | 1.00 |
| Q5_K_S | 16 | 1029.59 | 1029.49 | 1.00 |
| Q6_K ★ | 1 | 78.70 | 83.27 | 1.06 |
| Q6_K | 2 | 146.57 | 146.55 | 1.00 |
| Q6_K | 4 | 254.34 | 254.29 | 1.00 |
| Q6_K | 8 | 332.65 | 333.20 | 1.00 |
| Q6_K | 16 | 811.08 | 811.38 | 1.00 |
| Q8_0 ★ | 1 | 63.66 | 68.38 | 1.07 |
| Q8_0 | 2 | 120.87 | 120.77 | 1.00 |
| Q8_0 | 4 | 237.18 | 237.44 | 1.00 |
| Q8_0 | 8 | 421.77 | 421.44 | 1.00 |
| Q8_0 | 16 | 852.86 | 853.12 | 1.00 |

★ = nwarps=8 applied (whitelisted type, ub=1)

</details>

**Summary:**
- Whitelisted types ub=1: **+8.3% average** (1.02x – 1.15x)
- Non-whitelisted types ub=1: +0.7% average (nwarps=1, same as master)
- ub≥2: all within noise (worst: 0.99x)
- Regressions (<0.995x): 4 out of 105 points — IQ1_S ub=4 0.99x, IQ1_S ub=8 0.99x, Q3_K_S ub=4 0.99x, Q3_K_S ub=8 0.99x

Hardware: AMD Radeon AI PRO R9700 (gfx1201), ROCm 7.2.0, amdclang
Master: `03fd9d3bb`, PR: `b1f4a5ff5`
