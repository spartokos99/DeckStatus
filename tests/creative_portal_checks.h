#pragma once
#include "scene_components.h"
// Runs inside portal_test's disposable directory, never against the user's store.
void creative_portal_checks(const std::filesystem::path& root) {
    const std::string gif="R0lGODlhAQABAIAAAAAAAP///yH5BAAAAAAALAAAAAABAAEAAAICRAEAOw==";
    const std::string png="iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+jG1sAAAAASUVORK5CYII=";
    std::string asset,other,scene_id,key,old_key;
    {
        Portal p(root);old_key=p.overlay_keys(false)["master"];
        auto uploaded=p.edit_media({{"action","upload"},{"name","Animated <logo>.gif"},{"data",gif}});asset=uploaded["id"];
        check(uploaded["mime"]=="image/gif"&&uploaded["width"]==1&&!uploaded.contains("data"),"Media metadata incorrect");
        check(p.edit_media({{"action","upload"},{"name","Same bytes"},{"data",gif}})["id"]==asset&&p.media().size()==1,"Media deduplication failed");
        check(p.media_file(asset).first=="image/gif"&&p.media_file(asset).second.starts_with("GIF89a"),"Original GIF lost");
        other=p.edit_media({{"action","upload"},{"name","Other.png"},{"data",png}})["id"];
        fails(400,[&]{p.edit_media({{"action","upload"},{"name","active.svg"},{"data","PHN2Zz48L3N2Zz4="}});});
        fails(400,[&]{p.edit_media({{"action","upload"},{"data","!!!!"}});});
        fails(400,[&]{p.edit_media({{"action","upload"},{"data",gif},{"source","https://untrusted.example/file"}});});
        fails(404,[&]{p.media_file("../portal.json");});
        auto too_large=deckstatus::components::decode(png);too_large[16]=0;too_large[17]=0;too_large[18]=0x13;too_large[19]=static_cast<char>(0x88);
        fails(400,[&]{deckstatus::components::image_info(too_large);});
        fails(400,[&]{deckstatus::components::decode(std::string(11184816,'A'));});
        Json image={{"name","Image"},{"type","image"},{"options",{{"assetId",asset},{"fit","contain"},{"audioEnabled",true},{"reactScale",.4}}}};
        const auto preset=p.edit_preset({{"action","save"},{"preset",image}});
        fails(409,[&]{p.edit_media({{"action","delete"},{"id",asset}});});
        auto invalid=image;invalid["options"]["assetId"]=std::string(64,'0');fails(400,[&]{p.edit_preset({{"action","save"},{"preset",invalid}});});
        invalid=image;invalid["options"]["reactScale"]=50;fails(400,[&]{p.edit_preset({{"action","save"},{"preset",invalid}});});
        invalid=image;invalid["options"]["audioEnabled"]="true";fails(400,[&]{p.edit_preset({{"action","save"},{"preset",invalid}});});
        p.edit_preset({{"action","save"},{"preset",{{"name","Literal text"},{"type","text"},{"options",{{"text","<script>literal</script>"},{"fontSize",64},{"color","#ffffff"}}}}}});
        Json layer={{"id","image"},{"type","image"},{"x",0},{"y",0},{"width",640},{"height",240},{"rotation",15},{"opacity",1},{"visible",true},{"options",image["options"]}};
        Json scene={{"name","Creative"},{"width",1920},{"height",1080},{"background","transparent"},{"items",Json::array({layer})}};
        auto saved=p.edit_scene({{"action","save"},{"scene",scene}});scene_id=saved["id"];key=saved["key"];
        check(p.broadcast_access(key,"/api/media/"+asset,scene_id),"Scene asset unavailable");
        check(!p.broadcast_access(key,"/api/media/"+other,scene_id)&&!p.broadcast_access(key,"/api/media",scene_id),"Scene exposed unrelated media");
        check(p.broadcast_access(key,"/api/audio/state",scene_id)&&!p.broadcast_access(key,"/api/audio/devices",scene_id),"Reaction scope incorrect");
        p.edit_preset({{"action","delete"},{"id",preset["id"]},{"revision",1}});
        fails(409,[&]{p.edit_media({{"action","delete"},{"id",asset}});});
        scene["items"][0]["options"]["audioEnabled"]=false;
        saved=p.edit_scene({{"action","save"},{"id",scene_id},{"revision",saved["revision"]},{"scene",scene}});
        check(!p.broadcast_access(key,"/api/audio/state",scene_id),"Static image scene exposed audio");
        scene["items"][0]["visible"]=false;
        saved=p.edit_scene({{"action","save"},{"id",scene_id},{"revision",saved["revision"]},{"scene",scene}});
        check(!p.broadcast_access(key,"/api/media/"+asset,scene_id),"Hidden image still exposed media");
        const auto keys=p.overlay_keys(false);
        check(p.broadcast_access(keys["image"],"/component/image","")&&p.broadcast_access(keys["image"],"/api/media/"+other,""),"Image renderer scope missing");
        check(!p.broadcast_access(keys["text"],"/api/media/"+asset,"")&&!p.broadcast_access(keys["fx"],"/api/scenes",""),"Creative key scope leaked");
        const auto before=p.media();
        const auto held=CreateFileW((root/"portal.json").c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        check(held!=INVALID_HANDLE_VALUE,"Could not lock media store");
        fails(500,[&]{p.edit_media({{"action","delete"},{"id",other}});});CloseHandle(held);check(p.media()==before,"Failed media write changed memory");
    }
    {
        Portal p(root);check(p.media().size()==2&&p.media_file(asset).first=="image/gif","Media lost on restart");
        check(p.scene(scene_id)["items"][0]["rotation"]==15,"Layer rotation lost");
        check(p.overlay_keys(false)["master"]==old_key,"Existing OBS key changed");
        p.edit_media({{"action","delete"},{"id",other}});check(p.media().size()==1,"Unused media not deleted");
    }
    // A v2.0.2 store has neither the media collection nor creative renderer keys.
    Json legacy;{std::ifstream file(root/"portal.json");file>>legacy;}
    legacy.erase("media");for(const auto* kind:{"text","image","fx"})legacy["overlayKeys"].erase(kind);
    {std::ofstream file(root/"portal.json");file<<legacy.dump();}
    {Portal p(root);check(p.media().empty()&&p.overlay_keys(false).size()==6,"Creative migration failed");}
    Json upgraded;{std::ifstream file(root/"portal.json");file>>upgraded;}
    upgraded.erase("media");for(const auto* kind:{"text","image","fx"})upgraded["overlayKeys"].erase(kind);
    check(upgraded==legacy,"Creative migration modified existing records");
}
