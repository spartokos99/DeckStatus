#pragma once
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstdint>
#include <string_view>

namespace deckstatus {
// Read-only PWV3/PWV5 analysis, as documented by Deep Symmetry. Each source
// column covers 1/150 s. Peak reduction bounds HTTP/IPC size without inventing audio.
inline nlohmann::json parse_track_waveform(std::string_view bytes) {
    using Json=nlohmann::json;
    if(bytes.size()<28||bytes.size()>20*1024*1024||bytes.substr(0,4)!="PMAI")return nullptr;
    const auto be32=[&](std::size_t at) {std::uint32_t n=0;for(int i=0;i<4;++i)n=(n<<8)|static_cast<unsigned char>(bytes[at+i]);return n;};
    const auto header=be32(4),length=be32(8);
    if(header<28||header>length||length!=bytes.size())return nullptr;
    std::size_t chosen=0;bool color=false;
    for(std::size_t at=header;at<length;) {
        if(length-at<12)return nullptr;
        const auto h=be32(at+4),size=be32(at+8);
        if(h<12||size<h||size>length-at)return nullptr;
        const auto tag=bytes.substr(at,4);
        if(tag=="PWV5"||tag=="PWV3") {
            if(h!=24||be32(at+12)!=(tag=="PWV5"?2u:1u))return nullptr;
            const auto count=be32(at+16),width=be32(at+12);
            if(!count||count>2160000||count!=(size-h)/width||(size-h)%width)return nullptr;
            if(!chosen||tag=="PWV5") { chosen=at;color=tag=="PWV5"; }
        }
        at+=size;
    }
    if(!chosen)return nullptr;
    const auto count=be32(chosen+16),points=std::min(count,30000u);
    auto samples=Json::array();samples.get_ref<Json::array_t&>().reserve(points);
    for(std::uint32_t i=0;i<points;++i) {
        std::uint16_t peak=0;
        const auto start=static_cast<std::uint64_t>(i)*count/points,end=static_cast<std::uint64_t>(i+1)*count/points;
        for(auto n=start;n<end;++n) {
            const auto at=chosen+24+n*(color?2:1);const auto first=static_cast<unsigned char>(bytes[at]);
            const auto value=static_cast<std::uint16_t>(color?(first<<8)|static_cast<unsigned char>(bytes[at+1]):
                ((first>>5)<<13)|(4u<<10)|(7u<<7)|((first&31u)<<2));
            if(((value>>2)&31)>=((peak>>2)&31))peak=value;
        }
        samples.push_back(peak);
    }
    return {{"format","rgb5"},{"durationMs",static_cast<std::uint64_t>(count)*1000/150},{"samples",std::move(samples)}};
}
inline bool valid_track_waveform(const nlohmann::json& value) {
    if(!value.is_object()||value.size()!=3||value.value("format",nlohmann::json())!="rgb5"||!value.contains("durationMs")||!value["durationMs"].is_number_integer()||
        value["durationMs"].get<std::int64_t>()<=0||value["durationMs"].get<std::int64_t>()>14400000||!value.contains("samples")||!value["samples"].is_array()||value["samples"].empty()||value["samples"].size()>30000)return false;
    for(const auto& v:value["samples"])if(!v.is_number_integer()||v.get<std::int64_t>()<0||v.get<std::int64_t>()>65535)return false;
    return true;
}
}
