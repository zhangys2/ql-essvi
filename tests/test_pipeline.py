import csv, json, subprocess, sys
from pathlib import Path

ROOT=Path(__file__).parents[1]
def test_pipeline(tmp_path):
    chain=tmp_path/'chain.csv'; dfs=tmp_path/'dfs.csv'; out=tmp_path/'normalized.csv'
    chain.write_text('Effective Date,Expiry Date,Last Trade Date,Option Type,Strike,Bid,Ask\n11-11-2024,11-18-2024,11-01-2024,Call,6000,100,110\n11-11-2024,11-18-2024,11-01-2024,Put,6000,100,110\n')
    dfs.write_text('T,DF\n0,1\n1,0.98\n')
    subprocess.run([sys.executable,str(ROOT/'python/prepare_inputs.py'),str(chain),str(dfs),str(out)],check=True)
    rows=list(csv.DictReader(out.open())); assert len(rows)==2; assert all(r['quote_id'] for r in rows); assert all(r['filter_reason'] or r['included']=='true' for r in rows)

def test_every_contract_schema_has_a_valid_example():
    kinds={'normalized':'normalized_quotes','fitted':'fitted_quotes','grid':'surface_grid','parameters':'parameters','diagnostics':'diagnostics'}
    for kind,name in kinds.items():
        example=ROOT/'schemas'/f'{name}.example.csv'
        result=subprocess.run([sys.executable,str(ROOT/'python/validate_schema.py'),kind,str(example)],capture_output=True,text=True)
        assert result.returncode==0, f'{example.name}: {result.stderr}'
