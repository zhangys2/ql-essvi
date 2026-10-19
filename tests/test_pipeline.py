import csv, json, subprocess, sys, statistics
import pytest
from pathlib import Path

ROOT=Path(__file__).parents[1]
def test_pipeline(tmp_path):
    chain=tmp_path/'chain.csv'; dfs=tmp_path/'dfs.csv'; out=tmp_path/'normalized.csv'
    chain.write_text('Effective Date,Expiry Date,Last Trade Date,Option Type,Strike,Bid,Ask\n11-11-2024,11-18-2024,11-01-2024,Call,6000,100,110\n11-11-2024,11-18-2024,11-01-2024,Put,6000,100,110\n')
    dfs.write_text('T,DF\n0,1\n1,0.98\n')
    subprocess.run([sys.executable,str(ROOT/'python/prepare_inputs.py'),str(chain),str(dfs),str(out)],check=True)
    rows=list(csv.DictReader(out.open())); assert len(rows)==2; assert all(r['quote_id'] for r in rows); assert all(r['filter_reason'] or r['included']=='true' for r in rows)

def test_prepare_inputs_emits_median_normalized_weights(tmp_path):
    chain=tmp_path/'chain.csv'; dfs=tmp_path/'dfs.csv'; out=tmp_path/'normalized.csv'
    chain.write_text('Effective Date,Expiry Date,Last Trade Date,Option Type,Strike,Bid,Ask\n11-11-2024,11-18-2024,11-11-2024,Call,6000,100,110\n11-11-2024,11-18-2024,11-11-2024,Put,6000,100,110\n11-11-2024,11-18-2024,11-11-2024,Call,6100,45,55\n11-11-2024,11-18-2024,11-11-2024,Put,5900,45,55\n')
    dfs.write_text('T,DF\n0,1\n1,0.98\n')
    subprocess.run([sys.executable,str(ROOT/'python/prepare_inputs.py'),str(chain),str(dfs),str(out)],check=True)
    rows=list(csv.DictReader(out.open()))
    included=[r for r in rows if r['included']=='true']
    excluded=[r for r in rows if r['included']=='false']
    assert len(included)==2 and len(excluded)==2
    weights=[float(r['weight']) for r in included]
    assert all(weight>0 for weight in weights)
    assert sorted(weights)[0]+sorted(weights)[1] == pytest.approx(2.0)
    assert all(r['weight']=='' for r in excluded)

def test_prepare_inputs_normalizes_each_effective_date_separately(tmp_path):
    chain=tmp_path/'chain.csv'; dfs=tmp_path/'dfs.csv'; out=tmp_path/'normalized.csv'
    chain.write_text(
        'Effective Date,Expiry Date,Last Trade Date,Option Type,Strike,Bid,Ask\n'
        '11-11-2024,11-18-2024,11-11-2024,Call,6000,100,110\n'
        '11-11-2024,11-18-2024,11-11-2024,Put,6000,100,110\n'
        '11-11-2024,11-18-2024,11-11-2024,Call,6100,45,55\n'
        '11-11-2024,11-18-2024,11-11-2024,Put,5900,45,55\n'
        '11-12-2024,11-12-2025,11-12-2024,Call,6000,100,110\n'
        '11-12-2024,11-12-2025,11-12-2024,Put,6000,100,110\n'
        '11-12-2024,11-12-2025,11-12-2024,Call,6100,45,55\n'
        '11-12-2024,11-12-2025,11-12-2024,Put,5900,45,55\n'
    )
    dfs.write_text('T,DF\n0,1\n1,0.98\n')
    subprocess.run([sys.executable,str(ROOT/'python/prepare_inputs.py'),str(chain),str(dfs),str(out)],check=True)
    rows=list(csv.DictReader(out.open()))
    included_by_date={}
    for row in rows:
        if row['included']=='true':
            included_by_date.setdefault(row['effective_date'], []).append(float(row['weight']))
    assert set(included_by_date)=={'2024-11-11','2024-11-12'}
    assert all(len(weights)==2 for weights in included_by_date.values())
    assert all(statistics.median(weights)==pytest.approx(1.0) for weights in included_by_date.values())

def test_every_contract_schema_has_a_valid_example():
    kinds={'normalized':'normalized_quotes','fitted':'fitted_quotes','grid':'surface_grid','parameters':'parameters','diagnostics':'diagnostics'}
    for kind,name in kinds.items():
        example=ROOT/'schemas'/f'{name}.example.csv'
        result=subprocess.run([sys.executable,str(ROOT/'python/validate_schema.py'),kind,str(example)],capture_output=True,text=True)
        assert result.returncode==0, f'{example.name}: {result.stderr}'

def test_fitted_schema_rejects_non_numeric_fields(tmp_path):
    fitted=tmp_path/'fitted.csv'
    fitted.write_text(
        'quote_id,time_to_expiry,strike,log_moneyness,fitted_total_variance,fitted_iv,status\n'
        'q000001,not-a-number,95,-0.0533036180111567,0.021800652117907,0.20967269972805,ok\n'
    )
    result=subprocess.run([sys.executable,str(ROOT/'python/validate_schema.py'),'fitted',str(fitted)],capture_output=True,text=True)
    assert result.returncode != 0
    assert 'time_to_expiry is not numeric' in result.stderr
