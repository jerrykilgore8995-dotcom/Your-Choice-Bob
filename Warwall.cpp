#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
using namespace juce;
using AP=AudioProcessorValueTreeState;
namespace {
constexpr int N=14;
const std::array<const char*,N> ids={"input","lowcut","tight","body","mud","bite","fizz","highcut","drive","iron","glue","width","mix","output"};
const std::array<const char*,N> names={"INPUT","LOW CUT","TIGHT","BODY","MUD","BITE","FIZZ","HIGH CUT","DRIVE","IRON","GLUE","WIDTH","BLEND","OUTPUT"};
const std::array<float,N> lo={-18,45,0,-6,-8,-6,0,4500,0,0,0,60,0,-18};
const std::array<float,N> hi={18,160,100,6,0,6,100,18000,100,100,100,130,100,18};
const std::array<float,N> def={0,75,35,0,-1,0,25,12500,20,25,15,100,100,0};
AP::ParameterLayout makeLayout(){AP::ParameterLayout p;for(int i=0;i<N;++i)p.add(std::make_unique<AudioParameterFloat>(ParameterID{ids[i],1},names[i],NormalisableRange<float>(lo[i],hi[i],(i==1||i==7)?1.f:.1f),def[i]));return p;}
float gain(float d){return std::exp(d*.11512925465f);}
float db(float x){return 8.685889638f*std::log(std::max(x,1e-8f));}
struct Biquad {
 float b0=1,b1=0,b2=0,a1=0,a2=0,z1=0,z2=0;
 void set(int type,float f,float q,float d,double sr){
 float w=6.283185307f*std::min(f,float(sr*.45))/float(sr),c=std::cos(w),s=std::sin(w),a=s/(2*q),A=std::sqrt(gain(d)),a0=1+a;
 if(type==0){b0=(1+c)*.5f;b1=-(1+c);b2=b0;a1=-2*c;a2=1-a;}
 else if(type==1){b0=(1-c)*.5f;b1=1-c;b2=b0;a1=-2*c;a2=1-a;}
 else if(type==2){b0=1+a*A;b1=-2*c;b2=1-a*A;a0=1+a/A;a1=-2*c;a2=1-a/A;}
 else {b0=a;b1=0;b2=-a;a1=-2*c;a2=1-a;}
 b0/=a0;b1/=a0;b2/=a0;a1/=a0;a2/=a0;
 }
 float tick(float x){float y=b0*x+z1;z1=b1*x-a1*y+z2;z2=b2*x-a2*y;return y;}
 void reset(){z1=z2=0;}
};
struct Channel {Biquad hp,lp,body,mud,bite,lowBand,fizzBand;float dcx=0,dcy=0;void reset(){*this=Channel{};}};
}
class Warwall : public AudioProcessor {
public:
 AP p{*this,nullptr,"WARWALL",makeLayout()};
 std::array<Channel,2> state;
 std::array<SmoothedValue<float>,N> sm;
 std::unique_ptr<dsp::Oversampling<float>> os;
 AudioBuffer<float> dry;
 double sr=44100;float lowEnv=0,fizzEnv=0,fullEnv=0,grLow=0,grFizz=0,grGlue=0;
 std::atomic<float> lowMeter{0},fizzMeter{0},glueMeter{0};
 Warwall():AudioProcessor(BusesProperties().withInput("Input",AudioChannelSet::stereo(),true).withOutput("Output",AudioChannelSet::stereo(),true)){}
 const String getName()const override{return "WARWALL";}
 bool acceptsMidi()const override{return false;}bool producesMidi()const override{return false;}bool isMidiEffect()const override{return false;}
 double getTailLengthSeconds()const override{return 0;}int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}const String getProgramName(int)override{return {};}void changeProgramName(int,const String&)override{}
 bool isBusesLayoutSupported(const BusesLayout& l)const override{return l.getMainInputChannelSet()==l.getMainOutputChannelSet()&&(l.getMainOutputChannelSet()==AudioChannelSet::mono()||l.getMainOutputChannelSet()==AudioChannelSet::stereo());}
 void prepareToPlay(double s,int n)override{
 sr=s;auto c=(size_t)getTotalNumOutputChannels();os=std::make_unique<dsp::Oversampling<float>>(c,2,dsp::Oversampling<float>::filterHalfBandFIREquiripple,true,true);
 os->initProcessing((size_t)n);os->reset();setLatencySamples(roundToInt(os->getLatencyInSamples()));dry.setSize((int)c,n);
 for(auto& v:state)v.reset();lowEnv=fizzEnv=fullEnv=grLow=grFizz=grGlue=0;
 for(int i=0;i<N;++i){sm[i].reset(s,.025);sm[i].setCurrentAndTargetValue(*p.getRawParameterValue(ids[i]));}
 }
 void releaseResources()override{os.reset();}
 float follow(float v,float x,float a,float r)const{float k=std::exp(-1.f/(float(sr)*.001f*(x>v?a:r)));return k*v+(1-k)*x;}
 void processBlock(AudioBuffer<float>& b,MidiBuffer&)override{
 ScopedNoDenormals nd;if(!os)return;int n=b.getNumSamples(),ch=b.getNumChannels();if(n>dry.getNumSamples()){b.clear();return;}
 for(int j=0;j<N;++j)sm[j].setTargetValue(*p.getRawParameterValue(ids[j]));
 float v[N];for(int j=0;j<N;++j)v[j]=sm[j].skip(n);
 float in=gain(v[0]),out=gain(v[13]),tight=v[2]*.01f,fizz=v[6]*.01f,glue=v[10]*.01f;
 for(int c=0;c<ch;++c){auto& z=state[c];z.hp.set(0,v[1],.707f,0,sr);z.lp.set(1,v[7],.707f,0,sr);z.body.set(2,230,.75f,v[3],sr);z.mud.set(2,420,1.1f,v[4],sr);z.bite.set(2,1650,.85f,v[5],sr);z.lowBand.set(3,145,.8f,0,sr);z.fizzBand.set(3,4200,1.1f,0,sr);}
 // Linked band detectors, gain reduction applied only to the relevant band.
 for(int i=0;i<n;++i){
 float x[2]{},l[2]{},h[2]{},pl=0,ph=0,pf=0;
 for(int c=0;c<ch;++c){auto& z=state[c];x[c]=z.bite.tick(z.mud.tick(z.body.tick(z.hp.tick(b.getSample(c,i)*in))));l[c]=z.lowBand.tick(x[c]);h[c]=z.fizzBand.tick(x[c]);pl=std::max(pl,l[c]*l[c]);ph=std::max(ph,h[c]*h[c]);pf=std::max(pf,x[c]*x[c]);}
 lowEnv=follow(lowEnv,pl,4,85);fizzEnv=follow(fizzEnv,ph,1.5f,60);fullEnv=follow(fullEnv,pf,12,120);
 float tl=std::min(9.f,std::max(0.f,db(std::sqrt(lowEnv))+26)*.65f)*tight;
 float th=std::min(7.f,std::max(0.f,db(std::sqrt(fizzEnv))+32)*.65f)*fizz;
 float tg=std::min(5.f,std::max(0.f,db(std::sqrt(fullEnv))+18)*.35f)*glue;
 grLow=follow(grLow,tl,3,90);grFizz=follow(grFizz,th,2,65);grGlue=follow(grGlue,tg,18,130);
 for(int c=0;c<ch;++c){float y=x[c]+(gain(-grLow)-1)*l[c]+(gain(-grFizz)-1)*h[c];b.setSample(c,i,y*gain(-grGlue));}
 }
 lowMeter.store(grLow);fizzMeter.store(grFizz);glueMeter.store(grGlue);
 // Parallel color inside oversampled stream, preserving phase alignment.
 dsp::AudioBlock<float> block(b);auto high=os->processSamplesUp(block);
 float drive=v[8]*.01f,iron=v[9]*.01f,mix=v[12]*.01f;
 float d=1+drive*4,bias=.16f*iron,tb=std::tanh(bias),norm=d*(1-tb*tb);
 for(size_t c=0;c<high.getNumChannels();++c){auto* a=high.getChannelPointer(c);for(size_t i=0;i<high.getNumSamples();++i){float x=a[i];float sat=(std::tanh(d*x+bias)-tb)/norm;float y=x+drive*(sat-x);float tr=std::atan(2*y)*.5f;y+=iron*.5f*(tr-y);a[i]=x+mix*(y-x);}}
 os->processSamplesDown(block);
 float width=v[11]*.01f,dc=std::exp(-6.2831853f*3/float(sr));
 for(int i=0;i<n;++i){float y[2]{};for(int c=0;c<ch;++c){auto& z=state[c];float x=z.lp.tick(b.getSample(c,i));y[c]=x-z.dcx+dc*z.dcy;z.dcx=x;z.dcy=y[c];}if(ch==2){float m=(y[0]+y[1])*.5f,s=(y[0]-y[1])*.5f*width;y[0]=m+s;y[1]=m-s;}for(int c=0;c<ch;++c)b.setSample(c,i,y[c]*out);}
 }
 void getStateInformation(MemoryBlock& b)override{auto x=p.copyState().createXml();copyXmlToBinary(*x,b);}void setStateInformation(const void* d,int n)override{auto x=getXmlFromBinary(d,n);if(x&&x->hasTagName(p.state.getType()))p.replaceState(ValueTree::fromXml(*x));}
 bool hasEditor()const override{return true;}AudioProcessorEditor* createEditor()override;
};
class WallEditor:public AudioProcessorEditor,private Timer{
 Warwall& o;std::array<Slider,N> sliders;std::array<Label,N> labels;std::array<std::unique_ptr<AP::SliderAttachment>,N> att;
 class Look:public LookAndFeel_V4{public:void drawRotarySlider(Graphics& g,int x,int y,int w,int h,float p,float a,float b,Slider&)override{float cx=x+w*.5f,cy=y+h*.5f,r=std::min(w,h)*.36f;Rectangle<float> box(cx-r,cy-r,r*2,r*2);g.setColour(Colour(0xff121414));g.fillEllipse(box.expanded(5));g.setColour(Colour(0xffc45c36));Path arc;arc.addCentredArc(cx,cy,r+5,r+5,0,a,a+(b-a)*p,true);g.strokePath(arc,PathStrokeType(3));g.setGradientFill(ColourGradient(Colour(0xff858784),cx,cy-r,Colour(0xff292c2b),cx,cy+r,false));g.fillEllipse(box);g.setColour(Colour(0xffead9bb));float q=a+(b-a)*p;g.drawLine(cx+std::sin(q)*r*.4f,cy-std::cos(q)*r*.4f,cx+std::sin(q)*r*.9f,cy-std::cos(q)*r*.9f,3);}}look;
public:
 WallEditor(Warwall& p):AudioProcessorEditor(p),o(p){setSize(900,450);setLookAndFeel(&look);for(int i=0;i<N;++i){auto& s=sliders[i];s.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);s.setTextBoxStyle(Slider::TextBoxBelow,false,72,20);s.setColour(Slider::textBoxTextColourId,Colour(0xffead9bb));s.setColour(Slider::textBoxBackgroundColourId,Colour(0xff161918));s.setColour(Slider::textBoxOutlineColourId,Colours::transparentBlack);addAndMakeVisible(s);labels[i].setText(i==12?"COLOR MIX":names[i],dontSendNotification);labels[i].setJustificationType(Justification::centred);labels[i].setColour(Label::textColourId,Colour(0xffe2d9c4));addAndMakeVisible(labels[i]);att[i]=std::make_unique<AP::SliderAttachment>(o.p,ids[i],s);}startTimerHz(24);}
 ~WallEditor()override{stopTimer();setLookAndFeel(nullptr);}
 void timerCallback()override{repaint(500,15,370,70);}
 void paint(Graphics& g)override{g.fillAll(Colour(0xff151818));g.setGradientFill(ColourGradient(Colour(0xff414642),0,10,Colour(0xff242927),0,440,false));g.fillRoundedRectangle(12,12,876,426,5);g.setColour(Colour(0xffd8a172));g.setFont(FontOptions(36.f,Font::bold));g.drawText("WARWALL",30,22,440,44,Justification::centredLeft);g.setFont(FontOptions(13.f));g.drawText("BOB AUDIO  /  GUITAR BUS CONSOLE",33,66,420,17,Justification::centredLeft);
 const char* title[]={"PALM MUTE","FIZZ","GLUE"};float val[]={o.lowMeter.load(),o.fizzMeter.load(),o.glueMeter.load()};for(int j=0;j<3;++j){int x=510+j*120;g.setColour(Colour(0xff111514));g.fillRoundedRectangle((float)x,25,110,48,3);g.setColour(Colour(0xffd8a172));g.drawText(title[j],x,27,110,14,Justification::centred);for(int k=0;k<10;++k){g.setColour(val[j]>k?Colour(0xffe89b52):Colour(0xff443d32));g.fillRect(x+7+k*10,46,7,10);}g.setColour(Colour(0xffdbd2c2));g.drawText(String(val[j],1)+" dB",x,56,110,15,Justification::centred);}
 g.setColour(Colour(0xff967751));g.drawHorizontalLine(94,30,870);g.drawHorizontalLine(265,30,870);g.drawHorizontalLine(419,30,870);
 for(int x:{22,878})for(int y:{22,428}){g.setColour(Colour(0xff8c8e87));g.fillEllipse(float(x-4),float(y-4),8,8);g.setColour(Colours::black);g.drawLine(float(x-3),float(y+2),float(x+3),float(y-2),1);}}
 void resized()override{for(int i=0;i<N;++i){int x=30+(i%7)*120,y=i<7?110:278;labels[i].setBounds(x,y,112,20);sliders[i].setBounds(x,y+21,112,112);}}
};
AudioProcessorEditor* Warwall::createEditor(){return new WallEditor(*this);}
AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new Warwall();}
