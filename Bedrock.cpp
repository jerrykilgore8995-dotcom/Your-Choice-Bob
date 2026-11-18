#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
using namespace juce;
using AP=AudioProcessorValueTreeState;
namespace {
constexpr int N=13;
const std::array<const char*,N> ids={"input","rumble","split","anchor","weight","grind","growl","clarity","smooth","iron","balance","glue","output"};
const std::array<const char*,N> labels={"INPUT","RUMBLE Hz","SPLIT Hz","ANCHOR","WEIGHT","GRIND","GROWL","CLARITY","SMOOTH","IRON","BALANCE","GLUE","OUTPUT"};
const std::array<float,N> lo={-18,20,80,0,-6,0,-6,-6,0,0,-100,0,-18},hi={18,65,350,100,6,100,6,6,100,100,100,100,18},defaults={0,28,150,60,1,30,0,1,30,20,0,20,0};
AP::ParameterLayout layout(){AP::ParameterLayout l;for(int i=0;i<N;++i)l.add(std::make_unique<AudioParameterFloat>(ParameterID{ids[i],1},labels[i],NormalisableRange<float>(lo[i],hi[i],.1f),defaults[i]));return l;}
float gain(float d){return std::exp(d*.11512925465f);}
float db(float x){return 8.685889638f*std::log(std::max(x,1e-8f));}
struct Bell{
 float b0=1,b1=0,b2=0,a1=0,a2=0,z1=0,z2=0;
 void set(float f,float q,float d,double sr){float w=6.2831853f*std::min(f,float(sr*.45))/float(sr),c=std::cos(w),a=std::sin(w)/(2*q),A=std::sqrt(gain(d)),a0=1+a/A;b0=(1+a*A)/a0;b1=-2*c/a0;b2=(1-a*A)/a0;a1=b1;a2=(1-a/A)/a0;}
 float tick(float x){float y=b0*x+z1;z1=b1*x-a1*y+z2;z2=b2*x-a2*y;return y;}
};
struct Channel{Bell weight,growl,clarity;float dcx=0,dcy=0,previous=0,top=0;};
}
class Bedrock:public AudioProcessor{
public:
 AP p{*this,nullptr,"BEDROCK",layout()};
 std::array<SmoothedValue<float>,N> sm;
 dsp::LinkwitzRileyFilter<float> crossover,rumble;
 std::unique_ptr<dsp::Oversampling<float>> os;
 AudioBuffer<float> lows;
 dsp::DelayLine<float,dsp::DelayLineInterpolationTypes::None> lowDelay{8192};
 std::array<Channel,2> state;
 double sr=44100;float eLow=0,eHigh=0,eGlue=0,gLow=0,gHigh=0,gGlue=0;
 std::atomic<float> lowMeter{0},highMeter{0},glueMeter{0};
 Bedrock():AudioProcessor(BusesProperties().withInput("Input",AudioChannelSet::stereo(),true).withOutput("Output",AudioChannelSet::stereo(),true)){}
 const String getName()const override{return "BEDROCK";}
 bool acceptsMidi()const override{return false;}bool producesMidi()const override{return false;}bool isMidiEffect()const override{return false;}
 double getTailLengthSeconds()const override{return 0;}int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}const String getProgramName(int)override{return {};}void changeProgramName(int,const String&)override{}
 bool isBusesLayoutSupported(const BusesLayout& l)const override{return l.getMainInputChannelSet()==l.getMainOutputChannelSet()&&(l.getMainOutputChannelSet()==AudioChannelSet::mono()||l.getMainOutputChannelSet()==AudioChannelSet::stereo());}
 void prepareToPlay(double s,int n)override{
 sr=s;auto ch=(uint32)getTotalNumOutputChannels();dsp::ProcessSpec spec{s,(uint32)n,ch};crossover.prepare(spec);rumble.prepare(spec);rumble.setType(dsp::LinkwitzRileyFilterType::highpass);
 crossover.setCutoffFrequency(150);rumble.setCutoffFrequency(28);os=std::make_unique<dsp::Oversampling<float>>(ch,2,dsp::Oversampling<float>::filterHalfBandFIREquiripple,true,true);os->initProcessing((size_t)n);os->reset();
 int latency=roundToInt(os->getLatencyInSamples());setLatencySamples(latency);lowDelay.prepare(spec);lowDelay.setDelay((float)latency);lowDelay.reset();lows.setSize((int)ch,n);state={};eLow=eHigh=eGlue=gLow=gHigh=gGlue=0;
 for(int i=0;i<N;++i){sm[i].reset(s,.03);sm[i].setCurrentAndTargetValue(*p.getRawParameterValue(ids[i]));}
 }
 void releaseResources()override{os.reset();}
 float follow(float v,float t,float a,float r)const{float k=std::exp(-1000.f/(float(sr)*(t>v?a:r)));return k*v+(1-k)*t;}
 void processBlock(AudioBuffer<float>& b,MidiBuffer&)override{
 ScopedNoDenormals nd;if(!os)return;int n=b.getNumSamples(),ch=b.getNumChannels();if(n>lows.getNumSamples()){b.clear();return;}
 for(int j=0;j<N;++j)sm[j].setTargetValue(*p.getRawParameterValue(ids[j]));
 float v[N];for(int j=0;j<N;++j)v[j]=sm[j].skip(n);
 rumble.setCutoffFrequency(v[1]);crossover.setCutoffFrequency(v[2]);float anchor=v[3]*.01f,smooth=v[8]*.01f,grind=v[5]*.01f,iron=v[9]*.01f,glue=v[11]*.01f,in=gain(v[0]),out=gain(v[12]);
 for(int c=0;c<ch;++c){state[c].weight.set(70,.7f,v[4],sr);state[c].growl.set(700,.85f,v[6],sr);state[c].clarity.set(2200,.9f,v[7],sr);}
 for(int i=0;i<n;++i){
 float low[2]{},high[2]{},pl=0,ph=0;
 for(int c=0;c<ch;++c){float x=rumble.processSample(c,b.getSample(c,i)*in);crossover.processSample(c,x,low[c],high[c]);low[c]=state[c].weight.tick(low[c]);high[c]=state[c].clarity.tick(state[c].growl.tick(high[c]));pl=std::max(pl,low[c]*low[c]);ph=std::max(ph,high[c]*high[c]);}
 eLow=follow(eLow,pl,12,110);eHigh=follow(eHigh,ph,5,70);
 float tl=std::min(18.f,std::max(0.f,db(std::sqrt(eLow))+30)*.85f)*anchor;
 float th=std::min(12.f,std::max(0.f,db(std::sqrt(eHigh))+28)*.7f)*smooth;
 gLow=follow(gLow,tl,8,145);gHigh=follow(gHigh,th,3,100);
 // Limited makeup keeps the low path stable without forcing a fixed loudness.
 float lg=gain(-gLow+anchor*5.f),hg=gain(-gHigh+smooth*2.f);
 for(int c=0;c<ch;++c){float x=low[c]*lg;lowDelay.pushSample(c,x);lows.setSample(c,i,lowDelay.popSample(c));b.setSample(c,i,high[c]*hg);}
 }
 dsp::AudioBlock<float> block(b);auto up=os->processSamplesUp(block);
 float drive=1+grind*12,bias=.20f*grind,tb=std::tanh(bias),norm=drive*(1-tb*tb);
 for(size_t c=0;c<up.getNumChannels();++c){auto* x=up.getChannelPointer(c);for(size_t i=0;i<up.getNumSamples();++i){float a=x[i],sat=(std::tanh(drive*a+bias)-tb)/norm;float y=a+grind*(sat-a);float col=std::atan(2*y)*.5f;y+=iron*.65f*(col-y);x[i]=y;}}
 os->processSamplesDown(block);
 float lb=gain(-std::max(0.f,v[10])*.12f),hb=gain(std::min(0.f,v[10])*.12f),dc=std::exp(-6.2831853f*3/float(sr)),top=1-std::exp(-6.2831853f*std::min(9000.f,float(sr*.4))/float(sr));
 for(int i=0;i<n;++i){float y[2]{},pk=0;for(int c=0;c<ch;++c){auto& z=state[c];float h=b.getSample(c,i);z.top+=top*(h-z.top);h+=(z.top-h)*grind*.65f;float x=lb*lows.getSample(c,i)+hb*h;y[c]=x-z.dcx+dc*z.dcy;z.dcx=x;z.dcy=y[c];pk=std::max(pk,y[c]*y[c]);}eGlue=follow(eGlue,pk,15,130);gGlue=follow(gGlue,std::min(6.f,std::max(0.f,db(std::sqrt(eGlue))+18)*.4f)*glue,15,160);float g=gain(-gGlue+glue)*out;for(int c=0;c<ch;++c)b.setSample(c,i,y[c]*g);}
 lowMeter.store(gLow);highMeter.store(gHigh);glueMeter.store(gGlue);
 }
 void getStateInformation(MemoryBlock& b)override{auto x=p.copyState().createXml();copyXmlToBinary(*x,b);}void setStateInformation(const void* d,int n)override{auto x=getXmlFromBinary(d,n);if(x&&x->hasTagName(p.state.getType()))p.replaceState(ValueTree::fromXml(*x));}
 bool hasEditor()const override{return true;}AudioProcessorEditor* createEditor()override;
};
class BedEditor:public AudioProcessorEditor,private Timer{
 Bedrock& o;std::array<Slider,N> s;std::array<Label,N> l;std::array<std::unique_ptr<AP::SliderAttachment>,N> a;
 ComboBox presets;
 class Look:public LookAndFeel_V4{public:void drawRotarySlider(Graphics& g,int x,int y,int w,int h,float p,float a,float b,Slider&)override{float cx=x+w*.5f,cy=y+h*.5f,r=std::min(w,h)*.37f;Rectangle<float> box(cx-r,cy-r,2*r,2*r);g.setColour(Colour(0xff101618));g.fillEllipse(box.expanded(5));g.setColour(Colour(0xff58b7a8));Path arc;arc.addCentredArc(cx,cy,r+5,r+5,0,a,a+(b-a)*p,true);g.strokePath(arc,PathStrokeType(3));g.setGradientFill(ColourGradient(Colour(0xffa5a4a0),cx,cy-r,Colour(0xff343a3b),cx,cy+r,false));g.fillEllipse(box);g.setColour(Colour(0xfff1e5cb));float q=a+(b-a)*p;g.drawLine(cx+std::sin(q)*r*.35f,cy-std::cos(q)*r*.35f,cx+std::sin(q)*r*.88f,cy-std::cos(q)*r*.88f,3);}}look;
 void loadPreset(int id){
 const float presetsData[5][N]={{0,28,150,65,2,25,1,1,30,25,0,25,0},{0,30,180,80,1,65,3,2,45,35,10,30,0},{0,25,120,50,3,15,0,0,25,45,-10,20,0},{0,30,200,70,0,45,1,4,50,20,15,20,0},{0,24,140,45,1,5,0,0,20,15,0,15,0}};
 if(id<1||id>5)return;for(int j=0;j<N;++j)if(auto* p=o.p.getParameter(ids[j])){p->beginChangeGesture();p->setValueNotifyingHost(p->convertTo0to1(presetsData[id-1][j]));p->endChangeGesture();}
 }
public:
 BedEditor(Bedrock& p):AudioProcessorEditor(p),o(p){setSize(910,465);setLookAndFeel(&look);
 presets.addItem("Rock foundation",1);presets.addItem("Metal demolition",2);presets.addItem("Vintage fat",3);presets.addItem("Pick attack",4);presets.addItem("Clean DI",5);presets.setTextWhenNothingSelected("START HERE / PRESETS");presets.onChange=[this]{loadPreset(presets.getSelectedId());presets.setSelectedId(0,dontSendNotification);};addAndMakeVisible(presets);
 for(int i=0;i<N;++i){s[i].setSliderStyle(Slider::RotaryHorizontalVerticalDrag);s[i].setTextBoxStyle(Slider::TextBoxBelow,false,74,20);s[i].setColour(Slider::textBoxTextColourId,Colour(0xffe2d9c4));s[i].setColour(Slider::textBoxBackgroundColourId,Colour(0xff101618));s[i].setColour(Slider::textBoxOutlineColourId,Colours::transparentBlack);addAndMakeVisible(s[i]);l[i].setText(labels[i],dontSendNotification);l[i].setJustificationType(Justification::centred);l[i].setColour(Label::textColourId,Colour(0xffe2d9c4));addAndMakeVisible(l[i]);a[i]=std::make_unique<AP::SliderAttachment>(o.p,ids[i],s[i]);}startTimerHz(24);}
 ~BedEditor()override{stopTimer();setLookAndFeel(nullptr);}
 void timerCallback()override{repaint(480,16,405,65);}
 void paint(Graphics& g)override{g.fillAll(Colour(0xff111819));g.setGradientFill(ColourGradient(Colour(0xff34494b),0,12,Colour(0xff1d2c2e),0,450,false));g.fillRoundedRectangle(12,12,886,441,5);g.setColour(Colour(0xff9bd9c8));g.setFont(FontOptions(36.f,Font::bold));g.drawText("BEDROCK",30,19,400,45,Justification::centredLeft);g.setFont(FontOptions(12.f));g.drawText("BOB AUDIO / SPLIT BASS CONSOLE",33,61,400,17,Justification::centredLeft);
 const char* t[]={"ANCHOR","SMOOTH","GLUE"};float m[]={o.lowMeter.load(),o.highMeter.load(),o.glueMeter.load()};for(int j=0;j<3;++j){int x=490+j*130;g.setColour(Colour(0xff101618));g.fillRoundedRectangle(float(x),24,122,53,3);g.setColour(Colour(0xff9bd9c8));g.drawText(t[j],x,27,122,15,Justification::centred);for(int k=0;k<12;++k){g.setColour(m[j]>k*(j==0?1.5f:1.f)?Colour(0xff65c6b2):Colour(0xff304745));g.fillRect(x+5+k*9,46,6,10);}g.drawText(String(m[j],1)+" dB",x,57,122,16,Justification::centred);}
 g.setColour(Colour(0xff577f7b));g.drawHorizontalLine(116,30,880);g.drawHorizontalLine(280,30,880);g.drawHorizontalLine(438,30,880);for(int x:{22,888})for(int y:{22,443}){g.setColour(Colour(0xff8d9290));g.fillEllipse(float(x-4),float(y-4),8,8);g.setColour(Colours::black);g.drawLine(float(x-3),float(y+2),float(x+3),float(y-2),1);}}
 void resized()override{presets.setBounds(32,84,260,26);for(int i=0;i<N;++i){int row=i<7?0:1,j=row?i-7:i;int x=(row?90:30)+j*120,y=row?287:124;l[i].setBounds(x,y,112,20);s[i].setBounds(x,y+21,112,111);}}
};
AudioProcessorEditor* Bedrock::createEditor(){return new BedEditor(*this);}
AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new Bedrock();}
