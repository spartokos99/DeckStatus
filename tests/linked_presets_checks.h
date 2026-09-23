#pragma once
inline void linked_presets_checks(const std::filesystem::path& root) {
    using Json=nlohmann::json;std::string scene_id,preset_id,key;
    const Json preferences={{"autoConnect",true},{"devices",Json::array({{{"player",3},{"deck",1},{"name","CDJ-3000"}},{{"player",1},{"deck",4},{"name","CDJ-3000"}}})}};
    {
        Portal p(root);p.save_prolink_settings(preferences);
        auto bad=preferences;bad["devices"][1]["deck"]=1;fails(400,[&]{p.save_prolink_settings(bad);});check(p.prolink_settings()==preferences,"Invalid selection changed saved devices");
        Json design={{"name","Linked master"},{"type","master"}};
        design["options"]={{"infoVersion",2},{"fields",Json::array({"title","label","bpm","currentBpm"})},{"historyFields",Json::array({"title"})},{"bpmInteger",true},{"hideMissing",true}};
        design["options"]["fieldStyles"]={{"title",{{"font","impact"},{"background","#123456"},{"color","#abcdef"},{"marginTop",2},{"marginBottom",10},{"fontSize",48},{"fontStyle","italic"},{"fontWeight",800}}}};
        auto preset=p.edit_preset({{"action","save"},{"preset",design}});preset_id=preset["id"];
        auto invalid=design;invalid["options"]["fieldStyles"]["title"]["font"]="url(https://invalid.example/font)";fails(400,[&]{p.edit_preset({{"action","save"},{"preset",invalid}});});
        const Json invalid_styles={{"fontSize",201},{"fontWeight",450},{"fontStyle","url(x)"}};
        for(const auto& [property,value]:invalid_styles.items()) {
            auto rejected=design;rejected["options"]["fieldStyles"]["title"][property]=value;fails(400,[&]{p.edit_preset({{"action","save"},{"preset",rejected}});});
        }
        Json layer={{"id","master"},{"type","master"},{"presetId",preset_id},{"x",23},{"y",40},{"width",750},{"height",800},{"rotation",17},{"opacity",.7},{"visible",true},{"options",design["options"]}};
        Json scene={{"name","Linked scene"},{"width",1920},{"height",1080},{"background","transparent"},{"items",Json::array({layer})}};
        auto saved=p.edit_scene({{"action","save"},{"scene",scene}});scene_id=saved["id"];key=saved["key"];
        design["options"]["historyFields"]=Json::array({"artist","label"});
        const auto held=CreateFileW((root/"portal.json").c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);check(held!=INVALID_HANDLE_VALUE,"Could not lock linked store");
        fails(500,[&]{p.edit_preset({{"action","save"},{"id",preset_id},{"revision",1},{"preset",design}});});CloseHandle(held);
        check(p.scene(scene_id,true)==saved&&p.presets()[0]==preset,"Failed update changed linked data");
        p.edit_preset({{"action","save"},{"id",preset_id},{"revision",1},{"preset",design}});
        auto updated=p.scene(scene_id,true);check(updated["items"][0]["options"]==design["options"]&&updated["revision"]==2,"Linked options did not update atomically");
        for(const auto* field:{"x","y","width","height","rotation","opacity","visible"})check(updated["items"][0][field]==layer[field],"Preset update moved or hid layer");
        check(updated["key"]==key,"Preset update changed OBS key");
        fails(409,[&]{p.edit_scene({{"action","save"},{"id",scene_id},{"revision",1},{"scene",saved}});});
        auto mismatch=scene;mismatch["items"][0]["type"]="deck";fails(400,[&]{p.edit_scene({{"action","save"},{"scene",mismatch}});});
        auto deck_design=design;deck_design["type"]="deck";deck_design["name"]="Linked deck";deck_design["options"]["deck"]=1;
        auto deck_preset=p.edit_preset({{"action","save"},{"preset",deck_design}});
        auto deck_scene=scene;auto& deck_layer=deck_scene["items"][0];deck_layer["type"]="deck";deck_layer["presetId"]=deck_preset["id"];deck_layer["options"]=deck_design["options"];deck_layer["options"]["deck"]=4;
        auto deck_saved=p.edit_scene({{"action","save"},{"scene",deck_scene}});
        check(deck_saved["items"][0]["options"]["deck"]==4,"Linked scene save overwrote the selected deck");
        deck_design["options"]["deck"]=2;deck_design["options"]["fieldStyles"]["title"]["fontSize"]=60;
        p.edit_preset({{"action","save"},{"id",deck_preset["id"]},{"revision",1},{"preset",deck_design}});
        const auto deck_updated=p.scene(deck_saved["id"],true);
        check(deck_updated["items"][0]["options"]["deck"]==4&&deck_updated["items"][0]["options"]["fieldStyles"]["title"]["fontSize"]==60,"Preset update lost deck override or typography");
        deck_scene["items"][0]["options"]["deck"]=5;fails(400,[&]{p.edit_scene({{"action","save"},{"scene",deck_scene}});});
    }
    {
        Portal p(root);check(p.prolink_settings()==preferences,"ProLink preferences lost on restart");
        auto before=p.scene(scene_id,true);check(before["items"][0]["presetId"]==preset_id,"Preset link lost on restart");
        p.edit_preset({{"action","delete"},{"id",preset_id},{"revision",2}});
        auto after=p.scene(scene_id,true);check(after["items"][0]["presetId"]==""&&after["items"][0]["options"]==before["items"][0]["options"]&&after["key"]==key,"Deleting preset lost layer or key");
    }
    // Reconstruct an old store: only the unchanged, uniquely named copy may migrate.
    const auto legacy_root=root/"legacy";std::string legacy_scene,legacy_preset,legacy_key;
    {
        Portal p(legacy_root);
        const auto preset=p.edit_preset({{"action","save"},{"preset",{{"name","Legacy master"},{"type","master"},{"options",{{"history",2}}}}}});legacy_preset=preset["id"];
        Json item={{"id","unchanged"},{"name","Legacy master"},{"type","master"},{"x",10},{"y",20},{"width",640},{"height",800},{"opacity",1},{"visible",true},{"options",preset["options"]}};
        auto customized=item;customized["id"]="customized";customized["options"]["history"]=4;
        auto reactive=item;reactive["id"]="reactive";reactive["options"]["audioEnabled"]=true;
        auto inactive=item;inactive["id"]="inactive";inactive["options"]["audioEnabled"]=false;inactive["options"]["reactScale"]=.3;
        item["options"]["audioEnabled"]=false;item["options"]["reactScale"]=0;
        const auto scene=p.edit_scene({{"action","save"},{"scene",{{"name","Legacy scene"},{"width",1920},{"height",1080},{"background","transparent"},{"items",Json::array({item,customized,reactive,inactive})}}}});legacy_scene=scene["id"];legacy_key=scene["key"];
    }
    Json store;{std::ifstream input(legacy_root/"portal.json");input>>store;}
    for(auto& item:store["scenes"][legacy_scene]["items"])item.erase("presetId");
    {std::ofstream output(legacy_root/"portal.json");output<<store.dump();}
    {
        Portal p(legacy_root);const auto scene=p.scene(legacy_scene,true);
        check(scene["items"][0]["presetId"]==legacy_preset&&scene["items"][1]["presetId"]==""&&scene["items"][2]["presetId"]==""&&scene["items"][3]["presetId"]=="","Migration linked a customized layer or lost an exact match");
        auto preset=p.presets()[0];preset["options"]["history"]=7;
        p.edit_preset({{"action","save"},{"id",legacy_preset},{"revision",1},{"preset",preset}});
        const auto updated=p.scene(legacy_scene,true);
        check(updated["items"][0]["options"]["history"]==7&&updated["items"][1]["options"]["history"]==4&&updated["items"][2]["options"]["audioEnabled"]==true&&updated["key"]==legacy_key,"Migrated preset update changed independent layers or OBS key");
    }
}
