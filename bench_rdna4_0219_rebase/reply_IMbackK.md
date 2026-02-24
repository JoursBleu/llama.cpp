Thanks for the review!

1. **`MMVQ_RDNA4_ROWS` block removed** — you were right, since `rows_per_block=1` is the default return value of `calc_rows_per_block()`, the RDNA4-specific block was pointless. Removed both the macro and the block.

2. **Wave size** — agreed, all RDNA devices are wave32-only via HIP. The current code doesn't touch wave size logic; it only changes `nwarps` in `calc_nwarps()`.
