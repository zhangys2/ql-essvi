#include "calibrator.hpp"
#include <nlopt.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace essvi {
namespace {
struct Context { std::vector<Quote> q; std::vector<double> expiries; };
std::vector<std::string> split(const std::string& s) { std::vector<std::string> r; std::stringstream x(s); std::string v; while(std::getline(x,v,',')) r.push_back(v); return r; }
double val(const std::string& s) { return s.empty()?NAN:std::stod(s); }
double objective(unsigned, const double* x, double*, void* p) {
  auto& c=*static_cast<Context*>(p); double z=0;
  for(auto& q:c.q) if(q.included && std::isfinite(q.mid_iv)) {
    auto i=std::lower_bound(c.expiries.begin(),c.expiries.end(),q.expiry)-c.expiries.begin();
    Slice s{q.expiry,x[3*i],x[3*i+1],x[3*i+2]}; double d=sliceTotalVariance(s,std::log(q.strike/q.forward));
    z += (d-q.expiry*q.mid_iv*q.mid_iv)*(d-q.expiry*q.mid_iv*q.mid_iv);
  } return z;
}
void constraints(unsigned m, double* r, unsigned n, const double* x, double*, void* p) {
  unsigned j=0; constexpr double e=1e-8; (void)p;
  for(unsigned i=0;i<n/3;i++) { auto t=x[3*i], psi=x[3*i+1], rho=x[3*i+2]; r[j++]=e-t; r[j++]=e-psi; r[j++]=std::abs(rho)-1+e; r[j++]=psi*(1+std::abs(rho))-4+e; r[j++]=psi*psi*(1+std::abs(rho))-4*t+e; if(i) { auto pt=x[3*i-3], pp=x[3*i-2], pr=x[3*i-1]; r[j++]=pt-t+e; r[j++]=pp-psi+e; r[j++]=pp*std::max((1+pr)/(1+rho),(1-pr)/(1-rho))-psi+e; } }
  (void)m;
}
}
std::vector<Quote> readQuotes(const std::string& path) {
 std::ifstream f(path); std::string line; std::getline(f,line); auto h=split(line); std::unordered_map<std::string,size_t> ix; for(size_t i=0;i<h.size();++i) ix[h[i]]=i;
 std::vector<Quote> out; while(std::getline(f,line)){auto v=split(line); if(v.size()<h.size()) continue; try { out.push_back({v[ix["quote_id"]],val(v[ix["time_to_expiry"]]),val(v[ix["forward"]]),val(v[ix["strike"]]),val(v[ix["mid_iv"]]),v[ix["included"]]=="true"}); } catch(...) {} } return out;
}
CalibrationResult calibrate(const std::vector<Quote>& quotes) {
 Context c{quotes,{}}; for(auto&q:quotes) if(q.included && std::isfinite(q.expiry)) c.expiries.push_back(q.expiry); std::sort(c.expiries.begin(),c.expiries.end()); c.expiries.erase(std::unique(c.expiries.begin(),c.expiries.end()),c.expiries.end());
 CalibrationResult out; if(c.expiries.empty()){out.error="no included quotes";return out;} unsigned n=3*c.expiries.size(), m=5*c.expiries.size()+3*(c.expiries.size()-1); std::vector<double>x(n),lo(n),hi(n); for(size_t i=0;i<c.expiries.size();++i){x[3*i]=.002+c.expiries[i]*.04;x[3*i+1]=.02+c.expiries[i]*.03;x[3*i+2]=-.4+0.002*i;lo[3*i]=1e-6;hi[3*i]=5;lo[3*i+1]=1e-6;hi[3*i+1]=3.9;lo[3*i+2]=-.98;hi[3*i+2]=.98;}
 nlopt_opt opt=nlopt_create(NLOPT_LD_SLSQP,n); nlopt_set_lower_bounds(opt,lo.data()); nlopt_set_upper_bounds(opt,hi.data()); nlopt_set_min_objective(opt,objective,&c); std::vector<double>tol(m,1e-8); nlopt_add_inequality_mconstraint(opt,m,constraints,&c,tol.data()); nlopt_set_maxeval(opt,2000); nlopt_set_ftol_rel(opt,1e-10); double minf=0; auto rc=nlopt_optimize(opt,x.data(),&minf); nlopt_destroy(opt); out.objective=minf; for(size_t i=0;i<c.expiries.size();++i) out.surface.slices.push_back({c.expiries[i],x[3*i],x[3*i+1],x[3*i+2]}); out.error=validate(out.surface); out.converged=out.error.empty() && (rc>0 || rc==NLOPT_ROUNDOFF_LIMITED); if(!out.converged && out.error.empty()) out.error=nlopt_result_to_string(rc); return out;
}
void writeParameters(const CalibrationResult&r,const std::string&p){std::ofstream f(p);f<<"time_to_expiry,theta,psi,rho,phi\n";for(auto&s:r.surface.slices)f<<s.expiry<<','<<s.theta<<','<<s.psi<<','<<s.rho<<','<<s.psi/s.theta<<'\n';}
}
