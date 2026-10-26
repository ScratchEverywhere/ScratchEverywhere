"""Check streaming audio against direct interpolation with ASan/UBSan."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="audio-resampler-") as temporary:
    work = Path(temporary)
    source = work / "resampler.cpp"
    source.write_text(r'''
#include "audio_resampler.hpp"
#include <cassert>
#include <iostream>
#include <limits>

struct Source {
    std::vector<float> data;
    int channels, cursor=0, shortRead=0;
    Source(int frames,int ch,int fragment):channels(ch),shortRead(fragment) {
        for(int i=0;i<frames;++i) for(int c=0;c<ch;++c)
            data.push_back(float(.6*std::sin(i*.071+c*.8)+.1*std::cos(i*.133+c*.2)));
    }
    int read(float *out,int request) {
        int count=std::min(request,int(data.size()/channels)-cursor);
        if(shortRead)count=std::min(count,shortRead);
        std::copy_n(data.data()+cursor*channels,count*channels,out);
        cursor+=count;return count;
    }
    float at(double position,int channel) const {
        int i=int(position),n=int(data.size()/channels);
        float frac=float(position-i);
        return data[i*channels+channel]+(data[std::min(i+1,n-1)*channels+channel]-data[i*channels+channel])*frac;
    }
};
void near(float a,float b) { if(std::abs(a-b)>2e-5)std::cerr<<a<<" != "<<b<<"\n";assert(std::abs(a-b)<=2e-5); }
void run(int count,int channels,double step,int block,int fragment) {
    Source source(count,channels,fragment);AudioResampler state;std::vector<float> result;
    for(int call=0;;++call) {
        assert(call<100000);
        std::vector<float> output(block*2,.125f);
        int mixed=state.mixStereo(output.data(),block,channels,step,.5f,.25f,
                                   [&](float *out,int n){return source.read(out,n);});
        assert(0<=mixed && mixed<=block);
        for(int i=mixed*2;i<block*2;++i)assert(output[i]==.125f);
        result.insert(result.end(),output.begin(),output.begin()+mixed*2);
        if(!mixed)break;
    }
    int expected=int(std::ceil(count/step-1e-9));
    assert(std::abs(int(result.size()/2)-expected)<=1);
    if(step==1.)assert(int(result.size()/2)==count);
    assert(source.cursor==count);
    for(int i=0;i<std::min(expected,int(result.size()/2));++i) {
        double position=i*step;
        if(position>=count)break;
        near(result[2*i],.125f+source.at(position,0)*.5f);
        near(result[2*i+1],.125f+source.at(position,channels>1?1:0)*.25f);
    }
}
int main() {
    int cases=0;
    for(double step:{.5,1.,1.25,2.,3.75,22050./48000,44100./48000,8000./48000})
        for(int channels:{1,2})for(int block:{1,17,511,1024,4096})for(int fragment:{0,7}) {
            run(5003,channels,step,block,fragment);++cases;
        }
    for(int count:{0,1,3})for(int ch:{1,2})for(double step:{.5,1.,3.75})run(count,ch,step,1024,1);
    run(22050*60,1,22050./48000,1024,0);
    // A pitch change must retain lookahead and its fractional source position.
    Source source(5003,2,5);AudioResampler state;double position=0;
    for(double step:{.5,1.25,2.,.75}) {
        std::vector<float> out(200,0);
        assert(state.mixStereo(out.data(),100,2,step,1,1,[&](float *p,int n){return source.read(p,n);})==100);
        for(int i=0;i<100;++i)for(int c=0;c<2;++c)near(out[2*i+c],source.at(position+i*step,c));
        position+=100*step;
    }
    // Reinitializing a stream must not retain samples or EOF from a prior play.
    state.reset();source.cursor=0;float out[2]={0,0};
    assert(state.mixStereo(out,1,2,1,1,1,[&](float *p,int n){return source.read(p,n);})==1);
    near(out[0],source.data[0]);near(out[1],source.data[1]);
    for(double bad:{0.,-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
        bool read=false;
        assert(state.mixStereo(out,1,2,bad,1,1,[&](float *,int){read=true;return 0;})==0);
        assert(!read);
    }
    std::cout<<cases<<" streaming rate/channel/block/short-read cases, EOF, gains, pitch changes and reset passed.\n";
}
''')
    exe = work / "resampler"
    subprocess.run(["clang++", "-std=c++17", "-fsanitize=address,undefined",
                    "-I", str(root / "include"), str(source), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
