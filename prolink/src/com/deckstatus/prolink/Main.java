package com.deckstatus.prolink;

import org.deepsymmetry.beatlink.*;
import org.deepsymmetry.beatlink.data.*;
import org.json.*;
import java.io.*;
import java.net.*;
import java.nio.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.concurrent.*;

/** Optional read-only PRO DJ LINK adapter. stdin/stdout belong exclusively to DeckStatus. */
public final class Main {
    // Windows standard streams may use the native code page even when file.encoding is UTF-8.
    private static final PrintStream OUTPUT = new PrintStream(new FileOutputStream(FileDescriptor.out), true, StandardCharsets.UTF_8);
    private final DeviceFinder finder = DeviceFinder.getInstance();
    private final VirtualCdj cdj = VirtualCdj.getInstance();
    private final MetadataFinder metadata = MetadataFinder.getInstance();
    private final Map<SlotReference, Long> mediaEpochs = new ConcurrentHashMap<>();
    private final Map<String, Integer> trackIds = new ConcurrentHashMap<>();
    private final Set<Integer> sentArt = ConcurrentHashMap.newKeySet();
    private final Map<Integer, String> selectedAddresses = new ConcurrentHashMap<>();
    private final Map<Integer, String> lastTracks = new ConcurrentHashMap<>();
    private final Map<Integer, Long> loadedAt = new ConcurrentHashMap<>();
    private final int idBase;
    private volatile List<Integer> players = List.of();
    private volatile String phase = "stopped", message = "prolinkStopped";
    private volatile String localAddress = "", networkInterface = "";
    private volatile boolean connected;
    private final ScheduledExecutorService timer = Executors.newSingleThreadScheduledExecutor();

