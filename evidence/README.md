# Reproducibility evidence

`benchmark_manifest.json` binds the local benchmark artifacts to the ql-essvi commit and the pushed CVI notebook commit. It records SHA-256 hashes, row counts, source repository, and the exact adapter/build commands.

The generated CSVs remain ignored because they are data products. The manifest and summary are committed so an auditor can verify the cross-repository provenance and expected dimensions without copying the full 14,364-row snapshot into Git.

The CVI source of truth is the pushed notebook commit recorded in the manifest. Its additive export cell writes `surface_grid.csv`, `fitted_quotes.csv`, `parameters.csv`, and `diagnostics.csv` without changing fitting cells.
