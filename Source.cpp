#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
using namespace juce;
using AP=AudioProcessorValueTreeState;
static AP::ParameterLayout layout(){
 AP::ParameterLayout l;
 auto add=[&](const char* id,const char* name,float lo,float hi,float def,float step=0.1f){l.add(std::make_unique<AudioParameterFloat>(ParameterID{id,1},name,NormalisableRange<float>(lo,hi,step),def));};
 add("input","Input",-18,18,0); add("threshold","Threshold",-48,0,-22); add("attack","Attack ms",1,40,15); add("release","Release ms",30,600,100); add("ratio","Ratio",2,12,4); add("density","Density",0,100,35); add("drive","Drive",0,100,25); add("edge","Edge",0,100,20); add("hpf","Detector Hz",20,250,80); add("mix","Mix",0,100,35); add("output","Output",-18,18,0); 
 return l;
}
class Iron : public AudioProcessor {
public:
 AP p{*this,nullptr,"IRONFIST",layout()};
 std::unique_ptr<dsp::Oversampling<float>> os;
 AudioBuffer<float> dry;
 dsp::DelayLine<float,dsp::DelayLineInterpolationTypes::None> delay{256};
 double sr=44100; float power=0, peak=0, gr1=0,gr2=0; float low[2]{},dcx[2]{},dcy[2]{};
 std::atomic<float> meter{0};
 std::array<SmoothedValue<float>,11> smooth;
 const std::array<const char*,11> ids={"input","threshold","attack","release","ratio","density","drive","edge","hpf","mix","output"};
 Iron():AudioProcessor(BusesProperties().withInput("Input",AudioChannelSet::stereo(),true).withOutput("Output",AudioChannelSet::stereo(),true)){}
 const String getName() const override{return "IRONFIST";}
 bool acceptsMidi()const override{return false;} bool producesMidi()const override{return false;} bool isMidiEffect()const override{return false;}
 double getTailLengthSeconds()const override{return 0;}
 int getNumPrograms()override{return 1;} int getCurrentProgram()override{return 0;} void setCurrentProgram(int)override{} const String getProgramName(int)override{return {};} void changeProgramName(int,const String&)override{}
 bool isBusesLayoutSupported(const BusesLayout& l)const override{return l.getMainInputChannelSet()==l.getMainOutputChannelSet()&&(l.getMainOutputChannelSet()==AudioChannelSet::mono()||l.getMainOutputChannelSet()==AudioChannelSet::stereo());}
 void prepareToPlay(double s,int n)override{
 sr=s; auto ch=(size_t)getTotalNumOutputChannels(); os=std::make_unique<dsp::Oversampling<float>>(ch,2,dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,true,true);
 os->initProcessing((size_t)n); os->reset(); const int lat=roundToInt(os->getLatencyInSamples()); setLatencySamples(lat);
 delay.prepare({s,(uint32)n,(uint32)ch}); delay.setDelay((float)lat); delay.reset(); dry.setSize((int)ch,n); power=peak=gr1=gr2=0; for(int c=0;c<2;++c)low[c]=dcx[c]=dcy[c]=0;
 for(size_t i=0;i<ids.size();++i){smooth[i].reset(s,0.015);smooth[i].setCurrentAndTargetValue(*p.getRawParameterValue(ids[i]));}
 }
 void releaseResources()override{os.reset();}
 static float db(float x){return 20*std::log10(std::max(x,1e-9f));}
 static float gain(float x){return std::pow(10.f,x/20.f);}
 static float knee(float x,float width){if(x<=-width*.5f)return 0;if(x>=width*.5f)return x;return (x+width*.5f)*(x+width*.5f)/(2*width);}
 void processBlock(AudioBuffer<float>& b,MidiBuffer&)override{
 ScopedNoDenormals no; if(!os)return; int n=b.getNumSamples(),ch=b.getNumChannels(); if(n>dry.getNumSamples()){b.clear();return;}
 for(size_t i=0;i<ids.size();++i)smooth[i].setTargetValue(*p.getRawParameterValue(ids[i]));
 // Store gain and mix per block; smooth input and envelope controls per sample.
 float drive=smooth[6].skip(n)*.01f,edge=smooth[7].skip(n)*.01f;
 float mix=smooth[9].getCurrentValue(), mixEnd=smooth[9].skip(n), out=smooth[10].getCurrentValue(),outEnd=smooth[10].skip(n);
 for(int i=0;i<n;++i){
 float input=gain(smooth[0].getNextValue()),t=smooth[1].getNextValue(),a=smooth[2].getNextValue(),r=smooth[3].getNextValue(),ratio=smooth[4].getNextValue(),density=smooth[5].getNextValue()*.01f,hpf=smooth[8].getNextValue();
 float hp=1-std::exp(-MathConstants<float>::twoPi*hpf/(float)sr),sq=0,pk=0;
 for(int c=0;c<ch;++c){float x=b.getSample(c,i); delay.pushSample(c,x);dry.setSample(c,i,delay.popSample(c));x*=input;b.setSample(c,i,x);low[c]+=hp*(x-low[c]);float h=x-low[c];sq+=h*h/ch;pk=std::max(pk,std::abs(h));}
 float rmsCoef=std::exp(-1.f/(.010f*(float)sr));power=rmsCoef*power+(1-rmsCoef)*sq;
 peak=std::max(pk,peak*std::exp(-1.f/(.025f*(float)sr)));
 float target1=(1-1/ratio)*knee(db(std::sqrt(power))-t,4);
 float target2=(1-1/(ratio+4))*knee(db(peak)-t+3,8);
 auto follow=[&](float v,float target,float attack,float release){float k=std::exp(-1000.f/((target>v?attack:release)*(float)sr));return k*v+(1-k)*target;};
 gr1=follow(gr1,target1,a,r);gr2=follow(gr2,target2,std::max(1.f,a*.45f),r*1.7f);
 float g=(1-density)*gain(-gr1)+density*gain(-gr2);
 for(int c=0;c<ch;++c)b.setSample(c,i,b.getSample(c,i)*g);
 }
 meter.store((gr1+gr2)*.5f);
 dsp::AudioBlock<float> block(b); auto high=os->processSamplesUp(block);
 // Asymmetric, zero-offset soft clipping. Edge adds a symmetric harder stage.
 float d=1+drive*9, bias=.18f*drive, e=1+edge*4;
 for(size_t c=0;c<high.getNumChannels();++c){auto* v=high.getChannelPointer(c);for(size_t i=0;i<high.getNumSamples();++i){
 float x=v[i], y=(std::tanh(d*x+bias)-std::tanh(bias))/(d*(1-std::tanh(bias)*std::tanh(bias)));
 y=(1-edge)*y+edge*(std::atan(e*y)/e); v[i]=(drive==0&&edge==0)?x:y;
 }}
 os->processSamplesDown(block);
 float dc=std::exp(-MathConstants<float>::twoPi*3/(float)sr);
 for(int i=0;i<n;++i){float f=(float)(i+1)/std::max(1,n),m=(mix+(mixEnd-mix)*f)*.01f,g=gain(out+(outEnd-out)*f);
 for(int c=0;c<ch;++c){float x=b.getSample(c,i),y=x-dcx[c]+dc*dcy[c];dcx[c]=x;dcy[c]=y;b.setSample(c,i,g*((1-m)*dry.getSample(c,i)+m*y));}}
 }
 void getStateInformation(MemoryBlock& b)override{auto xml=p.copyState().createXml();copyXmlToBinary(*xml,b);}
 void setStateInformation(const void* d,int n)override{auto xml=getXmlFromBinary(d,n);if(xml&&xml->hasTagName(p.state.getType()))p.replaceState(ValueTree::fromXml(*xml));}
 bool hasEditor()const override{return true;}
 AudioProcessorEditor* createEditor()override;
};
class Rack : public AudioProcessorEditor, private Timer {
 Iron& owner;
 std::array<Slider,11> knobs;
 std::array<Label,11> labels;
 std::array<std::unique_ptr<AP::SliderAttachment>,11> attach;
 const std::array<const char*,11> names={"INPUT","THRESHOLD","ATTACK","RELEASE","RATIO","DENSITY","DRIVE","EDGE","SC HPF","BLEND","OUTPUT"};
 class Look : public LookAndFeel_V4 {public:
 void drawRotarySlider(Graphics& g,int x,int y,int w,int h,float pos,float start,float end,Slider&)override{
 float r=std::min(w,h)*.40f,cx=x+w*.5f,cy=y+h*.5f; Rectangle<float> disk(cx-r,cy-r,r*2,r*2);
 g.setColour(Colour(0xff101113));g.fillEllipse(disk.expanded(4));g.setColour(Colour(0xffb97a35));Path arc;arc.addCentredArc(cx,cy,r+5,r+5,0,start,start+(end-start)*pos,true);g.strokePath(arc,PathStrokeType(3));
 g.setGradientFill(ColourGradient(Colour(0xff777b7e),cx,cy-r,Colour(0xff222426),cx,cy+r,false));g.fillEllipse(disk);g.setColour(Colour(0xff111214));g.drawEllipse(disk,2);
 float ang=start+(end-start)*pos;g.setColour(Colour(0xffffe3ab));g.drawLine(cx+std::sin(ang)*r*.45f,cy-std::cos(ang)*r*.45f,cx+std::sin(ang)*r*.86f,cy-std::cos(ang)*r*.86f,3);
 }
 } look;
public:
 Rack(Iron& p):AudioProcessorEditor(p),owner(p){
 setSize(840,410); setLookAndFeel(&look);
 for(size_t i=0;i<knobs.size();++i){auto& k=knobs[i];k.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);k.setTextBoxStyle(Slider::TextBoxBelow,false,76,20);k.setColour(Slider::textBoxTextColourId,Colour(0xfff3d9a7));k.setColour(Slider::textBoxBackgroundColourId,Colour(0xff18191b));k.setColour(Slider::textBoxOutlineColourId,Colours::transparentBlack);addAndMakeVisible(k);labels[i].setText(names[i],dontSendNotification);labels[i].setJustificationType(Justification::centred);labels[i].setColour(Label::textColourId,Colour(0xffded6c8));addAndMakeVisible(labels[i]);attach[i]=std::make_unique<AP::SliderAttachment>(owner.p,owner.ids[i],k);}
 startTimerHz(24);
 }
 ~Rack()override{stopTimer();setLookAndFeel(nullptr);}
 void timerCallback()override{repaint(590,16,228,55);}
 void paint(Graphics& g)override{
 g.fillAll(Colour(0xff202224));g.setColour(Colour(0xff0d0e10));g.fillRoundedRectangle(10,10,820,390,6);g.setGradientFill(ColourGradient(Colour(0xff383b3e),0,10,Colour(0xff222426),0,400,false));g.fillRoundedRectangle(17,17,806,376,4);
 g.setColour(Colour(0xffd9a65d));g.setFont(FontOptions(32.f,Font::bold));g.drawText("IRONFIST",32,20,300,39,Justification::centredLeft);g.setFont(FontOptions(12.f));g.drawText("PARALLEL ANALOG DYNAMICS",35,58,300,18,Justification::centredLeft);
 g.setColour(Colour(0xff161719));g.fillRoundedRectangle(590,23,220,44,4);float gr=owner.meter.load();for(int i=0;i<20;++i){g.setColour(gr>i*1.5f?Colour(0xffe5a243):Colour(0xff443b2d));g.fillRect(598+i*10,34,7,13);}g.setColour(Colour(0xffe9ce9c));g.drawText("GAIN REDUCTION  "+String(gr,1)+" dB",594,49,210,15,Justification::centred);
 g.setColour(Colour(0xff8e754e));g.drawHorizontalLine(84,32,808);g.drawHorizontalLine(233,32,808);
 for(int x:{24,816})for(int y:{24,386}){g.setColour(Colour(0xff888888));g.fillEllipse((float)x-4,(float)y-4,8,8);g.setColour(Colours::black);g.drawLine((float)x-3,(float)y+2,(float)x+3,(float)y-2,1);}
 }
 void resized()override{for(int i=0;i<11;++i){int row=i<6?0:1,j=row?i-6:i; int x=(row?90:36)+j*128,y=row?246:96;labels[i].setBounds(x,y,120,20);knobs[i].setBounds(x,y+20,120,108);}}
};
AudioProcessorEditor* Iron::createEditor(){return new Rack(*this);}
AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new Iron();}
