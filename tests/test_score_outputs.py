import csv, json, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).parents[1]


def test_score_reports_distance_to_band(tmp_path):
    quotes = tmp_path / "quotes.csv"
    fitted = tmp_path / "fitted.csv"
    out = tmp_path / "score.json"
    quotes.write_text(
        "quote_id,included,bid_iv,ask_iv\n"
        "q1,true,0.20,0.22\n"
        "q2,true,0.20,0.22\n"
        "q3,false,0.20,0.22\n"
    )
    fitted.write_text(
        "quote_id,fitted_iv,status\n"
        "q1,0.21,ok\n"
        "q2,0.24,ok\n"
        "q3,0.50,excluded\n"
    )
    subprocess.run(
        [sys.executable, str(ROOT / "python/score_outputs.py"), str(quotes),
         "--fitted", str(fitted), "--output", str(out)],
        check=True,
    )
    score = json.loads(out.read_text())
    assert score["quotes_scored"] == 2
    assert score["bid_ask_containment"] == 0.5
    assert abs(score["mean_iv_distance_to_band"] - 0.01) < 1e-12
    assert abs(score["median_iv_distance_to_band"] - 0.01) < 1e-12
