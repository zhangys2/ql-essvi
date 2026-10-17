import json
import math
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BENCH = ROOT / "build" / "essvi_bench"


def run_benchmark(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [str(BENCH), *args],
        cwd=ROOT,
        capture_output=True,
        text=True,
        timeout=30,
        check=False,
    )


def test_check_cli_serializes_loss_ratio_and_status(tmp_path: Path) -> None:
    report_path = tmp_path / "fast-quality.json"
    result = run_benchmark(
        "--scenario",
        "fast",
        "--scenario-updates",
        "1",
        "--seed",
        "2",
        "--timings-out",
        str(report_path),
        "--check",
    )

    assert result.returncode in (0, 2), result.stdout + result.stderr
    assert "quality_check_table=fast" in result.stdout
    loss_row = next(
        line
        for line in result.stdout.split("quality_check_table=fast\n", 1)[1].splitlines()
        if line.startswith("summed band loss warm / cold |")
    )
    report = json.loads(report_path.read_text(encoding="utf-8"))
    ratio = report["runs"][0]["summed_band_loss_ratio"]
    printed_ratio = loss_row.split("|")[1].strip()

    if ratio is None:
        assert printed_ratio == "inf"
        assert loss_row.endswith("MISS | BREACH")
        return

    assert math.isfinite(ratio)
    assert math.isclose(float(printed_ratio), ratio, rel_tol=1e-15)
    expected_target = "PASS" if ratio <= 1.05 else "MISS"
    expected_hard_limit = "PASS" if ratio <= 1.2 else "BREACH"
    assert loss_row.endswith(f"{expected_target} | {expected_hard_limit}")
