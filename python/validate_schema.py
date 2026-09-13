#!/usr/bin/env python3
import argparse,csv,sys
SCHEMAS={
'normalized': ['quote_id','effective_date','expiry_date','time_to_expiry','forward','discount_factor','strike','option_type','bid_iv','ask_iv','mid_iv','included','filter_reason'],
'fitted': ['quote_id','time_to_expiry','strike','log_moneyness','fitted_total_variance','fitted_iv','status'],
'grid': ['time_to_expiry','forward','discount_factor','log_moneyness','strike','fitted_total_variance','fitted_iv','is_tail'],
'parameters': ['time_to_expiry','theta','psi','rho','phi','butterfly_safe','calendar_safe'],
'diagnostics': ['model','converged','objective','iterations','runtime_seconds','error']}
p=argparse.ArgumentParser();p.add_argument('kind',choices=SCHEMAS);p.add_argument('file');a=p.parse_args()
with open(a.file,newline='') as f:
 r=csv.reader(f); header=next(r)
 if header!=SCHEMAS[a.kind]: print(f'header mismatch: {header}',file=sys.stderr);sys.exit(1)
 rows=list(r)
 if not rows: print('empty CSV',file=sys.stderr);sys.exit(1)
print(f'{a.kind}: valid header, {len(rows)} rows')
