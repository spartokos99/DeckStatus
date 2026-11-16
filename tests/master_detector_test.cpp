#include "master_detector.h"
#include "master_history.h"
#include <iostream>
#include <stdexcept>

using Json = nlohmann::json;
using Detector = deckstatus::MasterDetector;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
struct Fixture {
    Detector detector;
    deckstatus::MasterHistory history;
    Detector::Clock::time_point start = Detector::Clock::now();
    int time{};
    Json input = {{"mode","prolink"},{"status","connected"},{"demo",false},{"decks",Json::array()}};
    Json output;
    Fixture() {
        detector.configure({{"detectionBeats",8},{"interruptBeats",4}});
        for (int id = 1; id <= 4; ++id) input["decks"].push_back({
            {"id",id},{"trackId",10+id},{"loaded",true},{"metadataAvailable",true},{"statusAvailable",true},
            {"title","Track "+std::to_string(id)},{"artist","Studio North"},{"bpm",120},{"originalBpm",126},
            {"playing",id==1},{"onAir",true},{"stopped",false},{"positionMs",0},{"durationMs",240000}});
    }
    Json& deck(int id) { return input["decks"][id-1]; }
    Json tick(int ms = 100) {
        time += ms; input["updatedAt"] = time;
        // Deliberately noisy source master; it must have no influence.
        input["masterDeckId"] = time%200==0?2:1;
        output = input; detector.apply(output,start+std::chrono::milliseconds(time)); history.update(output);
        return output["masterDeckId"];
    }
    Json advance(int ms) { while (ms > 100) { tick(); ms -= 100; } return tick(ms); }
};

void transitions() {
    Fixture f;
    check(f.tick(0)==1,"First eligible track was not immediate");
    const auto session=f.output["decks"][0]["sourceSession"];
    check(session.is_string()&&!session.get<std::string>().empty(),"Source session is missing");
    f.deck(2)["playing"]=true;check(f.tick()==1,"Overlap switched immediately");
    f.deck(2)["positionMs"]=999999;f.deck(2)["beatNumber"]=999999;
    check(f.advance(3999)==1,"Smart threshold triggered before eight beats");
    check(f.tick(1)==2,"Eight beats at 120 BPM did not trigger");
    check(f.output["decks"][1]["sourceSession"]==session,"Source session changed during playback");
    Fixture restarted;restarted.tick(0);
    check(restarted.output["decks"][0]["sourceSession"]!=session,"A restarted detector reused the cache identity");
    for(int i=0;i<100;++i)check(f.detector.snapshot()["masterDeckId"]==2,"Reading snapshot changed master");
    check(f.advance(5000)==2&&f.history.full_snapshot()["total"]==2,"Reported outgoing track stole master or duplicated history");
    f.deck(2)["playing"]=false;check(f.tick()==2&&f.advance(1999)==2,"Short pause removed current");
    f.deck(2)["playing"]=true;check(f.tick(1)==2,"Short-pause resume lost current");
    f.deck(2)["onAir"]=false;check(f.tick()==2&&f.advance(1999)==2,"Mixer-cut allowance expired early");
    check(f.tick(1).is_null(),"Expired interruption still live");
    check(f.history.full_snapshot()["total"]==2,"Expired current fell back to an already reported track");
    f.deck(2)["onAir"]=true;check(f.tick()==2,"Resumed track was never reportable again");
    f.detector.configure({{"detectionBeats",1},{"useOnAir",false}});
    check(f.tick()==2&&f.history.full_snapshot()["total"]==2,"Configuration re-armed old tracks");
}

void end_only() {
    Fixture f;f.detector.configure({{"onlyAfterEnd",true}});
    check(f.tick(0)==1,"End-only first track missing");
    f.deck(2)["playing"]=true;check(f.advance(10000)==1,"End-only enabled smart overlap");
    f.deck(3)["playing"]=true;f.advance(1000);
    f.deck(1)["playing"]=false;f.deck(1)["stopped"]=true;
    check(f.tick()==2,"Cue did not select longest-playing unreported deck");
    f.deck(2)["loaded"]=false;check(f.tick()==3,"Unload did not advance");
    f.deck(3)["playing"]=false;f.deck(3)["stopped"]=true;
    check(f.tick().is_null(),"Stopped decks became master");
    Fixture pause;pause.detector.configure({{"onlyAfterEnd",true},{"interruptBeats",0}});
    pause.tick(0);pause.deck(2)["playing"]=true;pause.tick();
    pause.deck(1)["playing"]=false;check(pause.tick()==2,"Zero interruption did not hand over on pause");
    Fixture missing_mixer;missing_mixer.tick(0);missing_mixer.deck(2)["playing"]=true;missing_mixer.tick();
    missing_mixer.deck(1)["onAir"]=nullptr;missing_mixer.deck(1)["stopped"]=true;missing_mixer.deck(1)["playing"]=false;
    check(missing_mixer.tick()==2,"Known cue/end was ignored because outgoing On Air was unknown");
}

