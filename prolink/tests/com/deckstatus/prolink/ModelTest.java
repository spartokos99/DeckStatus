package com.deckstatus.prolink;
import org.deepsymmetry.beatlink.*;
import org.json.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.io.*;
import java.nio.file.Path;
import java.util.concurrent.*;

public final class ModelTest {
    static void check(boolean condition, String message) { if (!condition) throw new AssertionError(message); }
    static CdjStatus status(int player, int source, int slot, int id, int flags) throws Exception {
        byte[] bytes = new byte[0x11c];
        System.arraycopy("CDJ-3000".getBytes(StandardCharsets.US_ASCII),0,bytes,0x0b,8);
        bytes[0x21]=(byte)player;bytes[0x23]=(byte)(bytes.length-0x24);
        bytes[0x28]=(byte)source;bytes[0x29]=(byte)slot;bytes[0x2a]=1;
        for(int i=0;i<4;i++)bytes[0x2c+i]=(byte)(id>>((3-i)*8));
        bytes[0x7b]=3;bytes[0x89]=(byte)flags;bytes[0x8d]=0x10;
        bytes[0x92]=0x32;bytes[0x93]=0;bytes[0x9f]=(byte)0xff;
        return new CdjStatus(new DatagramPacket(bytes,bytes.length,InetAddress.getByName("192.0.2."+player),50002));
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
    public static void main(String[] args) throws Exception {
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
        wireEncoding();
        System.out.println("ProLink Java model passed: parsed CDJ packets, flags/BPM, freshness, source/slot/media identities, unknown data, Unicode JSON and actual Windows UTF-8 pipes. No network sockets opened.");
    }
}
