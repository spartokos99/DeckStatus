package com.deckstatus.prolink;

import org.deepsymmetry.beatlink.DeviceAnnouncement;

/** Explicit PRO DJ LINK profiles; a familiar prefix is not sufficient for compatibility. */
final class DeviceSupport {
    private DeviceSupport() {}

    static boolean player(DeviceAnnouncement device) {
        return device.getDeviceNumber() >= 1 && device.getDeviceNumber() <= 6 &&
            switch (device.getDeviceName()) {
                case "CDJ-3000", "CDJ-3000X", "XDJ-AZ" -> true;
                default -> false;
            };
    }

    static boolean mixer(DeviceAnnouncement device) {
        return switch (device.getDeviceName()) {
            case "DJM-A9", "DJM-900NXS2" -> true;
            default -> false;
        };
    }

    static boolean supported(DeviceAnnouncement device) { return player(device) || mixer(device); }

    static String guidance(DeviceAnnouncement device) {
        return switch (device.getDeviceName()) {
            case "XDJ-AZ" -> "prolinkAzHelp";
            case "CDJ-3000X" -> "prolink3000xHelp";
            case "DJM-A9", "DJM-900NXS2" -> "prolinkMixerHelp";
            default -> "";
        };
    }
}