    private Main(int idBase) {
        this.idBase = idBase;
        metadata.addMountListener(new MountListener() {
            public void mediaMounted(SlotReference slot) { mediaEpochs.putIfAbsent(slot, 0L); }
            public void mediaUnmounted(SlotReference slot) { mediaEpochs.merge(slot, 1L, Long::sum); }
        });
    }
    static Object text(String value) { return value == null || value.isBlank() ? JSONObject.NULL : value; }
    static Object text(SearchableItem value) { return value == null ? JSONObject.NULL : text(value.label); }
    static boolean supportedPlayer(DeviceAnnouncement device) { return device.getDeviceName().equals("CDJ-3000") && device.getDeviceNumber() >= 1 && device.getDeviceNumber() <= 6; }
    static boolean fresh(DeviceUpdate update, long now) { return update != null && now - update.getTimestamp() >= 0 && now - update.getTimestamp() <= 3_000_000_000L; }
    static boolean matches(TrackMetadata track, CdjStatus status) {
        return track != null && track.trackReference.player == status.getTrackSourcePlayer() &&
            track.trackReference.slot == status.getTrackSourceSlot() && track.trackReference.rekordboxId == status.getRekordboxId() && track.trackType == status.getTrackType();
    }
    static JSONObject emptyDeck(int id) {
        JSONObject deck = new JSONObject().put("id", id).put("loaded", false).put("metadataAvailable", false);
        for (String field : List.of("trackId","title","artist","album","key","genre","label","bpm","originalBpm","coverUrl","positionMs","durationMs","isMaster","playing","onAir","synced","pitch","playerNumber","beatNumber")) deck.put(field, JSONObject.NULL);
        return deck;
    }
    private static synchronized void emit(JSONObject value) {
        OUTPUT.println(value);
    }
    private Set<DeviceAnnouncement> devices() { return finder.isRunning() ? finder.getCurrentDevices() : Set.of(); }
    private DeviceAnnouncement device(int number) {
        return devices().stream().filter(d -> d.getDeviceNumber() == number).findFirst().orElse(null);
    }
    private String identity(CdjStatus status) {
        DeviceAnnouncement source = device(status.getTrackSourcePlayer());
        SlotReference slot = SlotReference.getSlotReference(status.getTrackSourcePlayer(), status.getTrackSourceSlot());
        return identityKey(status, source == null ? "unknown" : source.getAddress().getHostAddress(), mediaEpochs.getOrDefault(slot, 0L));
    }
    static String identityKey(CdjStatus status, String address, long epoch) {
        return address + ":" + status.getTrackSourcePlayer() + ":" + status.getTrackSourceSlot() + ":" + epoch + ":" + status.getTrackType() + ":" + status.getRekordboxId();
    }
    private synchronized int trackId(String identity) {
        if (trackIds.size() >= 999_999 && !trackIds.containsKey(identity)) throw new IllegalStateException("Session track limit reached");
        return trackIds.computeIfAbsent(identity, key -> idBase + trackIds.size() + 1);
    }
    private void artwork(int player, int id, TrackMetadata track) {
        if (sentArt.contains(id) || !ArtFinder.getInstance().isRunning()) return;
        AlbumArt art = ArtFinder.getInstance().getLatestArtFor(player);
        if (art == null || art.artReference.player != track.trackReference.player || art.artReference.slot != track.trackReference.slot || art.artReference.rekordboxId != track.getArtworkId()) return;
        ByteBuffer buffer = art.getRawBytes();
        if (buffer.remaining() < 8 || buffer.remaining() > 2 * 1024 * 1024) return;
        byte[] bytes = new byte[buffer.remaining()]; buffer.get(bytes);
        String mime = bytes[0] == (byte)0xff && bytes[1] == (byte)0xd8 ? "image/jpeg" :
            bytes[0] == (byte)0x89 && bytes[1] == 'P' && bytes[2] == 'N' && bytes[3] == 'G' ? "image/png" : "";
        if (mime.isEmpty()) return;
        emit(new JSONObject().put("type", "cover").put("trackId", id).put("mime", mime).put("data", Base64.getEncoder().encodeToString(bytes)));
        sentArt.add(id);
    }
    private JSONObject deck(int id, int player, long now, boolean mixerOnline) {
        JSONObject deck = emptyDeck(id).put("playerNumber", player);
        DeviceAnnouncement announcement = device(player);
        if (!connected || !cdj.isRunning() || announcement == null || !announcement.getAddress().getHostAddress().equals(selectedAddresses.get(player))) return deck;
        DeviceUpdate update = cdj.getLatestStatusFor(announcement);
        if (!(update instanceof CdjStatus status) || !fresh(status, now)) return deck;
        deck.put("playing", status.isPlaying()).put("synced", status.isSynced()).put("onAir", mixerOnline ? status.isOnAir() : JSONObject.NULL)
            .put("pitch", (status.getPitch() / 1048576.0 - 1) * 100).put("firmware", text(status.getFirmwareVersion()));
        if (!status.isTrackLoaded() || status.getRekordboxId() <= 0) { lastTracks.remove(player); loadedAt.remove(player); return deck; }
        String identity = identity(status);
        if (!identity.equals(lastTracks.put(player, identity))) loadedAt.put(player, now);
        int trackId = trackId(identity);
        deck.put("loaded", true).put("trackId", trackId).put("isMaster", status.isTempoMaster())
            .put("sourceTrackId", status.getRekordboxId()).put("sourcePlayer", status.getTrackSourcePlayer()).put("sourceSlot", status.getTrackSourceSlot().name())
            .put("beatNumber", status.getBeatNumber() > 0 ? status.getBeatNumber() : JSONObject.NULL)
            .put("bpm", status.getBpm() > 0 && status.getBpm() != 65535 ? status.getEffectiveTempo() : JSONObject.NULL)
            .put("coverUrl", "/api/decks/" + id + "/cover?trackId=" + trackId);
        TrackMetadata track = metadata.isRunning() ? metadata.getLatestMetadataFor(player) : null;
        if (matches(track, status)) {
            deck.put("metadataAvailable", true).put("title", text(track.getTitle())).put("artist", text(track.getArtist()))
                .put("album", text(track.getAlbum())).put("key", text(track.getKey())).put("genre", text(track.getGenre())).put("label", text(track.getLabel()))
                .put("durationMs", track.getDuration() > 0 ? track.getDuration() * 1000L : JSONObject.NULL)
                .put("originalBpm", track.getTempo() > 0 ? track.getTempo() / 100.0 : JSONObject.NULL);
            artwork(player, trackId, track);
        }
        if (TimeFinder.getInstance().isRunning()) {
            TrackPositionUpdate position = TimeFinder.getInstance().getLatestPositionFor(player);
            // Do not transfer the previous track's position across a load. Wait for a new report.
            if (position != null && position.timestamp >= loadedAt.getOrDefault(player, now) && now - position.timestamp < 3_000_000_000L) {
                long time = TimeFinder.getInstance().getTimeFor(player);
                if (time >= 0) deck.put("positionMs", time).put("positionPrecise", position.precise);
            }
        }
        return deck;
    }
    private void publish() {
        try {
            long now = System.nanoTime();
            boolean active = connected && cdj.isRunning();
            boolean mixerOnline = active && devices().stream().anyMatch(d -> d.getDeviceName().equals("DJM-A9") && fresh(cdj.getLatestStatusFor(d), now));
            JSONArray list = new JSONArray(), decks = new JSONArray();
            for (DeviceAnnouncement device : devices().stream().sorted(Comparator.comparingInt(DeviceAnnouncement::getDeviceNumber)).toList()) {
                DeviceUpdate update = active ? cdj.getLatestStatusFor(device) : null;
                JSONObject item = new JSONObject().put("number", device.getDeviceNumber()).put("name", device.getDeviceName()).put("address", device.getAddress().getHostAddress())
                    .put("kind", device.getDeviceName().startsWith("DJM") ? "mixer" : "player").put("supported", supportedPlayer(device) || device.getDeviceName().equals("DJM-A9"))
                    .put("selectable", supportedPlayer(device)).put("selected", players.contains(device.getDeviceNumber()))
                    .put("online", true).put("fresh", fresh(update, now)).put("ageMs", Math.max(0, System.currentTimeMillis() - device.getTimestamp()));
                if (update instanceof CdjStatus status && fresh(status, now)) item.put("firmware", text(status.getFirmwareVersion()))
                    .put("playing", status.isPlaying()).put("onAir", mixerOnline ? status.isOnAir() : JSONObject.NULL).put("synced", status.isSynced())
                    .put("master", status.isTempoMaster()).put("bpm", status.getBpm() > 0 && status.getBpm() != 65535 ? status.getEffectiveTempo() : JSONObject.NULL);
                list.put(item);
            }
            List<Integer> selected = players;
            int masters = 0, master = 0, reporting = 0;
            long age = Long.MAX_VALUE;
            for (int id = 1; id <= 4; id++) {
                JSONObject deck = id <= selected.size() ? deck(id, selected.get(id - 1), now, mixerOnline) : emptyDeck(id);
                if (deck.optBoolean("isMaster", false)) { masters++; master = id; }
                if (!deck.isNull("playing")) {
                    reporting++;
                    DeviceUpdate update = cdj.getLatestStatusFor(selected.get(id - 1));
                    if (update != null) age = Math.min(age, Math.max(0, (now - update.getTimestamp()) / 1_000_000));
                }
                decks.put(deck);
            }
            if (active && cdj.getLatestStatus().stream().filter(update -> fresh(update, now) && update.isTempoMaster()).count() != 1) masters = 0;
            if (masters != 1) for (int i = 0; i < 4; i++) decks.getJSONObject(i).put("isMaster", JSONObject.NULL);
            String status = active && reporting > 0 ? "connected" : phase.equals("error") ? "error" : phase.equals("connecting") ? "starting" : "disconnected";
            String detail = active ? reporting == 0 ? "prolinkWaitingStatus" : "prolinkConnected" : message;
            JSONObject setup = new JSONObject().put("status", active ? "connected" : phase).put("message", detail).put("devices", list).put("players", selected)
                .put("localAddress", localAddress).put("networkInterface", networkInterface).put("virtualPlayer", active ? (cdj.getDeviceNumber() & 255) : JSONObject.NULL);
            emit(new JSONObject().put("type", "snapshot").put("schemaVersion", 1).put("mode", "prolink").put("demo", false)
                .put("status", status).put("message", detail).put("version", "PRO DJ LINK · Beat Link 8.0.0")
                .put("updatedAt", age == Long.MAX_VALUE ? JSONObject.NULL : System.currentTimeMillis() - age).put("sampleAgeMs", age == Long.MAX_VALUE ? JSONObject.NULL : age)
                .put("artworkStatus", "prolinkMetadataHelp").put("masterDeckId", masters == 1 ? master : JSONObject.NULL).put("decks", decks).put("setup", setup));
        } catch (Exception exception) {
            // A concurrent lifecycle transition can invalidate finder snapshots; never invent live data.
            emit(new JSONObject().put("type", "snapshot").put("schemaVersion", 1).put("mode", "prolink").put("demo", false).put("status", "disconnected")
                .put("message", "prolinkWaitingStatus").put("version", "PRO DJ LINK").put("updatedAt", JSONObject.NULL).put("sampleAgeMs", JSONObject.NULL)
                .put("artworkStatus", "prolinkMetadataHelp").put("masterDeckId", JSONObject.NULL).put("decks", new JSONArray(List.of(emptyDeck(1),emptyDeck(2),emptyDeck(3),emptyDeck(4))))
                .put("setup", new JSONObject().put("status", phase).put("message", message).put("devices", new JSONArray()).put("players", players)));
        }
    }
    private void disconnect() {
        connected = false;
        TimeFinder.getInstance().stop(); ArtFinder.getInstance().stop(); CrateDigger.getInstance().stop();
        BeatGridFinder.getInstance().stop(); metadata.stop(); BeatFinder.getInstance().stop(); cdj.stop(); finder.stop();
        lastTracks.clear(); loadedAt.clear(); mediaEpochs.replaceAll((key, value) -> value + 1);
        players = List.of(); selectedAddresses.clear(); localAddress = networkInterface = "";
        phase = "stopped"; message = "prolinkStopped";
    }
    private void command(JSONObject command) throws Exception {
        String action = command.getString("action");
        if (action.equals("disconnect")) { disconnect(); return; }
        if (action.equals("discover")) {
            if (!connected) { finder.start(); phase = "discovering"; message = "prolinkDiscovering"; }
            return;
        }
        if (!action.equals("connect")) throw new IllegalArgumentException("Invalid action");
        if (connected) throw new IllegalArgumentException("Disconnect before changing players");
        JSONArray chosen = command.getJSONArray("players");
        if (chosen.isEmpty() || chosen.length() > 4) throw new IllegalArgumentException("Select 1 to 4 players");
        List<Integer> selected = new ArrayList<>();
        for (int index = 0; index < chosen.length(); index++) {
            int player = chosen.getInt(index); DeviceAnnouncement device = device(player);
            if (selected.contains(player) || device == null || !supportedPlayer(device)) throw new IllegalArgumentException("Player unavailable");
            // Duplicate numbers make unicast state ambiguous: refuse to attach.
            if (devices().stream().filter(d -> d.getDeviceNumber() == player).count() != 1) throw new IllegalArgumentException("Duplicate player number");
            selected.add(player); selectedAddresses.put(player, device.getAddress().getHostAddress());
        }
        if (devices().stream().anyMatch(d -> !supportedPlayer(d) && !d.getDeviceName().equals("DJM-A9"))) {
            message = "prolinkUnsupportedNetwork"; phase = "error"; return;
        }
        selected.sort(Integer::compareTo);
        players = List.copyOf(selected); phase = "connecting"; message = "prolinkConnecting";
        cdj.setDeviceName("DeckStatus"); cdj.setUseStandardPlayerNumber(false);
        if (!cdj.start()) throw new IOException("Unable to join network");
        if (cdj.getMatchingInterfaces().size() != 1 || !cdj.findUnreachablePlayers().isEmpty()) {
            cdj.stop(); phase = "error"; message = "prolinkAmbiguousNetwork"; return;
        }
        localAddress = cdj.getLocalAddress().getHostAddress();
        networkInterface = cdj.getMatchingInterfaces().get(0).getDisplayName();
        // Never enable VirtualCdj status sending, sync, tempo control, loading or fader-start commands.
        metadata.setPassive(false); metadata.start(); CrateDigger.getInstance().start(); ArtFinder.getInstance().start(); TimeFinder.getInstance().start();
        connected = true; phase = "connected"; message = "prolinkConnected";
    }
    public static void main(String[] args) throws Exception {
        int idBase = args.length == 1 ? Integer.parseInt(args[0]) : 0;
        if (idBase < 0 || idBase > 2_000_000_000) throw new IllegalArgumentException("Invalid identity base");
        Main app = new Main(idBase);
        app.timer.scheduleAtFixedRate(app::publish, 0, 100, TimeUnit.MILLISECONDS);
        try (BufferedReader input = new BufferedReader(new InputStreamReader(System.in, StandardCharsets.UTF_8))) {
            String line;
            while ((line = input.readLine()) != null) {
                if (line.length() > 1024) continue;
                try { app.command(new JSONObject(line)); }
                catch (Exception exception) {
                    if (!app.connected && app.cdj.isRunning()) app.cdj.stop();
                    app.phase = "error"; app.message = exception instanceof BindException ? "prolinkPortsBusy" : "prolinkConnectionFailed";
                }
            }
        } finally { app.timer.shutdownNow(); app.disconnect(); System.exit(0); }
    }
}
