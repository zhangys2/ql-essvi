import csv, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).parents[1]


def test_adapt_cvi_interpolates_missing_strikes(tmp_path):
    quotes = tmp_path / "normalized.csv"
    cvi = tmp_path / "cvi.csv"
    out = tmp_path / "fitted.csv"
    quotes.write_text(
        "quote_id,time_to_expiry,forward,strike,included\n"
        "q1,1.0,100,90,true\n"
        "q2,1.0,100,100,true\n"
        "q3,1.0,100,110,true\n"
        "q4,2.0,100,100,true\n"
    )
    cvi.write_text(
        "time_to_expiry,strike,log_moneyness,fitted_total_variance,fitted_iv\n"
        "1.0,90,-0.1053605156578263,0.04,0.2\n"
        "1.0,110,0.09531017980432493,0.06,0.2449489742783178\n"
    )
    subprocess.run(
        [sys.executable, str(ROOT / "python/adapt_cvi_output.py"), str(quotes), str(cvi), str(out)],
        check=True,
    )
    rows = {r["quote_id"]: r for r in csv.DictReader(out.open())}
    assert rows["q1"]["status"] == "ok"
    assert rows["q3"]["status"] == "ok"
    assert rows["q2"]["status"] == "ok"
    assert 0.04 < float(rows["q2"]["fitted_total_variance"]) < 0.06
    assert rows["q4"]["status"] == "not_fitted"
