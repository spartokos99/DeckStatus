#pragma once
#include "portal.h"
#include <Windows.h>
#include <wincrypt.h>
#include <cmath>
#include <set>
#include <regex>

namespace deckstatus::components {
using Json = nlohmann::json;
inline bool creative(const std::string& kind) { return kind=="text"||kind=="image"||kind=="fx"; }
inline bool type(const std::string& kind) { return creative(kind)||kind=="deck"||kind=="master"||kind=="waveform"; }
inline const std::set<std::string> reaction_keys={"audioEnabled","audioBand","audioGain","audioThreshold","audioAttack","audioRelease","reactScale","reactX","reactY","reactRotation","reactOpacity"};
inline const std::set<std::string> creative_keys={"width","height","lang","text","font","fontSize","bold","italic","align","verticalAlign","color","background","backgroundOpacity","padding","radius","assetId","fit","effect","color2","intensity","density","speed","flashDuration","cooldown"};
inline bool numeric(const Json& v,double lo,double hi) {return v.is_number()&&std::isfinite(v.get<double>())&&v>=lo&&v<=hi;}
inline void validate_option(const std::string& name,const Json& v) {
    if(name=="audioEnabled"||name=="bold"||name=="italic") {if(!v.is_boolean())throw PortalError(400,"sceneInvalid");return;}
    const std::map<std::string,std::pair<double,double>> bounds={
        {"audioGain",{.1,20}},{"audioThreshold",{0,1}},{"audioAttack",{0,1000}},{"audioRelease",{0,3000}},
        {"reactScale",{-.9,3}},{"reactX",{-2000,2000}},{"reactY",{-2000,2000}},{"reactRotation",{-360,360}},{"reactOpacity",{-1,1}},
        {"width",{32,7680}},{"height",{32,4320}},{"fontSize",{10,300}},{"padding",{0,200}},{"radius",{0,200}},
        {"backgroundOpacity",{0,100}},{"intensity",{0,1}},{"density",{4,100}},{"speed",{0,3}},{"flashDuration",{30,1000}},{"cooldown",{100,3000}}};
    if(const auto it=bounds.find(name);it!=bounds.end()){if(!numeric(v,it->second.first,it->second.second))throw PortalError(400,"sceneInvalid");return;}
    if(!v.is_string())throw PortalError(400,"sceneInvalid");const auto s=v.get<std::string>();
    if(name=="text"){if(s.size()>2048)throw PortalError(400,"sceneInvalid");return;}
    if(name=="assetId"){if(!s.empty()&&!std::regex_match(s,std::regex("[a-f0-9]{64}")))throw PortalError(400,"sceneInvalid");return;}
    if(name=="color"||name=="color2"||name=="background"){if(!std::regex_match(s,std::regex("#[a-fA-F0-9]{6}")))throw PortalError(400,"sceneInvalid");return;}
    const std::map<std::string,std::set<std::string>> values={
        {"audioBand",{"rms","peak","bass","mid","high"}},{"font",{"system","serif","mono"}},
        {"align",{"left","center","right"}},{"verticalAlign",{"top","center","bottom"}},
        {"fit",{"contain","cover","fill"}},{"effect",{"flash","fog","both"}},{"lang",{"en","de"}}};
    const auto it=values.find(name);if(it==values.end()||!it->second.contains(s))throw PortalError(400,"sceneInvalid");
}
inline std::string decode(const std::string& encoded) {
    if(encoded.empty()||encoded.size()>11200000||encoded.size()%4||encoded.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=")!=std::string::npos)throw PortalError(400,"mediaInvalid");
    const auto pad=encoded.find('=');if(pad!=std::string::npos&&(pad<encoded.size()-2||encoded.find_first_not_of('=',pad)!=std::string::npos))throw PortalError(400,"mediaInvalid");
    DWORD count=0;
    if(!CryptStringToBinaryA(encoded.c_str(),static_cast<DWORD>(encoded.size()),CRYPT_STRING_BASE64,nullptr,&count,nullptr,nullptr)||count>8*1024*1024)throw PortalError(400,"mediaInvalid");
    std::string bytes(count,'\0');if(!CryptStringToBinaryA(encoded.c_str(),static_cast<DWORD>(encoded.size()),CRYPT_STRING_BASE64,reinterpret_cast<BYTE*>(bytes.data()),&count,nullptr,nullptr))throw PortalError(400,"mediaInvalid");
    bytes.resize(count);return bytes;
}
inline Json image_info(const std::string& bytes) {
    const auto n=bytes.size();const auto b=[&](std::size_t i)->unsigned{return static_cast<unsigned char>(bytes.at(i));};
    const auto le=[&](std::size_t i,int count)->unsigned{unsigned v=0;for(int j=0;j<count;++j)v|=b(i+j)<<(8*j);return v;};
    const auto be=[&](std::size_t i,int count)->unsigned{unsigned v=0;for(int j=0;j<count;++j)v=(v<<8)|b(i+j);return v;};
    unsigned width=0,height=0;std::string mime;
    if(n>=24&&bytes.compare(0,8,"\x89PNG\r\n\x1a\n",8)==0&&bytes.substr(12,4)=="IHDR"){mime="image/png";width=be(16,4);height=be(20,4);}
    else if(n>=14&&(bytes.starts_with("GIF87a")||bytes.starts_with("GIF89a"))&&bytes.back()==';'){mime="image/gif";width=le(6,2);height=le(8,2);}
    else if(n>=30&&bytes.starts_with("RIFF")&&bytes.substr(8,4)=="WEBP"){
        mime="image/webp";const auto kind=bytes.substr(12,4);
        if(kind=="VP8X"){width=le(24,3)+1;height=le(27,3)+1;}
        else if(kind=="VP8L"&&b(20)==0x2f){width=1+((b(21)|(b(22)<<8))&0x3fff);height=1+((b(22)>>6)|(b(23)<<2)|((b(24)&15)<<10));}
        else if(kind=="VP8 "&&b(23)==0x9d&&b(24)==1&&b(25)==0x2a){width=le(26,2)&0x3fff;height=le(28,2)&0x3fff;}
    } else if(n>=12&&b(0)==0xff&&b(1)==0xd8){
        mime="image/jpeg";std::size_t p=2;
        while(p+4<n){if(b(p++)!=0xff)break;while(p<n&&b(p)==0xff)++p;if(p+2>=n)break;const auto marker=b(p++);if(marker==0xda||marker==0xd9)break;if(marker==1||(marker>=0xd0&&marker<=0xd7))continue;const auto length=be(p,2);if(length<2||p+length>n)break;
            if((marker>=0xc0&&marker<=0xcf)&&marker!=0xc4&&marker!=0xc8&&marker!=0xcc&&length>=8){height=be(p+3,2);width=be(p+5,2);break;}p+=length;}
    }
    if(mime.empty()||!width||!height||width>4096||height>4096||n>8*1024*1024)throw PortalError(400,"mediaInvalid");
    return {{"mime",mime},{"width",width},{"height",height},{"bytes",n}};
}
}
