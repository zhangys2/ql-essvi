#!/usr/bin/env python3
"""Adapt additive CVI notebook output to the strict shared fitted-quotes schema."""
import argparse,csv

def num(x):
    try:return float(x)
    except (TypeError,ValueError):return None
p=argparse.ArgumentParser();p.add_argument('normalized');p.add_argument('cvi');p.add_argument('output');a=p.parse_args()
fit={}
for r in csv.DictReader(open(a.cvi,newline='')):
    key=(round(num(r['time_to_expiry']),6),round(num(r['strike']),8),r.get('option_type',r.get('Option Type','')).upper()[:1])
    fit[key]=r
with open(a.output,'w',newline='') as f:
    fields=['quote_id','time_to_expiry','strike','log_moneyness','fitted_total_variance','fitted_iv','status']
    w=csv.DictWriter(f,fieldnames=fields);w.writeheader()
    for q in csv.DictReader(open(a.normalized,newline='')):
        key=(round(num(q['time_to_expiry']),6),round(num(q['strike']),8),q['option_type'].upper()[:1]);r=fit.get(key)
        w.writerow({'quote_id':q['quote_id'],'time_to_expiry':q['time_to_expiry'],'strike':q['strike'],'log_moneyness':q.get('log_moneyness',''),'fitted_total_variance':r.get('fitted_total_variance','') if r else '','fitted_iv':r.get('fitted_iv','') if r else '','status':'ok' if r and q['included']=='true' else ('excluded' if q['included']!='true' else 'not_fitted')})
