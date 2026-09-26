#include "track_waveform.h"
#include <iostream>
#include <stdexcept>
using Json=nlohmann::json;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void word(std::string& bytes,std::size_t at,std::uint32_t value){for(int i=0;i<4;++i)bytes[at+i]=static_cast<char>(value>>(24-i*8));}
std::string analysis(bool color,unsigned count=300) {
    const unsigned width=color?2:1;std::string bytes(28+24+count*width,'\0');
    bytes.replace(0,4,"PMAI");word(bytes,4,28);word(bytes,8,static_cast<unsigned>(bytes.size()));
    bytes.replace(28,4,color?"PWV5":"PWV3");word(bytes,32,24);word(bytes,36,24+count*width);word(bytes,40,width);word(bytes,44,count);
    for(unsigned i=0;i<count;++i)if(color){bytes[52+i*2]=static_cast<char>(0xe3);bytes[53+i*2]=static_cast<char>(0xfc);}else bytes[52+i]=static_cast<char>(0xff);
    return bytes;
}
int main(){try{
    for(bool color:{false,true}){const auto result=deckstatus::parse_track_waveform(analysis(color));check(deckstatus::valid_track_waveform(result),"Analysis rejected");check(result["durationMs"]==2000&&result["samples"].size()==300,"Incorrect waveform timing");check(((result["samples"][0].get<int>()>>2)&31)==31,"Peak lost");}
    auto huge=deckstatus::parse_track_waveform(analysis(true,45000));check(huge["samples"].size()==30000&&huge["durationMs"]==300000,"Decimation changed duration or exceeded bound");
    const auto original=analysis(true);
    for(std::size_t size=0;size<original.size();++size)check(deckstatus::parse_track_waveform(std::string_view(original).substr(0,size)).is_null(),"Truncated analysis accepted");
    for(unsigned offset:{4,8,32,36,40,44}){auto broken=original;word(broken,offset,0xffffffff);check(deckstatus::parse_track_waveform(broken).is_null(),"Invalid analysis bounds accepted");}
    auto bad=huge;bad["samples"][0]=65536;check(!deckstatus::valid_track_waveform(bad),"Invalid IPC sample accepted");
    bad=huge;bad["durationMs"]=0;check(!deckstatus::valid_track_waveform(bad),"Zero duration accepted");
    std::cout<<"Track waveforms passed: RGB/blue, timing, peak reduction, truncation and IPC bounds.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
