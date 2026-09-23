package com.deckstatus.prolink;
import org.deepsymmetry.beatlink.*;
import org.deepsymmetry.beatlink.data.*;
import org.json.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.io.*;
import java.nio.file.Path;
import java.util.concurrent.*;
import java.util.*;

public final class ModelTest {
    static void check(boolean condition, String message) { if (!condition) throw new AssertionError(message); }
    static CdjStatus status(int player, int source, int slot, int id, int flags) throws Exception {
        return status("CDJ-3000", "192.0.2."+player, player, source, slot, id, flags);
    }
    static CdjStatus status(String model, String address, int player, int source, int slot, int id, int flags) throws Exception {
        byte[] bytes = new byte[0x11c];
        byte[] name=model.getBytes(StandardCharsets.US_ASCII);
        System.arraycopy(name,0,bytes,0x0b,name.length);
        bytes[0x21]=(byte)player;bytes[0x23]=(byte)(bytes.length-0x24);
        bytes[0x28]=(byte)source;bytes[0x29]=(byte)slot;bytes[0x2a]=1;
        for(int i=0;i<4;i++)bytes[0x2c+i]=(byte)(id>>((3-i)*8));
        bytes[0x7b]=3;bytes[0x89]=(byte)flags;bytes[0x8d]=0x10;
        bytes[0x92]=0x32;bytes[0x93]=0;bytes[0x9f]=(byte)0xff;
        return new CdjStatus(new DatagramPacket(bytes,bytes.length,InetAddress.getByName(address),50002));
    }
    static DeviceAnnouncement announcement(String model, int number, String address) throws Exception {
        byte[] bytes=new byte[0x36],name=model.getBytes(StandardCharsets.US_ASCII);
        System.arraycopy(name,0,bytes,0x0c,name.length);bytes[0x24]=(byte)number;
        return new DeviceAnnouncement(new DatagramPacket(bytes,bytes.length,InetAddress.getByName(address),50000));
    }
    static void hardwareProfiles() throws Exception {
        for(String model:new String[]{"CDJ-3000","CDJ-3000X","XDJ-AZ"}) {
            for(int player:new int[]{1,2,5,6}) {
                DeviceAnnouncement device=announcement(model,player,"192.0.2.10");
                check(Main.supportedPlayer(device)&&DeviceSupport.supported(device)&&!DeviceSupport.mixer(device),"Player announcement rejected: "+model);
                CdjStatus parsed=status(model,"192.0.2.10",player,1,3,42,0x78);
                check(parsed.getDeviceName().equals(model)&&parsed.getDeviceNumber()==player&&parsed.isTempoMaster()&&parsed.isOnAir()&&parsed.getEffectiveTempo()==128.0,"Model status decoding failed: "+model);
            }
            for(int number:new int[]{0,7,33})check(!Main.supportedPlayer(announcement(model,number,"192.0.2.10")),"Invalid player number selectable");
        }
        for(String model:new String[]{"DJM-A9","DJM-900NXS2"}) {
            DeviceAnnouncement mixer=announcement(model,33,"192.0.2.33");
            check(DeviceSupport.mixer(mixer)&&DeviceSupport.supported(mixer)&&!Main.supportedPlayer(mixer),"Mixer must be automatic, not a deck");
        }
        for(String model:new String[]{"XDJ-RX2","OPUS-QUAD","CDJ-3000-unknown","DJM-900NXS"})
            check(!DeviceSupport.supported(announcement(model,1,"192.0.2.90")),"Unverified model accepted: "+model);
        CdjStatus az1=status("XDJ-AZ","192.0.2.10",1,1,3,42,0x78),az2=status("XDJ-AZ","192.0.2.10",2,1,3,42,0);
        check(az1.getAddress().equals(az2.getAddress())&&az1.getDeviceNumber()!=az2.getDeviceNumber(),"Shared-IP deck identity lost");
        check(Main.identityKey(az1,"192.0.2.10",0).equals(Main.identityKey(az2,"192.0.2.10",0)),"Shared source track duplicated between AZ decks");
        CdjStatus usb2=status("XDJ-AZ","192.0.2.10",2,1,7,42,0),futureSlot=status("XDJ-AZ","192.0.2.10",2,1,8,42,0);
        check(Main.sourceSlot(usb2).equals("UNKNOWN_7"),"Unsupported slot number lost");
        check(!Main.identityKey(usb2,"192.0.2.10",0).equals(Main.identityKey(futureSlot,"192.0.2.10",0)),"Unknown media slots collided");
        check(!Main.identityKey(az1,"192.0.2.10",0).equals(Main.identityKey(usb2,"192.0.2.10",0)),"USB slots collided");
    }
    static void wireEncoding() throws Exception {
        Process process = new ProcessBuilder(Path.of(System.getProperty("java.home"), "bin", "java.exe").toString(),
            "-Dstdout.encoding=windows-1252", "-Dorg.slf4j.simpleLogger.defaultLogLevel=off", "-cp", System.getProperty("java.class.path"),
            "com.deckstatus.prolink.Main", "1000000").redirectError(ProcessBuilder.Redirect.DISCARD).start();
        ExecutorService reader = Executors.newSingleThreadExecutor();
        try {
            String line = reader.submit(() -> new BufferedReader(new InputStreamReader(process.getInputStream(), StandardCharsets.UTF_8.newDecoder())).readLine()).get(5, TimeUnit.SECONDS);
            JSONObject state = new JSONObject(line);
            check(state.getString("version").equals("PRO DJ LINK · Beat Link 8.0.0"), "Windows pipe output is not UTF-8");
            check(state.getJSONObject("setup").getString("status").equals("stopped"), "Helper started networking without a command");
            process.getOutputStream().close();
            check(process.waitFor(5, TimeUnit.SECONDS) && process.exitValue() == 0, "Helper did not exit after stdin closed");
        } finally {
            process.destroyForcibly(); process.waitFor(); reader.shutdownNow();
        }
    }
    static void metadataPolicy() {
        MetadataFinder finder=MetadataFinder.getInstance();
        LifecycleListener unrelated=new LifecycleListener(){
            public void started(LifecycleParticipant sender) {}
            public void stopped(LifecycleParticipant sender) {}
        };
        finder.addLifecycleListener(unrelated);
        CrateDigger.getInstance();
        check(finder.getLifecycleListeners().stream().anyMatch(l->l.getClass().getEnclosingClass()==CrateDigger.class),"Pinned dependency no longer has the expected auto-start hook; review policy");
        Main.configureMetadata();OpusProvider.getInstance();Main.configureMetadata();
        check(!CrateDigger.getInstance().isRunning(),"Unsafe DeviceSQL fallback running");
        check(finder.getLifecycleListeners().stream().noneMatch(l->l.getClass().getEnclosingClass()==CrateDigger.class),"DeviceSQL fallback can auto-start");
        check(finder.getLifecycleListeners().contains(unrelated),"Unrelated lifecycle listener removed");
        finder.removeLifecycleListener(unrelated);
    }
    public static void main(String[] args) throws Exception {
        var devices=Set.of(announcement("CDJ-3000",1,"192.0.2.1"),announcement("CDJ-3000",2,"192.0.2.2"),announcement("CDJ-3000",3,"192.0.2.3"),announcement("DJM-900NXS2",33,"192.0.2.33"),announcement("DJS-1000",4,"192.0.2.4"));
        var selected=Main.selectPlayers(new JSONObject("{\"mapping\":[{\"player\":3,\"deck\":1},{\"player\":1,\"deck\":4}]}"),devices);
        check(selected.equals(Map.of(1,3,4,1)),"Explicit deck mapping changed or unsupported neighbour blocked it");
        check(Main.selectPlayers(new JSONObject("{\"players\":[3,1,2]}"),devices).equals(Map.of(1,1,2,2,3,3)),"Legacy selection mapping changed");
        for(String invalid:new String[]{"{\"mapping\":[{\"player\":4,\"deck\":1}]}","{\"mapping\":[{\"player\":1,\"deck\":1},{\"player\":2,\"deck\":1}]}","{\"mapping\":[{\"player\":1,\"deck\":1},{\"player\":1,\"deck\":2}]}"}) {
            boolean refused=false;try{Main.selectPlayers(new JSONObject(invalid),devices);}catch(IllegalArgumentException expected){refused=true;}check(refused,"Invalid selection accepted");
        }
        var duplicates=new HashSet<>(devices);duplicates.add(announcement("DJS-1000",1,"192.0.2.90"));
        boolean ambiguous=false;try{Main.selectPlayers(new JSONObject("{\"players\":[1]}"),duplicates);}catch(IllegalArgumentException expected){ambiguous=true;}check(ambiguous,"Duplicate selected player number accepted");
        CdjStatus a=status(1,1,3,42,0x78),same=status(2,1,3,42,0),otherMedia=status(2,2,3,42,0),otherSlot=status(2,1,2,42,0);
        check(a.isPlaying() && a.isSynced() && a.isOnAir() && a.isTempoMaster(),"Status flags not decoded");
        check(!same.isPlaying() && !same.isTempoMaster(),"False status flags lost");
        check(a.getBpm()==12800 && a.getEffectiveTempo()==128.0,"Tempo scaling wrong");
        check(Main.fresh(a,a.getTimestamp()+2_999_000_000L),"Fresh status rejected");
        check(!Main.fresh(a,a.getTimestamp()+3_001_000_000L) && !Main.fresh(null,0),"Stale status accepted");
        String identity=Main.identityKey(a,"192.0.2.1",0);
        check(identity.equals(Main.identityKey(same,"192.0.2.1",0)),"Moving same source track duplicated identity");
        check(!identity.equals(Main.identityKey(otherMedia,"192.0.2.2",0)),"Different USB players collided");
        check(!identity.equals(Main.identityKey(otherSlot,"192.0.2.1",0)),"Different media slots collided");
        check(!identity.equals(Main.identityKey(a,"192.0.2.1",1)),"Remounted media reused identity");
        check(!identity.equals(Main.identityKey(a,"192.0.2.11",0)),"Replacement source address reused identity");
        JSONObject blank=Main.emptyDeck(4);check(!blank.getBoolean("loaded") && blank.isNull("bpm") && blank.isNull("playing"),"Unknown data fabricated");
        String title="Quote \" / Möbius / <script> / \n";
        check(new JSONObject(new JSONObject().put("title",Main.text(title)).toString()).getString("title").equals(title),"Metadata JSON damaged");
        check(Main.text(" ")==JSONObject.NULL,"Blank metadata fabricated");
        boolean rejected=false;try{new CdjStatus(new DatagramPacket(new byte[0x40],0x40,InetAddress.getLoopbackAddress(),50002));}catch(IllegalArgumentException expected){rejected=true;}
        check(rejected,"Truncated status packet accepted");
        hardwareProfiles();metadataPolicy();wireEncoding();
        System.out.println("ProLink Java model passed: CDJ-3000/3000X/XDJ-AZ packets, DJM profiles, shared-IP decks, flags/BPM, freshness, source/slot/media identities, disabled DeviceSQL auto-start, Unicode JSON and actual Windows UTF-8 pipes. No network sockets opened.");
    }
}
