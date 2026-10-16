import json
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


def test_check_cli_reports_positive_loss_over_zero_as_hard_breach(
    tmp_path: Path,
) -> None:
    report_path = tmp_path / "fast-zero-reference.json"
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

    assert result.returncode == 2, result.stdout + result.stderr
    assert "quality_check_table=fast" in result.stdout
    loss_row = next(
        line
        for line in result.stdout.split("quality_check_table=fast\n", 1)[1].splitlines()
        if line.startswith("summed band loss warm / cold |")
    )
    assert "| inf |" in loss_row
    assert loss_row.endswith("MISS | BREACH")

    report = json.loads(report_path.read_text(encoding="utf-8"))
    assert report["runs"][0]["summed_band_loss_ratio"] is None
