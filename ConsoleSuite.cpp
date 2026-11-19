#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
using namespace juce;
using AP=AudioProcessorValueTreeState;
#ifndef BOB_MODE
#define BOB_MODE 0
#endif
namespace {
constexpr int mode=BOB_MODE,N=12;
struct Param{const char* name;float lo,hi,def;};
constexpr Param commonIn{"INPUT dB",-18,18,0},commonOut{"OUTPUT dB",-18,18,0};
constexpr Param spec[4][N]={
{commonIn,{"LOW CUT Hz",40,200,80},{"LEVEL",0,100,45},{"CATCH",0,100,20},{"DE-ESS",0,100,25},{"BODY dB",-6,6,1},{"PRESENCE dB",-6,6,1},{"AIR dB",-6,6,1},{"VALVE",0,100,20},{"MIX",0,100,100},{"DE-ESS Hz",4000,9000,6500},commonOut},
{commonIn,{"LOW CUT Hz",20,100,30},{"ATTACK",-100,100,20},{"SUSTAIN",-100,100,0},{"PUNCH",0,100,30},{"THUMP dB",-6,6,1},{"BOX dB",-6,6,-1},{"CRACK dB",-6,6,1},{"SMASH",0,100,25},{"MIX",0,100,100},{"RELEASE ms",40,300,110},commonOut},
{commonIn,{"LOW Hz",40,250,100},{"LOW dB",-9,9,0},{"MID Hz",250,5000,1000},{"MID dB",-9,9,0},{"MID Q",.3,3,.8},{"HIGH Hz",3000,16000,10000},{"HIGH dB",-9,9,0},{"COLOR",0,100,25},{"MIX",0,100,100},{"BIAS",-100,100,25},commonOut},
{commonIn,{"SC HPF Hz",30,180,80},{"GLUE",0,100,25},{"ATTACK ms",3,40,15},{"RELEASE ms",50,600,150},{"WEIGHT dB",-3,3,0},{"TONE dB",-3,3,0},{"AIR dB",-3,3,0},{"IRON",0,100,15},{"MIX",0,100,100},{"WIDTH",70,125,100},commonOut}};
constexpr const char* names[]={"WARCRY","THUNDERHEAD","BLACKIRON","CROWN"};
constexpr const char* subtitles[]={"VOCAL LEVELING CONSOLE","DRUM IMPACT CONSOLE","ANALOG COLOR EQUALIZER","STEREO BUS FINISHER"};
constexpr unsigned accents[]={0xffd8a6ab,0xffe4a052,0xffc9b585,0xffa7c8e8};
constexpr float presets[4][4][N]={
{{0,80,45,20,25,1,1,1,20,100,6500,0},{0,100,70,45,40,0,2,1,35,100,6500,0},{0,65,35,15,15,2,0,2,25,100,6000,0},{0,120,80,65,55,-1,2,0,45,65,7000,0}},
{{0,30,20,0,30,1,-1,1,25,100,110,0},{0,30,50,-25,45,2,-2,3,40,75,90,0},{0,40,-15,45,60,1,-1,0,65,40,180,0},{0,25,10,10,15,2,-1,1,15,100,150,0}},
{{0,100,1,1000,0,.8,10000,1,25,100,25,0},{0,80,2,350,-2,.8,8000,1,40,100,45,0},{0,120,-1,1800,2,1,12000,2,35,80,-20,0},{0,70,0,900,0,.7,12000,0,70,50,65,0}},
{{0,80,25,15,150,0,0,0,15,100,100,0},{0,90,40,20,100,1,0,1,25,100,105,0},{0,60,15,30,250,1,-.5,0,20,100,100,0},{0,100,55,10,100,0,1,0,35,65,110,0}}};
constexpr const char* presetNames[4][4]={{"Rock lead","Metal upfront","Ballad warmth","Parallel vocal"},{"Kit punch","Snare impact","Room destruction","Vintage kit"},{"Console polish","Warm and thick","Guitar articulation","Parallel color"},{"Mix glue","Rock master bus","Vintage weight","Parallel aggression"}};
String id(int i){return "p"+String(i);}
AP::ParameterLayout layout(){AP::ParameterLayout p;for(int i=0;i<N;++i){auto s=spec[mode][i];p.add(std::make_unique<AudioParameterFloat>(ParameterID{id(i),1},s.name,NormalisableRange<float>(s.lo,s.hi,.01f),s.def));}return p;}
float gain(float x){return std::exp(x*.11512925465f);}
float db(float x){return 8.685889638f*std::log(std::max(x,1e-8f));}
struct Biquad{
 float b0=1,b1=0,b2=0,a1=0,a2=0,z1=0,z2=0;
 void set(int type,float freq,float q,float gd,double sr){
 float w=6.283185307f*std::clamp(freq,10.f,float(sr*.45))/float(sr),c=std::cos(w),s=std::sin(w),a=s/(2*q),A=std::sqrt(gain(gd)),a0=1+a;
 if(type==0){b0=(1+c)*.5f;b1=-(1+c);b2=b0;a1=-2*c;a2=1-a;}
 else if(type==1){b0=a;b1=0;b2=-a;a1=-2*c;a2=1-a;}
 else if(type==2){b0=1+a*A;b1=-2*c;b2=1-a*A;a0=1+a/A;a1=-2*c;a2=1-a/A;}
 else {float t=2*std::sqrt(A)*a;if(type==3){b0=A*((A+1)-(A-1)*c+t);b1=2*A*((A-1)-(A+1)*c);b2=A*((A+1)-(A-1)*c-t);a0=(A+1)+(A-1)*c+t;a1=-2*((A-1)+(A+1)*c);a2=(A+1)+(A-1)*c-t;}else{b0=A*((A+1)+(A-1)*c+t);b1=-2*A*((A-1)+(A+1)*c);b2=A*((A+1)+(A-1)*c-t);a0=(A+1)-(A-1)*c+t;a1=2*((A-1)-(A+1)*c);a2=(A+1)-(A-1)*c-t;}}
 b0/=a0;b1/=a0;b2/=a0;a1/=a0;a2/=a0;
 }
 float tick(float x){float y=b0*x+z1;z1=b1*x-a1*y+z2;z2=b2*x-a2*y;return y;}
};
struct Ch{Biquad hp,low,mid,high,side;float dx=0,dy=0;};
struct Env{float value=0;float step(float x,float a,float r){float k=x>value?a:r;return value=k*value+(1-k)*x;}};
}
class Console:public AudioProcessor{
public:
 AP p{*this,nullptr,"BOB_CONSOLE",layout()};
 std::array<Ch,2> st;std::array<SmoothedValue<float>,N> smooth;
 std::unique_ptr<dsp::Oversampling<float>> os;
 AudioBuffer<float> dry;
 dsp::DelayLine<float,dsp::DelayLineInterpolationTypes::None> dryDelay{8192};
 Env slow,fast,band,grA,grB;double sr=44100;
 std::atomic<float> meterA{0},meterB{0},peak{0};
 Console():AudioProcessor(BusesProperties().withInput("Input",AudioChannelSet::stereo(),true).withOutput("Output",AudioChannelSet::stereo(),true)){}
 const String getName()const override{return names[mode];}
 bool acceptsMidi()const override{return false;}bool producesMidi()const override{return false;}bool isMidiEffect()const override{return false;}
 double getTailLengthSeconds()const override{return 0;}
 int getNumPrograms()override{return 4;}int current=0;int getCurrentProgram()override{return current;}
 const String getProgramName(int i)override{return i>=0&&i<4?String(presetNames[mode][i]):String();}
 void changeProgramName(int,const String&)override{}
 void setCurrentProgram(int i)override{if(i<0||i>=4)return;current=i;for(int j=0;j<N;++j)if(auto* x=p.getParameter(id(j)))x->setValueNotifyingHost(x->convertTo0to1(presets[mode][i][j]));}
 bool isBusesLayoutSupported(const BusesLayout& l)const override{auto c=l.getMainOutputChannelSet();return c==l.getMainInputChannelSet()&&(c==AudioChannelSet::mono()||c==AudioChannelSet::stereo());}
 void prepareToPlay(double s,int n)override{
 sr=s;auto ch=(uint32)getTotalNumOutputChannels();os=std::make_unique<dsp::Oversampling<float>>(ch,2,dsp::Oversampling<float>::filterHalfBandFIREquiripple,true,true);os->initProcessing((size_t)n);os->reset();int lat=roundToInt(os->getLatencyInSamples());setLatencySamples(lat);dryDelay.prepare({s,(uint32)n,ch});dryDelay.setDelay(float(lat));dryDelay.reset();dry.setSize((int)ch,n);st={};slow={};fast={};band={};grA={};grB={};
 for(int j=0;j<N;++j){smooth[j].reset(s,.025);smooth[j].setCurrentAndTargetValue(*p.getRawParameterValue(id(j)));}
 }
 void releaseResources()override{os.reset();}
 float coef(float ms)const{return std::exp(-1000.f/(float(sr)*ms));}
 void processBlock(AudioBuffer<float>& b,MidiBuffer&)override{
 ScopedNoDenormals nd;if(!os)return;int n=b.getNumSamples(),ch=b.getNumChannels();if(n>dry.getNumSamples()){b.clear();return;}
 float v[N];for(int j=0;j<N;++j){smooth[j].setTargetValue(*p.getRawParameterValue(id(j)));v[j]=smooth[j].skip(n);}
 float input=gain(v[0]),out=gain(v[11]),mix=v[9]*.01f;
 for(int c=0;c<ch;++c){auto& z=st[c];
 if constexpr(mode==0){z.hp.set(0,v[1],.707f,0,sr);z.low.set(3,180,.707f,v[5],sr);z.mid.set(2,2600,.8f,v[6],sr);z.high.set(4,11000,.707f,v[7],sr);z.side.set(1,v[10],1.6f,0,sr);}
 if constexpr(mode==1){z.hp.set(0,v[1],.707f,0,sr);z.low.set(2,75,.8f,v[5],sr);z.mid.set(2,420,1.f,v[6],sr);z.high.set(2,3300,.9f,v[7],sr);}
 if constexpr(mode==2){z.low.set(3,v[1],.707f,v[2],sr);z.mid.set(2,v[3],v[5],v[4],sr);z.high.set(4,v[6],.707f,v[7],sr);}
 if constexpr(mode==3){z.side.set(0,v[1],.707f,0,sr);z.low.set(3,100,.707f,v[5],sr);z.mid.set(2,1500,.7f,v[6],sr);z.high.set(4,10500,.707f,v[7],sr);}
 }
 float aFast=coef(1),rFast=coef(35),aSlow=coef(18),rSlow=coef(160),aBand=coef(1.5f),rBand=coef(65);
 float aA=coef(mode==3?v[3]:mode==0?12:mode==1?12:10),rA=coef(mode==3?v[4]:mode==1?v[10]:180),aB=coef(1.5f),rB=coef(70);
 for(int i=0;i<n;++i){float x[2]{},h[2]{},pk=0,bp=0;
 for(int c=0;c<ch;++c){auto& z=st[c];float raw=b.getSample(c,i);dryDelay.pushSample(c,raw);dry.setSample(c,i,dryDelay.popSample(c));float y=raw*input;if constexpr(mode<2)y=z.hp.tick(y);y=z.high.tick(z.mid.tick(z.low.tick(y)));x[c]=y;
 if constexpr(mode==0||mode==3)h[c]=z.side.tick(y);float detector=mode==3?h[c]:y;pk=std::max(pk,std::abs(detector));bp=std::max(bp,std::abs(h[c]));}
 float f=fast.step(pk,aFast,rFast),s=slow.step(pk,aSlow,rSlow),be=band.step(bp,aBand,rBand),ta=0,tb=0,shape=0;
 if constexpr(mode==0){ta=std::min(16.f,std::max(0.f,db(s)+28)*.75f)*v[2]*.01f;tb=std::min(10.f,std::max(0.f,db(f)+13)*.85f)*v[3]*.01f;}
 if constexpr(mode==1){float contrast=std::clamp(db(f)-db(s),-18.f,18.f);shape=v[2]*.01f*std::max(0.f,contrast)*.55f+v[3]*.01f*std::max(0.f,-contrast)*.55f;ta=std::min(14.f,std::max(0.f,db(s)+22)*.7f)*v[4]*.01f;}
 if constexpr(mode==3)ta=std::min(7.f,std::max(0.f,db(s)+22)*.4f)*v[2]*.01f;
 float ga=grA.step(ta,aA,rA),gb=grB.step(tb,aB,rB);
 float ess=mode==0?gain(-std::min(10.f,std::max(0.f,db(be)+32)*.7f)*v[4]*.01f):1;
 float makeup=mode==0?v[2]*.035f:mode==1?v[4]*.015f:mode==3?v[2]*.012f:0;
 float g=gain(-ga-gb+shape+makeup);for(int c=0;c<ch;++c)b.setSample(c,i,(x[c]+(ess-1)*h[c])*g);
 }
 meterA.store(grA.value);meterB.store(mode==0?grB.value:0);
 dsp::AudioBlock<float> block(b);auto high=os->processSamplesUp(block);
 float amount=v[8]*.01f,d=1+amount*(mode==1?10.f:mode==2?5.f:mode==0?4.f:3.f);
 float bias=amount*(mode==2?v[10]*.003f:mode==0?.27f:mode==1?.08f:.12f),t=std::tanh(bias),norm=d*(1-t*t);
 for(size_t c=0;c<high.getNumChannels();++c){auto* x=high.getChannelPointer(c);for(size_t i=0;i<high.getNumSamples();++i){float a=x[i];float y=(std::tanh(d*a+bias)-t)/norm;if constexpr(mode==1)y=.65f*y+.35f*std::atan(d*a)/d;if constexpr(mode==3)y=.85f*y+.15f*std::atan(d*a)/d;x[i]=a+amount*(y-a);}}
 os->processSamplesDown(block);
 float dc=std::exp(-6.2831853f*3/float(sr)),pk=0;
 for(int i=0;i<n;++i){float y[2]{};for(int c=0;c<ch;++c){auto& z=st[c];float a=b.getSample(c,i),w=a-z.dx+dc*z.dy;z.dx=a;z.dy=w;y[c]=dry.getSample(c,i)*(1-mix)+mix*w;}if constexpr(mode==3){if(ch==2){float m=(y[0]+y[1])*.5f,s=(y[0]-y[1])*.5f*v[10]*.01f;y[0]=m+s;y[1]=m-s;}}for(int c=0;c<ch;++c){float a=y[c]*out;if(!std::isfinite(a))a=0;b.setSample(c,i,a);pk=std::max(pk,std::abs(a));}}
 peak.store(pk);
 }
 void getStateInformation(MemoryBlock& b)override{auto v=p.copyState();v.setProperty("program",current,nullptr);auto x=v.createXml();copyXmlToBinary(*x,b);}
 void setStateInformation(const void* d,int n)override{auto x=getXmlFromBinary(d,n);if(x&&x->hasTagName(p.state.getType())){auto v=ValueTree::fromXml(*x);current=int(v.getProperty("program",0));p.replaceState(v);}}
 bool hasEditor()const override{return true;}AudioProcessorEditor* createEditor()override;
};
class Rack:public AudioProcessorEditor,private Timer{
 Console& o;std::array<Slider,N> knobs;std::array<Label,N> labels;std::array<std::unique_ptr<AP::SliderAttachment>,N> attachments;ComboBox preset;
 class Look:public LookAndFeel_V4{public:void drawRotarySlider(Graphics& g,int x,int y,int w,int h,float p,float a,float b,Slider&)override{float cx=x+w*.5f,cy=y+h*.5f,r=std::min(w,h)*.36f;Rectangle<float> q(cx-r,cy-r,2*r,2*r);g.setColour(Colour(0xff141417));g.fillEllipse(q.expanded(5));Path arc;arc.addCentredArc(cx,cy,r+5,r+5,0,a,a+(b-a)*p,true);g.setColour(Colour(accents[mode]));g.strokePath(arc,PathStrokeType(2.5f));g.setGradientFill(ColourGradient(Colour(0xff959699),cx,cy-r,Colour(0xff292a30),cx,cy+r,false));g.fillEllipse(q);float t=a+(b-a)*p;g.setColour(Colour(0xfff5eee2));g.drawLine(cx+std::sin(t)*r*.4f,cy-std::cos(t)*r*.4f,cx+std::sin(t)*r*.88f,cy-std::cos(t)*r*.88f,3);}}look;
public:
 Rack(Console& p):AudioProcessorEditor(p),o(p){setSize(850,420);setLookAndFeel(&look);for(int i=0;i<4;++i)preset.addItem(presetNames[mode][i],i+1);preset.setSelectedId(o.current+1,dontSendNotification);preset.onChange=[this]{if(preset.getSelectedId()>0)o.setCurrentProgram(preset.getSelectedId()-1);};addAndMakeVisible(preset);
 for(int i=0;i<N;++i){auto& s=knobs[i];s.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);s.setTextBoxStyle(Slider::TextBoxBelow,false,80,20);s.setColour(Slider::textBoxTextColourId,Colour(0xffeee6d8));s.setColour(Slider::textBoxBackgroundColourId,Colour(0xff141417));s.setColour(Slider::textBoxOutlineColourId,Colours::transparentBlack);addAndMakeVisible(s);labels[i].setText(spec[mode][i].name,dontSendNotification);labels[i].setJustificationType(Justification::centred);labels[i].setColour(Label::textColourId,Colour(0xffebe4d9));addAndMakeVisible(labels[i]);attachments[i]=std::make_unique<AP::SliderAttachment>(o.p,id(i),s);}startTimerHz(24);}
 ~Rack()override{stopTimer();setLookAndFeel(nullptr);}
 void timerCallback()override{repaint(445,15,375,75);}
 void paint(Graphics& g)override{g.fillAll(Colour(0xff141417));g.setGradientFill(ColourGradient(Colour(0xff414147),0,10,Colour(0xff24252b),0,410,false));g.fillRoundedRectangle(10,10,830,400,5);g.setColour(Colour(accents[mode]));g.setFont(FontOptions(37.f,Font::bold));g.drawText(names[mode],28,17,410,43,Justification::centredLeft);g.setFont(FontOptions(12.f));g.drawText(subtitles[mode],30,61,400,17,Justification::centredLeft);
 float a=o.meterA.load(),b=o.meterB.load(),p=20*std::log10(std::max(o.peak.load(),1e-8f));for(int j=0;j<2;++j){int x=455+j*180;g.setColour(Colour(0xff151519));g.fillRoundedRectangle(float(x),25,170,48,3);float val=j?std::clamp((p+48)/48,0.f,1.f):std::clamp((a+b)/18,0.f,1.f);for(int k=0;k<16;++k){g.setColour(k<val*16?Colour(accents[mode]):Colour(0xff424148));g.fillRect(x+6+k*10,33,7,13);}g.setColour(Colour(accents[mode]));g.drawText(j?"OUT "+String(p,1)+" dBFS":"GR "+String(a+b,1)+" dB",x,49,170,18,Justification::centred);}
 g.setColour(Colour(accents[mode]).withAlpha(.5f));g.drawHorizontalLine(113,26,824);g.drawHorizontalLine(260,26,824);}
 void resized()override{preset.setBounds(28,83,290,25);for(int i=0;i<N;++i){int x=28+(i%6)*133,y=i<6?120:267;labels[i].setBounds(x,y,125,22);knobs[i].setBounds(x,y+22,125,107);}}
};
AudioProcessorEditor* Console::createEditor(){return new Rack(*this);}
AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new Console();}
