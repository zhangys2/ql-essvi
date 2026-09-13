#!/usr/bin/env python3
import argparse,csv,math
p=argparse.ArgumentParser();p.add_argument('normalized');p.add_argument('cvi_grid');p.add_argument('output');a=p.parse_args()
meta={};lo={};hi={}
for r in csv.DictReader(open(a.normalized,newline='')):
 t=r['time_to_expiry'];meta[t]=(r['forward'],r['discount_factor'])
 if r['included']=='true':
  k=float(r['strike'])/float(r['forward']); k=math.log(k);lo[t]=min(lo.get(t,k),k);hi[t]=max(hi.get(t,k),k)
with open(a.output,'w',newline='') as f:
 fields=['time_to_expiry','forward','discount_factor','log_moneyness','strike','fitted_total_variance','fitted_iv','is_tail'];w=csv.DictWriter(f,fieldnames=fields);w.writeheader()
 for r in csv.DictReader(open(a.cvi_grid,newline='')):
  t=r['time_to_expiry'];fwd,df=meta.get(t,(r['forward'],r['discount_factor']));k=float(r['log_moneyness']);r.update(forward=fwd,discount_factor=df,is_tail='true' if k<lo.get(t,k) or k>hi.get(t,k) else 'false');w.writerow({x:r.get(x,'') for x in fields})
