/* sherpa.r: GlobalTalk Sherpa's menus, dialogs and Finder resources.
 * Dialogs fit a 512x342 screen (Plus, SE, Mac XL). Icons are in icons.r (from tools/mkicons.py). */
#include "Multiverse.r"
#include "Processes.r"
#include "icons.r"

resource 'MBAR' (128) { { 128; 129; 130; 131; 132 } };

resource 'MENU' (128) { 128, textMenuProc, 0x7FFFFFFD, enabled, apple, {
    "About GlobalTalk Sherpa\0xC9", noIcon, noKey, noMark, plain;
    "-", noIcon, noKey, noMark, plain
} };

resource 'MENU' (129) { 129, textMenuProc, 0x7FFFFFFF, enabled, "File", {
    "Open Scan\0xC9", noIcon, "O", noMark, plain;
    "-", noIcon, noKey, noMark, plain;
    "Save Map as Picture\0xC9", noIcon, "S", noMark, plain;
    "Save List as Text\0xC9", noIcon, "T", noMark, plain;
    "-", noIcon, noKey, noMark, plain;
    "Export Icon Pack\0xC9", noIcon, noKey, noMark, plain;
    "Import Icon Pack\0xC9", noIcon, noKey, noMark, plain;
    "-", noIcon, noKey, noMark, plain;
    "Quit", noIcon, "Q", noMark, plain
} };

resource 'MENU' (130) { 130, textMenuProc, 0x7FFFFFFF, enabled, "Edit", {
    "Undo", noIcon, "Z", noMark, plain;
    "-", noIcon, noKey, noMark, plain;
    "Cut", noIcon, "X", noMark, plain;
    "Copy", noIcon, "C", noMark, plain;
    "Paste", noIcon, "V", noMark, plain;
    "Clear", noIcon, noKey, noMark, plain;
    "-", noIcon, noKey, noMark, plain;
    "Copy Icon", noIcon, noKey, noMark, plain;
    "Paste Icon for This Device", noIcon, noKey, noMark, plain;
    "Paste Icon for This Model", noIcon, noKey, noMark, plain;
    "Remove Custom Icon", noIcon, noKey, noMark, plain
} };

resource 'MENU' (131) { 131, textMenuProc, 0x7FFFFFFF, enabled, "Scan", {
    "Scan Local Network", noIcon, "L", noMark, plain;
    "Scan All of GlobalTalk", noIcon, "G", noMark, plain;
    "Stop Scan", noIcon, ".", noMark, plain;
    "-", noIcon, noKey, noMark, plain;
    "Local Network Zones\0xC9", noIcon, noKey, noMark, plain;
    "-", noIcon, noKey, noMark, plain;
    "No Automatic Rescan", noIcon, noKey, noMark, plain;
    "Rescan Every 15 Minutes", noIcon, noKey, noMark, plain;
    "Rescan Every 30 Minutes", noIcon, noKey, noMark, plain;
    "Rescan Every Hour", noIcon, noKey, noMark, plain
} };

resource 'MENU' (132) { 132, textMenuProc, 0x7FFFFFFF, enabled, "Map", {
    "All Zones", noIcon, "1", noMark, plain;
    "Network Diagram", noIcon, "2", noMark, plain;
    "Dashboard", noIcon, "3", noMark, plain;
    "-", noIcon, noKey, noMark, plain;
    "Back Out", noIcon, "U", noMark, plain
} };

/* About. Line breaks are \0x0D: Rez turns "\n" into a linefeed, which the Toolbox draws as a box
 * (Tim's photo, 26 Oct). The version here is updated with the vers resource at each build. */
resource 'ALRT' (128) { { 40, 40, 250, 470 }, 128, {
    OK, visible, silent; OK, visible, silent; OK, visible, silent; OK, visible, silent
}, centerMainScreen };

resource 'DITL' (128) { {
    { 180, 340, 200, 410 }, Button { enabled, "OK" };
    { 12, 12, 44, 44 }, Icon { disabled, 128 };
    { 12, 60, 30, 410 }, StaticText { disabled, "GlobalTalk Sherpa 1.0.2" };
    { 34, 60, 172, 410 }, StaticText { disabled,
        "Finds the AppleTalk zones and devices on your network and on GlobalTalk, "
        "and maps them. Click a zone or a device to look closer.\0x0D\0x0D"
        "Reliability comes from how often a device answered earlier scans.\0x0D"
        "Made for Marchintosh 2027 by Tim O'Connor." };
} };

/* a one-line message: ^0 */
resource 'ALRT' (130) { { 80, 60, 190, 452 }, 130, {
    OK, visible, sound1; OK, visible, sound1; OK, visible, sound1; OK, visible, sound1
}, alertPositionMainScreen };

resource 'DITL' (130) { {
    { 80, 312, 100, 382 }, Button { enabled, "OK" };
    { 10, 60, 74, 382 }, StaticText { disabled, "^0" };
} };

/* Local Network Zones (called lab zones inside the code and prefs) */
resource 'DLOG' (129) { { 60, 40, 232, 472 }, dBoxProc, invisible, noGoAway, 0, 129, "", centerMainScreen };

resource 'DITL' (129) { {
    { 140, 340, 160, 420 }, Button { enabled, "OK" };
    { 140, 250, 160, 330 }, Button { enabled, "Cancel" };
    { 140, 12, 160, 150 }, Button { enabled, "Ask the Router" };
    { 72, 15, 88, 417 }, EditText { enabled, "" };
    { 96, 12, 130, 420 }, StaticText { disabled, "" };
    { 10, 12, 64, 420 }, StaticText { disabled,
        "These are the zones Scan Local Network scans. Separate names with commas. "
        "Leave it empty and Sherpa asks your router each time: the zones of the "
        "networks plugged straight into it." };
} };

/* Finder */
data 'GTSh' (0, "Owner resource") { $"1547 6C6F 6261 6C54 616C 6B20 5368 6572 7061 2030 2E31" };
resource 'FREF' (128) { 'APPL', 0, "" };
resource 'FREF' (129) { 'GTSi', 1, "" };
resource 'BNDL' (128) { 'GTSh', 0, { 'ICN#', { 0, 128; 1, 129 }; 'FREF', { 0, 128; 1, 129 } } };

resource 'vers' (1) { 0x01, 0x02, 0x80, 0x00, verUS, "1.0.2", "1.0.2, GlobalTalk Sherpa, Marchintosh 2027" };

resource 'SIZE' (-1) {
    reserved, acceptSuspendResumeEvents, reserved, canBackground, doesActivateOnFGSwitch,
    backgroundAndForeground, dontGetFrontClicks, ignoreChildDiedEvents, is32BitCompatible,
    notHighLevelEventAware, onlyLocalHLEvents, notStationeryAware, dontUseTextEditServices,
    reserved, reserved, reserved,
    448 * 1024,
    320 * 1024
};