void gaps_and_metadata() {
    Fixture f;f.tick(0);f.deck(2)["playing"]=true;f.advance(4200);
    const auto entry=f.history.snapshot()["current"]["entryId"];
    f.input["status"]="disconnected";
    check(f.tick().is_null()&&!f.output["masterAvailable"].get<bool>(),"Outage published live master");
    f.advance(5000);f.input["status"]="connected";
    check(f.tick()==2&&f.history.snapshot()["current"]["entryId"]==entry&&f.history.full_snapshot()["total"]==2,"Reconnect duplicated current or switched to old deck");
    f.deck(2)["statusAvailable"]=false;
    check(f.tick().is_null(),"Missing player sample remained live");
    f.deck(3)["playing"]=true;check(f.advance(9000).is_null(),"Missing outgoing track was treated as stopped during recovery");
    f.deck(2)["statusAvailable"]=true;
    check(f.tick()==3,"A timed incoming track was not eligible after recovery");
    f.deck(3)["metadataAvailable"]=false;f.deck(3)["title"]=nullptr;f.deck(3)["originalBpm"]=nullptr;f.deck(3)["bpm"]=nullptr;
    f.tick();
    check(f.output["decks"][2]["title"]=="Track 3"&&f.output["decks"][2]["originalBpm"]==126&&f.output["decks"][2]["bpm"].is_null(),"Metadata cache lost static fields or invented current BPM");
    f.deck(3)["trackId"]=99;f.tick();
    check(f.output["decks"][2]["title"].is_null()&&f.output["decks"][2]["originalBpm"].is_null(),"Metadata crossed track identity");
    Fixture frozen;frozen.tick(0);frozen.deck(2)["playing"]=true;frozen.tick();
    frozen.input["status"]="stale";frozen.advance(20000);frozen.input["status"]="connected";
    check(frozen.tick()==1,"Unavailable seconds counted toward incoming detection");
    check(frozen.advance(3999)==1&&frozen.tick(1)==2,"Recovered timer did not resume correctly");
    Fixture lost;lost.tick(0);lost.deck(2)["playing"]=true;lost.advance(4200);
    lost.deck(2)["statusAvailable"]=false;lost.deck(3)["playing"]=true;lost.advance(10100);
    check(lost.output["masterDeckId"]==3,"Missing outgoing player blocked next track indefinitely");
    lost.deck(2)["statusAvailable"]=true;check(lost.advance(5000)==3,"Returning old player was re-armed");
}

void unknown_and_boundaries() {
    Fixture f;f.deck(1)["onAir"]=nullptr;
    check(f.tick(0).is_null(),"Unknown On Air qualified");
    f.detector.configure({{"useOnAir",false}});check(f.tick()==1,"Explicit On-Air opt-out was ignored");
    f.deck(2)["playing"]=true;f.deck(2)["bpm"]=nullptr;
    check(f.advance(10000)==1,"Unknown tempo qualified by guessed BPM");
    f.deck(2)["bpm"]=120;check(f.tick()==1&&f.advance(3999)==1&&f.tick(1)==2,"Unknown-tempo interval earned beats");
    for(const auto& value:Json::array({Json{{"detectionBeats",0}},Json{{"interruptBeats",1025}},Json{{"onlyAfterEnd",1}},Json{{"useOnAir","true"}},Json{{"holdMs",4000}},Json{{"prolinkMethod","smart"}}}))
        check(!Detector::valid_settings(value),"Invalid/newly removed setting accepted");
    check(Detector::migrate({{"holdMs",9500}})==Detector::defaults(),"Hold-only store did not migrate");
    check(Detector::migrate({{"prolinkMethod","smart"},{"detectionBeats",32},{"interruptBeats",8},{"useOnAir",false}})==
        Json{{"detectionBeats",32},{"interruptBeats",8},{"onlyAfterEnd",false},{"useOnAir",false}},"Beat settings were lost in migration");
    check(Detector::valid_settings({{"detectionBeats",1024},{"interruptBeats",0}}),"Setting boundaries rejected");
    Fixture cut;cut.tick(0);cut.deck(2)["playing"]=true;cut.tick();cut.advance(2000);
    cut.deck(2)["playing"]=false;cut.tick();cut.advance(1000);cut.deck(2)["playing"]=true;cut.tick();
    check(cut.advance(799)==1&&cut.tick(1)==2,"Short interruption restarted playback time instead of preserving start");
    Fixture offair;offair.tick(0);offair.deck(2)["playing"]=true;offair.deck(2)["onAir"]=false;
    check(offair.advance(5000)==1,"Off-Air pre-listening was reported");
    offair.deck(2)["onAir"]=true;check(offair.tick()==2,"Pre-listening did not retain elapsed playback time");
    Fixture cue_pause;cue_pause.tick(0);cue_pause.deck(2)["playing"]=true;cue_pause.deck(2)["onAir"]=false;
    cue_pause.advance(2000);cue_pause.deck(2)["playing"]=false;cue_pause.tick();cue_pause.advance(1999);
    cue_pause.deck(2)["playing"]=true;cue_pause.deck(2)["onAir"]=true;
    check(cue_pause.tick(1)==2,"An unreported off-air pause expired before the interruption allowance");
    Fixture tempo;tempo.tick(0);tempo.deck(2)["playing"]=true;tempo.tick();tempo.advance(2000);
    tempo.deck(2)["bpm"]=60;check(tempo.advance(5999)==1&&tempo.tick(1)==2,"Threshold did not use current effective BPM");
    Fixture mode;mode.tick(0);mode.deck(1)["playing"]=false;mode.deck(2)["playing"]=true;
    mode.input["mode"]="rekordbox";mode.input["demo"]=true;
    check(mode.tick()==2,"Source change retained the previous mode's episodes");
    mode.input["decks"]=nullptr;mode.tick();
    check(mode.output["status"]=="error"&&!mode.output["masterAvailable"].get<bool>(),"Malformed source appeared available");
}

