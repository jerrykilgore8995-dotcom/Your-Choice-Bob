#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include <cmath>
#include "AfterglowPresets.h"
using namespace juce;
using AP=AudioProcessorValueTreeState;
namespace {
constexpr int N=19;
constexpr float pi=3.14159265358979323846f;
const std::array<const char*,N> ids={"mod","rate","depth","modmix","regen","left","right","feedback","delaymix","verb","decay","predelay","verbmix","hp","lp","color","width","mix","sync"};
const std::array<const char*,N> titles={"MOD TYPE","RATE Hz","DEPTH","MOD LEVEL","REGEN","LEFT ms","RIGHT ms","REPEATS","DELAY LEVEL","VERB TYPE","DECAY s","PREDELAY ms","VERB LEVEL","LOW CUT Hz","HIGH CUT Hz","COLOR","WIDTH","MASTER MIX","TEMPO SYNC"};
const std::array<float,N> lo={0,.05,0,0,0,10,10,0,0,0,.2,0,0,20,1500,0,0,0,0};
const std::array<float,N> hi={7,8,100,100,80,2000,2000,85,100,5,10,200,100,600,19000,100,150,100,1};
const std::array<float,N> defaults=glowPresets[0].v;
AP::ParameterLayout layout(){
 AP::ParameterLayout p;
 for(int i=0;i<N;++i){
 if(i==0)p.add(std::make_unique<AudioParameterChoice>(ParameterID{ids[i],1},titles[i],StringArray{"Off","Analog chorus","Symphonic","Phaser","Flanger","Rotary","Tremolo","Dimension"},int(defaults[i])));
 else if(i==9)p.add(std::make_unique<AudioParameterChoice>(ParameterID{ids[i],1},titles[i],StringArray{"Off","Plate","Hall","Spring","Room","Gated"},int(defaults[i])));
 else if(i==18)p.add(std::make_unique<AudioParameterBool>(ParameterID{ids[i],1},titles[i],false));
 else p.add(std::make_unique<AudioParameterFloat>(ParameterID{ids[i],1},titles[i],NormalisableRange<float>(lo[i],hi[i],i==1||i==10?.01f: .1f),defaults[i]));
 }
 p.add(std::make_unique<AudioParameterBool>(ParameterID{"aux",1},"Aux wet only",false));
 p.add(std::make_unique<AudioParameterFloat>(ParameterID{"output",1},"Output dB",NormalisableRange<float>(-18,12,.1f),0));
 return p;
}
float gain(float d){return std::exp(d*.11512925465f);}
float sat(float x,float c){float d=1+c*2.3f;return x+c*((std::tanh(x*d)/d)-x);}
struct Ring{
 std::vector<float> a;int w=0;
 void prepare(int size){a.assign(size,0);w=0;}
 float read(float n)const{n=std::clamp(n,1.f,float(a.size()-2));float t=float(w)-n;if(t<0)t+=float(a.size());int i=int(t),j=(i+1)%int(a.size());return a[i]+(t-i)*(a[j]-a[i]);}
 void write(float x){a[w]=x;if(++w==int(a.size()))w=0;}
};
struct Filter{float z=0;float lp(float x,float a){z+=a*(x-z);return z;}};
struct Allpass{float z=0;float tick(float x,float a){float y=a*x+z;z=x-a*y;return y;}};
struct Channel{
 Ring mod,delay,pre;
 std::array<Allpass,6> ap;
 Filter hp,lp,echoLP,echoHP,rotor;
 float regen=0,echo=0;
 void prepare(double sr){mod.prepare(int(sr*.12)+8);delay.prepare(int(sr*8)+8);pre.prepare(int(sr*.25)+8);ap={};hp={};lp={};echoLP={};echoHP={};rotor={};regen=echo=0;}
};
struct Verb{
 std::array<Ring,8> line;
 std::array<Filter,8> damp;
 float gate=0;
 double sampleRate=44100;
 const std::array<float,8> times={29.7f,37.1f,41.9f,47.3f,53.9f,61.7f,67.9f,73.1f};
 void prepare(double sr){sampleRate=sr;for(int i=0;i<8;++i){line[i].prepare(int(sr*.18)+8);damp[i]={};}gate=0;}
 void tick(float l,float r,int type,float decay,float brightness,float& outL,float& outR){
 float scale=type==1?.72f:type==2?1.35f:type==3?.85f:type==4?.43f:.82f;
 float d=0.04f+brightness*.36f,read[8]{},filtered[8]{},sum=0;
 for(int i=0;i<8;++i){read[i]=line[i].read(times[i]*scale*.001f*float(sampleRate));filtered[i]=damp[i].lp(read[i],d);sum+=filtered[i];}
 float mid=(l+r)*.5f,side=(l-r)*.5f;
 for(int i=0;i<8;++i){float fb=std::pow(.001f,times[i]*scale*.001f/std::max(.2f,decay));float input=(i%2? -mid:mid)+(i%4<2?side:-side)*.6f;if(type==0)input=0;line[i].write(input*.24f+fb*(filtered[i]-sum*.25f));}
 outL=(read[0]+read[2]-read[4]+read[6])*.42f;outR=(read[1]-read[3]+read[5]+read[7])*.42f;
 if(type==3){outL=.8f*outL+.2f*read[4];outR=.8f*outR-.2f*read[5];}
 float key=std::max(std::abs(l),std::abs(r));gate=std::max(key,gate*.9995f);
 if(type==5){float g=std::clamp(gate*45.f,0.f,1.f);outL*=g;outR*=g;}if(type==0)outL=outR=0;
 }
};
}
class Afterglow:public AudioProcessor{
public:
 AP p{*this,nullptr,"AFTERGLOW",layout()};
 std::array<Channel,2> state;
 Verb verb;
 std::array<SmoothedValue<float>,N> smoothing;
 SmoothedValue<float> output,aux;
 double sr=44100;float phase=0,delayL=375,delayR=500;
 std::atomic<float> peak{0};
 Afterglow():AudioProcessor(BusesProperties().withInput("Input",AudioChannelSet::stereo(),true).withOutput("Output",AudioChannelSet::stereo(),true)){}
 const String getName()const override{return "AFTERGLOW";}
 bool acceptsMidi()const override{return false;}bool producesMidi()const override{return false;}bool isMidiEffect()const override{return false;}
 double getTailLengthSeconds()const override{return 120;}int getNumPrograms()override{return int(std::size(glowPresets));}int currentProgram=0;int getCurrentProgram()override{return currentProgram;}
 void setCurrentProgram(int x)override{if(x<0||x>=getNumPrograms())return;currentProgram=x;for(int i=0;i<N;++i)if(auto* par=p.getParameter(ids[i]))par->setValueNotifyingHost(par->convertTo0to1(glowPresets[x].v[i]));}
 const String getProgramName(int x)override{return x>=0&&x<getNumPrograms()?String(glowPresets[x].name):String();}void changeProgramName(int,const String&)override{}
 bool isBusesLayoutSupported(const BusesLayout& l)const override{auto i=l.getMainInputChannelSet(),o=l.getMainOutputChannelSet();return (i==AudioChannelSet::mono()||i==AudioChannelSet::stereo())&&(o==AudioChannelSet::stereo()||(i==AudioChannelSet::mono()&&o==AudioChannelSet::mono()));}
 void prepareToPlay(double s,int)override{sr=s;for(auto& c:state)c.prepare(s);verb.prepare(s);phase=0;delayL=*p.getRawParameterValue("left");delayR=*p.getRawParameterValue("right");for(int j=0;j<N;++j){smoothing[j].reset(s,.035);smoothing[j].setCurrentAndTargetValue(*p.getRawParameterValue(ids[j]));}output.reset(s,.025);output.setCurrentAndTargetValue(*p.getRawParameterValue("output"));aux.reset(s,.025);aux.setCurrentAndTargetValue(*p.getRawParameterValue("aux"));}
 void releaseResources()override{}
 void processBlock(AudioBuffer<float>& b,MidiBuffer&)override{
 ScopedNoDenormals nd;int n=b.getNumSamples(),ch=getTotalNumOutputChannels(),inCh=getTotalNumInputChannels();
 for(int j=0;j<N;++j)smoothing[j].setTargetValue(*p.getRawParameterValue(ids[j]));output.setTargetValue(*p.getRawParameterValue("output"));aux.setTargetValue(*p.getRawParameterValue("aux"));
 float bpm=120;if(auto* ph=getPlayHead())if(auto pos=ph->getPosition())if(auto t=pos->getBpm())bpm=float(std::clamp(*t,30.,300.));
 int mod=int(*p.getRawParameterValue("mod")),reverb=int(*p.getRawParameterValue("verb"));bool sync=*p.getRawParameterValue("sync")>.5f;
 float v[N];for(int j=0;j<N;++j)v[j]=smoothing[j].skip(n);
 float smpl=float(sr),rate=v[1],depth=v[2]*.01f,mm=v[3]*.01f,regen=v[4]*.01f,fb=v[7]*.01f,dm=v[8]*.01f,rm=v[12]*.01f,color=v[15]*.01f,width=v[16]*.01f,mix=v[17]*.01f;
 float delayTargetL=v[5]*(sync?120.f/bpm:1.f),delayTargetR=v[6]*(sync?120.f/bpm:1.f);
 float hp=1-std::exp(-2*pi*v[13]/smpl),lp=1-std::exp(-2*pi*std::min(v[14],smpl*.45f)/smpl),echoLP=1-std::exp(-2*pi*(10000.f-color*6500.f)/smpl),echoHP=1-std::exp(-2*pi*100.f/smpl);
 float maxPeak=0;
 for(int i=0;i<n;++i){
 float dry[2]={b.getSample(0,i),b.getSample(inCh>1?1:0,i)},modOut[2]{},echo[2]{},verbIn[2]{},wet[2]{};
 phase+=rate/smpl;if(phase>=1)phase-=1;
 float timeSmooth=1-std::exp(-1.f/(smpl*.045f));delayL+=timeSmooth*(delayTargetL-delayL);delayR+=timeSmooth*(delayTargetR-delayR);
 for(int c=0;c<2;++c){
 auto& z=state[c];float x=dry[c],p0=2*pi*(phase+(c?.25f:0)),lfo=std::sin(p0);float y=x;
 if(mod==1||mod==2||mod==4||mod==7){
 float base=mod==4?2.2f:mod==7?14.f:19.f;
 float span=mod==4?1.8f:mod==7?1.4f:6.5f;
 y=z.mod.read((base+depth*span*lfo)*.001f*smpl);
 if(mod==2){float yy=z.mod.read((24.f+4.5f*depth*std::sin(p0+2.094395f))*.001f*smpl);float zz=z.mod.read((32.f+5.f*depth*std::sin(p0+4.18879f))*.001f*smpl);y=(y+yy+zz)/3.f;}
 if(mod==7){float yy=z.mod.read((23.f+depth*1.3f*std::sin(p0+pi))*.001f*smpl);y=(y+yy)*.5f;}
 z.mod.write(x+(mod==4?regen*y:0));if(mod==4)y=(x+y)*.5f;
 }else{
 z.mod.write(x);
 if(mod==3){float f=250.f*std::pow(12.f,(.5f+.5f*lfo)*depth);float t=std::tan(pi*std::min(f,smpl*.4f)/smpl),a=(t-1)/(t+1);y=x+z.regen*regen;for(auto& q:z.ap)y=q.tick(y,a);z.regen=y;y=(x+y)*.5f;}
 else if(mod==5){float low=z.rotor.lp(x,1-std::exp(-2*pi*800/smpl));float high=x-low;float dop=z.mod.read((2.5f+1.7f*depth*lfo)*.001f*smpl);y=low*(1-.30f*depth+.30f*depth*std::sin(p0+.8f))+(high*.45f+dop*.55f)*(1-.45f*depth+.45f*depth*lfo);}
 else if(mod==6)y=x*(1-depth*.5f+depth*.5f*lfo);
 }
 modOut[c]=mod==0?0:y;
 float dt=c?delayR:delayL;
 float flutter=color*.24f*std::sin(2*pi*phase*3.f+c);
 echo[c]=z.delay.read((dt+flutter)*.001f*smpl);
 }
 for(int c=0;c<2;++c){
 auto& z=state[c];float x=dry[c];float modSignal=mod==0?x:x+mm*(modOut[c]-x);
 float feedback=z.echoLP.lp(.82f*echo[c]+.18f*echo[1-c],echoLP);feedback-=z.echoHP.lp(feedback,echoHP);
 z.delay.write(sat(modSignal+fb*feedback,color));
 float send=modSignal+dm*echo[c]*.65f;float pd=v[11]*.001f*smpl;verbIn[c]=pd<1?send:z.pre.read(pd);z.pre.write(send);
 }
 float vl=0,vr=0;verb.tick(verbIn[0],verbIn[1],reverb,v[10],1-color*.65f,vl,vr);
 float verbs[2]={vl,vr};float directGain=mod==0?1:1-mm;float a=aux.getNextValue();float master=mix+(1-mix)*a;
 for(int c=0;c<2;++c){auto& z=state[c];float w=mm*modOut[c]+dm*echo[c]+rm*verbs[c];w=sat(w,color*.45f);w-=z.hp.lp(w,hp);wet[c]=z.lp.lp(w,lp);}
 float m=(wet[0]+wet[1])*.5f,s=(wet[0]-wet[1])*.5f*width;wet[0]=m+s;wet[1]=m-s;
 float g=gain(output.getNextValue());
 for(int c=0;c<ch;++c){float w=ch==1?(wet[0]+wet[1])*.5f:wet[c];float y=g*((1-a)*(1-master+master*directGain)*dry[c]+master*w);if(!std::isfinite(y))y=0;b.setSample(c,i,y);maxPeak=std::max(maxPeak,std::abs(y));}
 }
 peak.store(maxPeak);
 }
 void getStateInformation(MemoryBlock& b)override{auto s=p.copyState();s.setProperty("program",currentProgram,nullptr);auto x=s.createXml();copyXmlToBinary(*x,b);}
 void setStateInformation(const void* d,int n)override{auto x=getXmlFromBinary(d,n);if(x&&x->hasTagName(p.state.getType())){auto v=ValueTree::fromXml(*x);currentProgram=int(v.getProperty("program",0));p.replaceState(v);}}
 bool hasEditor()const override{return true;}AudioProcessorEditor* createEditor()override;
};
class GlowEditor:public AudioProcessorEditor,private Timer{
 Afterglow& o;
 ComboBox presets,mod,verb;ToggleButton sync{"TEMPO SYNC"},aux{"AUX / 100% WET"};
 std::array<Slider,N> knobs;std::array<Label,N> labels;
 std::array<std::unique_ptr<AP::SliderAttachment>,N> at;
 std::unique_ptr<AP::ComboBoxAttachment> ma,va;std::unique_ptr<AP::ButtonAttachment> sa,aa;
 Slider out;Label outLabel;std::unique_ptr<AP::SliderAttachment> oa;
 class Look:public LookAndFeel_V4{public:void drawRotarySlider(Graphics& g,int x,int y,int w,int h,float p,float a,float b,Slider&)override{float cx=x+w*.5f,cy=y+h*.5f,r=std::min(w,h)*.36f;Rectangle<float> q(cx-r,cy-r,2*r,2*r);g.setColour(Colour(0xff0d1018));g.fillEllipse(q.expanded(4));g.setColour(Colour(0xffb58dd9));Path arc;arc.addCentredArc(cx,cy,r+4,r+4,0,a,a+(b-a)*p,true);g.strokePath(arc,PathStrokeType(2.5f));g.setGradientFill(ColourGradient(Colour(0xff858b9d),cx,cy-r,Colour(0xff30333d),cx,cy+r,false));g.fillEllipse(q);float t=a+(b-a)*p;g.setColour(Colour(0xffffe2fa));g.drawLine(cx+std::sin(t)*r*.35f,cy-std::cos(t)*r*.35f,cx+std::sin(t)*r*.85f,cy-std::cos(t)*r*.85f,2.5f);}}look;
 void setup(Slider& s,Label& l,const String& name){s.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);s.setTextBoxStyle(Slider::TextBoxBelow,false,70,20);s.setColour(Slider::textBoxTextColourId,Colour(0xffe2d4ee));s.setColour(Slider::textBoxBackgroundColourId,Colour(0xff10131b));s.setColour(Slider::textBoxOutlineColourId,Colours::transparentBlack);addAndMakeVisible(s);l.setText(name,dontSendNotification);l.setJustificationType(Justification::centred);l.setColour(Label::textColourId,Colour(0xffe1d9e9));addAndMakeVisible(l);}
