#pragma once
#include "portal.h"
#include <set>
#include <regex>

namespace deckstatus::track_options {
using Json = nlohmann::json;
inline const std::set<std::string> fields={"title","artist","album","label","key","bpm","currentBpm","cover"};
inline const std::set<std::string> extra={"infoVersion","historyFields","bpmInteger","hideMissing","contentAlign","overflow","elementGap","fieldStyles","coverPosition","coverShape","coverSpin","coverFit"};
inline const std::set<std::string> fonts={"system","serif","mono","arial","calibri","tahoma","verdana","trebuchet","impact","palatino"};
inline void validate(const std::string& key,const Json& value) {
    const auto fail=[] { throw PortalError(400,"presetInvalid"); };
    if(key=="fields"||key=="historyFields") {
        if(key=="historyFields"&&value.is_null())return;
        if(!value.is_array()||value.size()>fields.size()||(key=="fields"&&value.empty()))fail();
        std::set<std::string> used;
        for(const auto& field:value)if(!field.is_string()||!fields.contains(field.get<std::string>())||!used.insert(field.get<std::string>()).second)fail();
    } else if(key=="fieldStyles") {
        if(!value.is_object()||value.size()>9)fail();
        for(auto it=value.begin();it!=value.end();++it) {
            if((!fields.contains(it.key())||it.key()=="cover")&&it.key()!="badges"&&it.key()!="timeline")fail();
            if(!it.value().is_object()||it.value().size()>8)fail();
            for(auto style=it.value().begin();style!=it.value().end();++style) {
                if(style.key()=="color"||style.key()=="background") {if(!style.value().is_string()||!std::regex_match(style.value().get<std::string>(),std::regex("#[0-9a-fA-F]{6}")))fail();}
                else if(style.key()=="font") {if(!style.value().is_string()||!fonts.contains(style.value().get<std::string>()))fail();}
                else if(style.key()=="fontSize") {if(!style.value().is_number_integer()||style.value()<8||style.value()>200)fail();}
                else if(style.key()=="fontStyle") {if(!style.value().is_string()||(style.value()!="normal"&&style.value()!="italic"&&style.value()!="oblique"))fail();}
                else if(style.key()=="fontWeight") {if(!style.value().is_number_integer()||style.value()<100||style.value()>900||style.value().get<int>()%100)fail();}
                else if(style.key()=="marginTop"||style.key()=="marginBottom") {if(!style.value().is_number_integer()||style.value()<0||style.value()>64)fail();}
                else fail();
            }
        }
    } else if(key=="deck") {if(!value.is_number_integer()||value<1||value>4)fail();}
    else if(key=="bpmInteger"||key=="hideMissing"||key=="coverSpin"||key=="coverFit") {if(!value.is_boolean())fail();}
    else if(key=="infoVersion") {if(value!=2)fail();}
    else if(key=="elementGap") {if(!value.is_number_integer()||value<0||value>40)fail();}
    else {
        const std::set<std::string> allowed=key=="coverPosition"?std::set<std::string>{"left","top","right"}:key=="coverShape"?std::set<std::string>{"square","round"}:key=="overflow"?std::set<std::string>{"ellipsis","slide","expand"}:std::set<std::string>{"left","center","right"};
        if(!value.is_string()||!allowed.contains(value.get<std::string>()))fail();
    }
}
}