void rekordbox() {
    Fixture f;f.input["mode"]="rekordbox";
    check(f.tick(0).is_null(),"Rekordbox inferred playback without motion");
    f.deck(1)["positionMs"]=100;check(f.tick().is_null(),"Single position jump inferred playback");
    f.deck(1)["positionMs"]=200;check(f.tick()==1,"Consecutive Rekordbox advances did not start first track");
    check(f.output["decks"][0]["onAir"].is_null()&&f.output["decks"][0]["playbackSource"]=="position","Rekordbox invented On Air");
    f.deck(2)["positionMs"]=120000;f.tick();
    for(int i=0;i<60;++i){f.deck(1)["positionMs"]=300+i*100;check(f.tick()==1,"Stationary sought track was reported as playing");}
    // A loop jump must neither create a new episode nor jump the timer forward.
    f.deck(1)["positionMs"]=100;check(f.tick()==1,"Loop reset the current track");
    f.deck(1)["positionMs"]=200;f.tick();f.deck(1)["positionMs"]=300;f.tick();
    f.deck(2)["positionMs"]=120100;f.deck(1)["positionMs"]=400;f.tick();
    f.deck(2)["positionMs"]=120200;f.deck(1)["positionMs"]=500;check(f.tick()==1,"Rekordbox overlap skipped timing");
    for(int i=0;i<39;++i){f.deck(1)["positionMs"]=600+i*100;f.deck(2)["positionMs"]=120300+i*100;check(f.tick()==1,"Rekordbox threshold early");}
    f.deck(1)["positionMs"]=4500;f.deck(2)["positionMs"]=124200;check(f.tick()==2,"Rekordbox did not use common smart timing");
    f.deck(2)["positionMs"]=0;f.tick();
    check(f.advance(500).is_null(),"Stable cue position failed to end track or old track stole master");
    check(f.history.full_snapshot()["total"]==2,"Rekordbox created duplicate history");
    Fixture repeated;repeated.input["mode"]="rekordbox";repeated.input["sampleTick"]=1;repeated.tick(0);
    repeated.input["sampleTick"]=2;repeated.deck(1)["positionMs"]=100;repeated.tick();
    repeated.input["sampleTick"]=3;repeated.deck(1)["positionMs"]=200;check(repeated.tick()==1,"Fresh bridge samples not detected");
    repeated.advance(3000);
    check(repeated.output["decks"][0]["playing"].is_null()&&!repeated.output["masterAvailable"].get<bool>(),"Repeated bridge timestamp invented a pause/stop");
    repeated.input["sampleTick"]=4;repeated.deck(1)["positionMs"]=400;repeated.tick();
    repeated.input["sampleTick"]=5;repeated.deck(1)["positionMs"]=500;repeated.tick();
    repeated.input["sampleTick"]=6;repeated.deck(1)["positionMs"]=600;
    check(repeated.tick()==1&&repeated.history.full_snapshot()["total"]==1,"Fresh bridge recovery duplicated the track");
}

int main() {
    try { transitions();end_only();gaps_and_metadata();unknown_and_boundaries();rekordbox();
        std::cout<<"Shared now-playing detection: timing, interruptions, end-only, no re-promotion, gaps, metadata, migration and Rekordbox motion passed.\n";return 0;
    } catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
