#include "calibrator.hpp"
#include <nlopt.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace essvi {
namespace {
struct Context { std::vector<Quote> q; std::vector<double> expiries; };
std::vector<std::string> split(const std::string& s) { std::vector<std::string> r; std::stringstream x(s); std::string v; while(std::getline(x,v,',')) r.push_back(v); return r; }
double val(const std::string& s) { return s.empty()?NAN:std::stod(s); }
double sigmoid(double x) { return 1.0/(1.0+std::exp(-std::clamp(x,-40.0,40.0))); }
Slice slice(const std::vector<double>& x, size_t i, double t) {
  const double eta=std::exp(x[0]), lambda=x[1], a=x[2], b=x[3], theta=x[4+i];
  const double rho=-.98+1.96*sigmoid(a+b*std::log(std::max(t,1e-8)));
  return {t,theta,theta*eta*std::pow(theta,-lambda),rho};
}
double objective(unsigned n, const double* xp, double* grad, void* p) {
  auto& c=*static_cast<Context*>(p);
  auto value=[&](const double* y){ std::vector<double>x(y,y+n); double z=0; for(auto& q:c.q) if(q.included && std::isfinite(q.mid_iv)) { auto i=std::lower_bound(c.expiries.begin(),c.expiries.end(),q.expiry)-c.expiries.begin(); auto s=slice(x,i,q.expiry); double d=sliceTotalVariance(s,std::log(q.strike/q.forward))-q.expiry*q.mid_iv*q.mid_iv; z+=d*d; } return z; };
  const double z=value(xp); if(grad) for(unsigned i=0;i<n;++i){ std::vector<double> y(xp,xp+n); double h=1e-5*std::max(1.0,std::abs(xp[i])); y[i]+=h; grad[i]=(value(y.data())-z)/h; } return z;
}
void constraints(unsigned, double* r, unsigned, const double* xp, double*, void* p) {
  auto& c=*static_cast<Context*>(p); std::vector<double>x(xp,xp+4+c.expiries.size()); unsigned j=0; constexpr double e=1e-8;
  std::vector<Slice>s; for(size_t i=0;i<c.expiries.size();++i)s.push_back(slice(x,i,c.expiries[i]));
  for(size_t i=0;i<s.size();++i){auto&v=s[i];r[j++]=e-v.theta;r[j++]=e-v.psi;r[j++]=v.psi*(1+std::abs(v.rho))-4+e;r[j++]=v.psi*v.psi*(1+std::abs(v.rho))-4*v.theta+e;}
  for(size_t i=1;i<s.size();++i){auto&a=s[i-1];auto&b=s[i];r[j++]=a.theta-b.theta+e;r[j++]=a.psi-b.psi+e;r[j++]=a.psi*std::max((1+a.rho)/(1+b.rho),(1-a.rho)/(1-b.rho))-b.psi+e;}
}
}
std::vector<Quote> readQuotes(const std::string& path) {
 std::ifstream f(path); std::string line; std::getline(f,line); auto h=split(line); std::unordered_map<std::string,size_t> ix; for(size_t i=0;i<h.size();++i)ix[h[i]]=i;
 std::vector<Quote> out; while(std::getline(f,line)){auto v=split(line);if(v.size()<h.size())continue;try{out.push_back({v[ix["quote_id"]],val(v[ix["time_to_expiry"]]),val(v[ix["forward"]]),val(v[ix["discount_factor"]]),val(v[ix["strike"]]),val(v[ix["mid_iv"]]),v[ix["included"]]=="true"});}catch(...){}}
 return out;
}
CalibrationResult calibrate(const std::vector<Quote>& quotes) {
 Context c{quotes,{}};for(auto&q:quotes)if(q.included&&std::isfinite(q.expiry))c.expiries.push_back(q.expiry);std::sort(c.expiries.begin(),c.expiries.end());c.expiries.erase(std::unique(c.expiries.begin(),c.expiries.end()),c.expiries.end());CalibrationResult out;if(c.expiries.empty()){out.error="no included quotes";return out;}
 const unsigned n=4+c.expiries.size(),m=4*c.expiries.size()+3*(c.expiries.size()-1);std::vector<double>x(n),lo(n),hi(n);x[0]=std::log(.3);x[1]=.25;x[2]=0;x[3]=0;lo[0]=std::log(1e-4);hi[0]=std::log(10);lo[1]=0;hi[1]=.5;lo[2]=-5;hi[2]=5;lo[3]=-2;hi[3]=2;for(size_t i=0;i<c.expiries.size();++i){x[4+i]=.002+.04*c.expiries[i];lo[4+i]=1e-6;hi[4+i]=5;}
 auto started=std::chrono::steady_clock::now(); nlopt_opt opt=nlopt_create(NLOPT_LD_SLSQP,n);nlopt_set_lower_bounds(opt,lo.data());nlopt_set_upper_bounds(opt,hi.data());nlopt_set_min_objective(opt,objective,&c);std::vector<double>tol(m,1e-8);nlopt_add_inequality_mconstraint(opt,m,constraints,&c,tol.data());nlopt_set_maxeval(opt,3000);nlopt_set_ftol_rel(opt,1e-10);double minf=0;auto rc=nlopt_optimize(opt,x.data(),&minf);out.iterations=nlopt_get_numevals(opt);nlopt_destroy(opt);out.objective=minf;out.runtime_seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();for(size_t i=0;i<c.expiries.size();++i)out.surface.slices.push_back(slice(x,i,c.expiries[i]));out.error=validate(out.surface);out.converged=out.error.empty()&&(rc>0||rc==NLOPT_ROUNDOFF_LIMITED);if(!out.converged&&out.error.empty())out.error=nlopt_result_to_string(rc);return out;
}
void writeParameters(const CalibrationResult&r,const std::string&p){std::ofstream f(p);f<<"time_to_expiry,theta,psi,rho,phi,butterfly_safe,calendar_safe\n";for(size_t i=0;i<r.surface.slices.size();++i){auto&s=r.surface.slices[i];f<<s.expiry<<','<<s.theta<<','<<s.psi<<','<<s.rho<<','<<s.psi/s.theta<<','<<(butterflySafe(s)?"true":"false")<<','<<(!i||calendarSafe(r.surface.slices[i-1],s)?"true":"false")<<'\n';}}
void writeDiagnostics(const CalibrationResult&r,const std::string&p){std::ofstream f(p);f<<"model,converged,objective,iterations,runtime_seconds,error\n";f<<"eSSVI,"<<(r.converged?"true":"false")<<','<<r.objective<<','<<r.iterations<<','<<r.runtime_seconds<<','<<r.error<<'\n';}
}
