#!/usr/bin/env python3
"""Normalize option_chain.csv into the shared contract using only the stdlib."""
import argparse, csv, datetime as dt, math
from collections import defaultdict

def parse_date(value):
    for fmt in ('%Y-%m-%d', '%m-%d-%Y', '%m/%d/%Y'):
        try: return dt.datetime.strptime(value.strip(), fmt).date()
        except ValueError: pass
    raise ValueError(f'unsupported date: {value}')
def yearfrac(a,b): return (b-a).days/365.0

def norm_cdf(x): return 0.5*(1.0+math.erf(x/math.sqrt(2.0)))
def black(f,k,t,df,vol,call):
    if vol <= 0 or t <= 0: return df*max((f-k) if call else (k-f),0.0)
    s=vol*math.sqrt(t); d1=(math.log(f/k)+.5*s*s)/s; d2=d1-s
    return df*((f*norm_cdf(d1)-k*norm_cdf(d2)) if call else (k*norm_cdf(-d2)-f*norm_cdf(-d1)))
def implied(price,f,k,t,df,call):
    intrinsic=df*max((f-k) if call else (k-f),0.0)
    upper=df*(f if call else k)
    if not math.isfinite(price) or price <= intrinsic+1e-10 or price >= upper: return None
    lo,hi=1e-8,5.0
    for _ in range(100):
        mid=(lo+hi)/2
        if black(f,k,t,df,mid,call)>price: hi=mid
        else: lo=mid
    return (lo+hi)/2

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('chain'); ap.add_argument('dfs'); ap.add_argument('output'); args=ap.parse_args()
    with open(args.dfs,newline='') as f: dfs=[(float(r['T']),float(r['DF'])) for r in csv.DictReader(f)]
    def discount(t):
        if t<=dfs[0][0]: return dfs[0][1]
        for (a,da),(b,db) in zip(dfs,dfs[1:]):
            if t<=b: return da+(db-da)*(t-a)/(b-a)
        return dfs[-1][1]
    raw=[]
    with open(args.chain,newline='') as f:
        for n,r in enumerate(csv.DictReader(f),1):
            raw.append(r|{'_n':str(n)})
    grouped=defaultdict(list)
    for r in raw: grouped[(r['Effective Date'],r['Expiry Date'],r['Strike'])].append(r)
    forwards={}
    for (ed,xd,sk),rows in grouped.items():
        calls=[r for r in rows if r['Option Type'].upper().startswith('C')]
        puts=[r for r in rows if r['Option Type'].upper().startswith('P')]
        if calls and puts:
            try:
                t=yearfrac(parse_date(ed),parse_date(xd)); df=discount(t); k=float(sk)
                c=float(calls[0]['Mid'] if 'Mid' in calls[0] else (float(calls[0]['Bid'])+float(calls[0]['Ask']))/2)
                p=float(puts[0]['Mid'] if 'Mid' in puts[0] else (float(puts[0]['Bid'])+float(puts[0]['Ask']))/2)
                forwards[(ed,xd)]=forwards.get((ed,xd),[])+[k+(c-p)/df]
            except (ValueError,KeyError,ZeroDivisionError): pass
    out=[]
    for r in raw:
        ed0,xd0=r['Effective Date'],r['Expiry Date']; ed=parse_date(ed0).isoformat(); xd=parse_date(xd0).isoformat(); t=yearfrac(parse_date(ed0),parse_date(xd0)); df=discount(t)
        last_trade=parse_date(r['Last Trade Date'])
        fwd=sum(forwards.get((ed0,xd0),[float(r['Strike'])]))/len(forwards.get((ed0,xd0),[float(r['Strike'])]))
        try: strike=float(r['Strike']); bid=float(r['Bid']); ask=float(r['Ask'])
        except ValueError: strike=bid=ask=float('nan')
        reason=''; included=True
        if not (t>0 and math.isfinite(strike) and math.isfinite(bid) and math.isfinite(ask) and bid>=0 and ask>=bid and ask>0): included=False; reason='invalid_or_crossed_quote'
        elif last_trade < parse_date(ed0)-dt.timedelta(days=7): included=False; reason='stale_quote'
        elif (r['Option Type'].upper().startswith('C') and strike <= fwd) or (r['Option Type'].upper().startswith('P') and strike >= fwd): included=False; reason='not_otm'
        biv=aiv=None
        if included:
            call=r['Option Type'].upper().startswith('C'); biv=implied(bid,fwd,strike,t,df,call); aiv=implied(ask,fwd,strike,t,df,call)
            if biv is None or aiv is None: included=False; reason='iv_inversion_failed'
        out.append({'quote_id':'q'+r['_n'].zfill(6),'effective_date':ed,'expiry_date':xd,'time_to_expiry':f'{t:.12g}','forward':f'{fwd:.12g}','discount_factor':f'{df:.12g}','strike':f'{strike:.12g}' if math.isfinite(strike) else '','option_type':r['Option Type'].upper()[0] if r['Option Type'] else '','bid_iv':f'{biv:.12g}' if biv is not None else '','ask_iv':f'{aiv:.12g}' if aiv is not None else '','mid_iv':f'{(biv+aiv)/2:.12g}' if biv is not None and aiv is not None else '','included':'true' if included else 'false','filter_reason':reason})
    fields=list(out[0]);
    with open(args.output,'w',newline='') as f: w=csv.DictWriter(f,fieldnames=fields); w.writeheader(); w.writerows(out)
if __name__=='__main__': main()
