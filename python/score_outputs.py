#!/usr/bin/env python3
"""Shared, dependency-free scorer for fitter CSV outputs."""
import argparse,csv,json,math
from collections import defaultdict

def f(row,key):
    try:
        x=float(row[key]); return x if math.isfinite(x) else None
    except (KeyError,TypeError,ValueError): return None

def black_normalized(f,k,t,w):
    if t <= 0 or w <= 0: return max(f-k, 0.0)
    s=math.sqrt(w); d1=(math.log(f/k)+.5*w)/s; d2=d1-s
    cdf=lambda x: .5*(1+math.erf(x/math.sqrt(2)))
    return f*cdf(d1)-k*cdf(d2)

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('quotes'); ap.add_argument('--fitted',required=True); ap.add_argument('--grid'); ap.add_argument('--output',required=True); args=ap.parse_args()
    rows=list(csv.DictReader(open(args.quotes,newline='')))
    fitted={r['quote_id']:r for r in csv.DictReader(open(args.fitted,newline=''))}
    for r in rows: r.update(fitted.get(r['quote_id'], {}))
    eligible=[r for r in rows if r.get('included','').lower()=='true' and f(r,'bid_iv') is not None and f(r,'ask_iv') is not None]
    scored=[r for r in eligible if f(r,'fitted_iv') is not None]
    inside=sum(f(r,'bid_iv')-1e-12<=f(r,'fitted_iv')<=f(r,'ask_iv')+1e-12 for r in scored)
    result={'quotes_total':len(rows),'quotes_included':len(eligible),'quotes_scored':len(scored),'bid_ask_containment':inside/len(scored) if scored else None,'butterfly_violations':None,'calendar_violations':None}
    if args.grid:
        grid=list(csv.DictReader(open(args.grid,newline='')))
        by_t=defaultdict(list)
        for r in grid:
            t=f(r,'time_to_expiry'); k=f(r,'log_moneyness'); w=f(r,'fitted_total_variance'); strike=f(r,'strike'); forward=f(r,'forward')
            if t is not None and k is not None and w is not None and strike is not None and forward is not None:
                by_t[t].append((k,w,strike,forward))
        butterfly=0
        for vals in by_t.values():
            vals.sort();
            for i in range(1,len(vals)-1):
                # Convexity of undiscounted call prices on the strike grid.
                c0=black_normalized(vals[i-1][3],vals[i-1][2],times[0] if False else 1.0,vals[i-1][1])
                c1=black_normalized(vals[i][3],vals[i][2],1.0,vals[i][1])
                c2=black_normalized(vals[i+1][3],vals[i+1][2],1.0,vals[i+1][1])
                s1=(c1-c0)/(vals[i][2]-vals[i-1][2]); s2=(c2-c1)/(vals[i+1][2]-vals[i][2])
                if s2 < s1-1e-8: butterfly+=1
        times=sorted(by_t)
        calendar=0
        common=sorted(set(k for t in times for k,_,_,_ in by_t[t]))
        for a,b in zip(times,times[1:]):
            da={k:w for k,w,_,_ in by_t[a]}; db={k:w for k,w,_,_ in by_t[b]}
            for k in common:
                if k in da and k in db and db[k] < da[k]-1e-8: calendar+=1
        result.update({'butterfly_violations':butterfly,'calendar_violations':calendar,'grid_points':sum(map(len,by_t.values()))})
    with open(args.output,'w') as out: json.dump(result,out,indent=2); out.write('\n')
if __name__=='__main__': main()
