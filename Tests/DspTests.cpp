#include "VoiceDsp.h"
#include <cstdio>
#include <limits>
using namespace sgk::research;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"Failed line %d: %s\n",__LINE__,#x); return 1; } } while (0)
int main() {
    Envelope e;EnvelopeSettings s{.02,.1,.7,.2,.01};
    CHECK(e.prepare(s,44100));double last=0;
    for(int n=0;n<300;++n)last=e.next();
    e.noteOff();CHECK(e.next()<=last);
    e.noteOff(true);for(int n=0;n<2000;++n)e.next();CHECK(e.finished());
    s.attack=-1;CHECK(!e.prepare(s,44100));
    Filter a,b;CHECK(a.prepare(44100,2,.5,1));CHECK(b.prepare(44100,2,.5,1));
    CHECK(!a.configure(.5,std::numeric_limits<double>::quiet_NaN()));
    CHECK(!a.configure(2,.5));
    for(int n=0;n<10000;++n){
        const double input=.1*std::sin(n*.1);double x=0,y=0;
        CHECK(a.process(input,x));CHECK(b.process(input,y));CHECK(x==y);
    }
    double output=123;CHECK(!a.process(std::numeric_limits<double>::infinity(),output));CHECK(output==123);
    CHECK(!a.prepare(44100,9,.5,.1));
    CHECK(a.prepare(44100,0,.5,.1));CHECK(a.process(.125,output));CHECK(output==.125);
    std::puts("Native envelope/filter checks passed");return 0;
}
