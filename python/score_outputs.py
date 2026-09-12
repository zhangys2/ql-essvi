#!/usr/bin/env python3
"""Shared, dependency-free scorer for fitter CSV outputs."""
import argparse,csv,json,math
from collections import defaultdict

def f(row,key):
    try: return float(row[key])
    except (KeyError,TypeError,ValueError): return None

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('quotes'); ap.add_argument('--grid'); ap.add_argument('--output',required=True); args=ap.parse_args()
    rows=list(csv.DictReader(open(args.quotes,newline='')))
    eligible=[r for r in rows if r.get('included','').lower()=='true' and f(r,'bid_iv') is not None and f(r,'ask_iv') is not None]
    scored=[r for r in eligible if f(r,'fitted_iv') is not None]
    inside=sum(f(r,'bid_iv')-1e-12<=f(r,'fitted_iv')<=f(r,'ask_iv')+1e-12 for r in scored)
    result={'quotes_total':len(rows),'quotes_included':len(eligible),'quotes_scored':len(scored),'bid_ask_containment':inside/len(scored) if scored else None,'butterfly_violations':None,'calendar_violations':None}
    if args.grid:
        grid=list(csv.DictReader(open(args.grid,newline='')))
        by_t=defaultdict(list)
        for r in grid:
            t=f(r,'time_to_expiry'); k=f(r,'log_moneyness'); w=f(r,'fitted_total_variance')
            if t is not None and k is not None and w is not None: by_t[t].append((k,w))
        butterfly=0
        for vals in by_t.values():
            vals.sort();
            for i in range(1,len(vals)-1):
                # Convexity of call price is approximated by convexity of total variance only
                # as a cheap diagnostic; production output should also include prices.
                if vals[i][1] > vals[i-1][1] + (vals[i+1][1]-vals[i-1][1])*(vals[i][0]-vals[i-1][0])/(vals[i+1][0]-vals[i-1][0]) + 1e-8: butterfly+=1
        times=sorted(by_t)
        calendar=0
        common=sorted(set(k for t in times for k,_ in by_t[t]))
        for a,b in zip(times,times[1:]):
            da=dict(by_t[a]); db=dict(by_t[b])
            for k in common:
                if k in da and k in db and db[k] < da[k]-1e-8: calendar+=1
        result.update({'butterfly_violations':butterfly,'calendar_violations':calendar,'grid_points':sum(map(len,by_t.values()))})
    with open(args.output,'w') as out: json.dump(result,out,indent=2); out.write('\n')
if __name__=='__main__': main()