public:
 GlowEditor(Afterglow& p):AudioProcessorEditor(p),o(p){setSize(1020,590);setLookAndFeel(&look);
 String group;for(int i=0;i<o.getNumPrograms();++i){if(group!=glowPresets[i].group){group=glowPresets[i].group;presets.addSectionHeading(group);}presets.addItem(glowPresets[i].name,i+1);}
 presets.setSelectedId(o.getCurrentProgram()+1,dontSendNotification);presets.onChange=[this]{if(presets.getSelectedId()>0)o.setCurrentProgram(presets.getSelectedId()-1);};addAndMakeVisible(presets);
 StringArray mods{"Off","Analog chorus","Symphonic","Phaser","Flanger","Rotary","Tremolo","Dimension"};for(int j=0;j<mods.size();++j)mod.addItem(mods[j],j+1);
 StringArray verbs{"Off","Plate","Hall","Spring","Room","Gated"};for(int j=0;j<verbs.size();++j)verb.addItem(verbs[j],j+1);
 for(auto* c:{&presets,&mod,&verb}){c->setColour(ComboBox::backgroundColourId,Colour(0xff141825));c->setColour(ComboBox::textColourId,Colour(0xffe2d4ee));c->setColour(ComboBox::outlineColourId,Colour(0xff79648f));addAndMakeVisible(*c);}
 ma=std::make_unique<AP::ComboBoxAttachment>(o.p,"mod",mod);va=std::make_unique<AP::ComboBoxAttachment>(o.p,"verb",verb);
 for(auto* t:{&sync,&aux}){t->setColour(ToggleButton::textColourId,Colour(0xffe2d4ee));addAndMakeVisible(*t);}
 sa=std::make_unique<AP::ButtonAttachment>(o.p,"sync",sync);aa=std::make_unique<AP::ButtonAttachment>(o.p,"aux",aux);
 for(int j=0;j<N;++j)if(j!=0&&j!=9&&j!=18){setup(knobs[j],labels[j],titles[j]);at[j]=std::make_unique<AP::SliderAttachment>(o.p,ids[j],knobs[j]);}
 setup(out,outLabel,"OUTPUT dB");oa=std::make_unique<AP::SliderAttachment>(o.p,"output",out);startTimerHz(20);}
 ~GlowEditor()override{stopTimer();setLookAndFeel(nullptr);}
 void timerCallback()override{repaint(775,18,220,58);}
 void paint(Graphics& g)override{g.fillAll(Colour(0xff101219));g.setGradientFill(ColourGradient(Colour(0xff333345),0,10,Colour(0xff202333),0,580,false));g.fillRoundedRectangle(12,12,996,566,5);g.setColour(Colour(0xffc7a0e4));g.setFont(FontOptions(36.f,Font::bold));g.drawText("AFTERGLOW",30,17,450,45,Justification::centredLeft);g.setFont(FontOptions(12.f));g.drawText("BOB AUDIO  /  VINTAGE STUDIO FX RACK",33,62,400,17,Justification::centredLeft);
 g.setColour(Colour(0xff11151e));g.fillRoundedRectangle(780,24,205,40,3);float db=20*std::log10(std::max(o.peak.load(),1e-6f));for(int i=0;i<18;++i){g.setColour(db>-54+i*3?Colour(i>15?0xffe7a66b:0xff93c7ba):Colour(0xff34404a));g.fillRect(788+i*10,33,7,12);}g.setColour(Colour(0xffc7a0e4));g.drawText("OUTPUT",785,46,195,14,Justification::centred);
 const char* h[]={"01  MODULATION","02  STEREO DELAY","03  REVERB","04  RETURN / COLOR"};for(int j=0;j<4;++j){int y=121+j*110;g.setColour(Colour(0xff151925));g.fillRoundedRectangle(25,float(y),970,104,4);g.setColour(Colour(0xffba9bcf));g.setFont(FontOptions(13.f,Font::bold));g.drawText(h[j],35,y+5,185,22,Justification::centredLeft);}
 }
 void resized()override{
 presets.setBounds(32,84,455,29);aux.setBounds(510,84,220,29);sync.setBounds(740,84,200,29);
 mod.setBounds(35,156,170,27);verb.setBounds(35,376,170,27);
 auto place=[&](int j,int x,int y){labels[j].setBounds(x,y,114,18);knobs[j].setBounds(x,y+18,114,79);};
 for(int j=1;j<=4;++j)place(j,230+(j-1)*175,125);
 for(int j=5;j<=8;++j)place(j,230+(j-5)*175,235);
 for(int j=10;j<=12;++j)place(j,230+(j-10)*175,345);
 for(int j=13;j<=17;++j)place(j,225+(j-13)*126,455);
 outLabel.setBounds(855,455,114,18);out.setBounds(855,473,114,79);
 }
};
AudioProcessorEditor* Afterglow::createEditor(){return new GlowEditor(*this);}
AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new Afterglow();}
