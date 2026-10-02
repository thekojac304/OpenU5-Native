// Alpha 4 A4-POLISH3 (ALPHA4_UI.md section 13) -- the keyboard backlight in
// the portable core and the device's pure write policy.
//
//   a4_polish3_keyboard_light
//
//   L  the five levels, their names and the duty each one sends
//   J  settings.json: the key, a round trip, an older card, a bad value
//   P  the capture task's write policy: once per change, bounded retries,
//      a rewrite after the keyboard is reinitialized
//   M  the "Keyboard Backlight" row on both Settings pages
#include "keyboard_backlight.h"
#include "openu5/frontend.h"
#include "openu5/frontend_settings.h"
#include "openu5/system_menu.h"

#include <cstdio>
#include <cstring>
#include <string>

using namespace openu5;

namespace {
int checks = 0, failures = 0;
void check(bool ok, const char *id, const std::string &label) {
    ++checks;
    if (!ok) ++failures;
    std::printf("%s %s %s\n", ok ? "GREEN" : "RED", id, label.c_str());
}

UiAction act(UiActionKind k, Direction d = Direction::North) {
    UiAction a{};
    a.kind = k;
    a.direction = d;
    return a;
}
const UiAction kNext = act(UiActionKind::Next), kConfirm = act(UiActionKind::Confirm), kBack = act(UiActionKind::Back),
               kEast = act(UiActionKind::Direction, Direction::East), kWest = act(UiActionKind::Direction, Direction::West);

void test_levels() {
    bool names = kKeyboardBacklightLevels == 5 && tdeck::kKeyboardBacklightLevelCount == kKeyboardBacklightLevels;
    const char *expected[] = {"Off", "Low", "Medium", "High", "Max"};
    for (uint8_t i = 0; i < 5; ++i) names = names && std::strcmp(keyboard_backlight_name(i), expected[i]) == 0;
    check(names && std::strcmp(keyboard_backlight_name(9), "Off") == 0, "L1",
          "five levels Off/Low/Medium/High/Max; an out-of-range level reads Off");
    bool rising = tdeck::kKeyboardBacklightDuty[0] == 0 && tdeck::kKeyboardBacklightDuty[4] == 255 &&
                  tdeck::kKeyboardBacklightDuty[2] == 127 && tdeck::kKeyboardBacklightDuty[1] > 30;
    for (int i = 1; i < 5; ++i) rising = rising && tdeck::kKeyboardBacklightDuty[i] > tdeck::kKeyboardBacklightDuty[i - 1];
    check(rising, "L2",
          "duties 0 < Low (above the sketch's 30) < Medium 127 (the sketch's Alt+B default) < High < Max 255");
    uint8_t frame[3] = {0, 0, 0xee};
    tdeck::keyboard_backlight_frame(3, frame);
    const bool high = frame[0] == 0x01 && frame[1] == tdeck::kKeyboardBacklightDuty[3] && frame[2] == 0xee;
    tdeck::keyboard_backlight_frame(0, frame);
    const bool off = frame[0] == 0x01 && frame[1] == 0;
    tdeck::keyboard_backlight_frame(200, frame);
    check(high && off && frame[1] == 0 && kKeyboardBacklightDefault == 0, "L3",
          "the frame is exactly 0x01 <duty> (two bytes, nothing after); a bad level sends Off; the default is Off");
}

void test_json() {
    FrontendSettings s{};
    std::string text;
    check(s.keyboard_backlight == 0 && encode_settings(s, text) &&
              text.find("\"keyboardBacklight\":0") != std::string::npos,
          "J1", "a default settings.json stores \"keyboardBacklight\":0 (Off)");
    bool trip = true;
    for (uint8_t level = 0; level < kKeyboardBacklightLevels; ++level) {
        FrontendSettings w{}, r{};
        w.keyboard_backlight = level;
        w.sound_volume = 30;
        trip = trip && encode_settings(w, text) && decode_settings(text, r) && r.keyboard_backlight == level &&
               r.sound_volume == 30;
    }
    check(trip, "J2", "every level survives encode -> decode (a reboot), with the other settings");
    // An Alpha 4 A4-ENH2 card, written before the key existed.
    const std::string old_card = "{\"version\":1,\"brightness\":60,\"movementMode\":true,\"trackballResponsiveness\":100,"
                                 "\"uiSize\":1,\"developerToolsVisible\":false,\"soundVolume\":40,\"musicVolume\":70,"
                                 "\"touchControls\":false,\"trackballSpeed\":7}";
    FrontendSettings old{};
    old.keyboard_backlight = 4;
    check(decode_settings(old_card, old) && old.keyboard_backlight == 0 && old.brightness == 60 &&
              old.trackball_speed == 7 && old.sound_volume == 40 && old.movement_mode,
          "J3", "an older settings.json without the key loads, keeps every value, and takes Off");
    bool bad = true;
    for (const char *v : {"5", "-1", "2.5", "\"High\"", "true", "255"}) {
        std::string doc = old_card;
        doc.insert(doc.size() - 1, std::string(",\"keyboardBacklight\":") + v);
        FrontendSettings r{};
        r.keyboard_backlight = 3;
        bad = bad && decode_settings(doc, r) && r.keyboard_backlight == 0 && r.trackball_speed == 7;
    }
    check(bad, "J4", "an out-of-range or non-integer level keeps the document and reads Off");
}

void test_policy() {
    tdeck::KeyboardBacklightPolicy p{};
    const bool none = !p.write_due(tdeck::KeyboardBacklightPolicy::kNone);
    const bool boot = p.write_due(0);
    p.write_finished(0, true);
    check(none && boot && !p.write_due(0), "P1",
          "nothing asked: no write; boot's saved level (even Off): one write; the same level again: none");
    p.write_finished(2, true);
    const bool same = !p.write_due(2);
    const bool changed = p.write_due(4);
    check(same && changed, "P2", "an unchanged level writes nothing; a changed one is due at once");
    int writes = 0;
    for (int pass = 0; pass < 10; ++pass)
        if (p.write_due(4)) {
            ++writes;
            p.write_finished(4, false);
        }
    check(writes == tdeck::KeyboardBacklightPolicy::kMaxAttempts && writes == 3, "P3",
          "a failing write is tried 3 times over ten service passes, then left alone (no bus flood)");
    const bool other = p.write_due(1);
    p.write_finished(1, true);
    const bool back = p.write_due(4);
    check(other && !p.write_due(1) && back, "P4",
          "after giving up, a new level is due; and so is the given-up level when chosen again");
    p.write_finished(4, true);
    const bool settled = !p.write_due(4);
    p.keyboard_reinitialized();
    check(settled && p.write_due(4), "P5",
          "raw mode re-entered after a recovery (the C3 may have reset dark): the same level is written again");
    p.write_finished(4, false);
    p.keyboard_reinitialized();
    check(p.write_due(4) && p.attempts == 0, "P6", "a reinitialization also restores the retry budget");
}

void open_settings(SystemMenuSession &m, const FrontendSettings &s) {
    m.open(s, FrontendSaveCatalog{});
    for (int i = 0; i < 3; ++i) m.handle(kNext);
    m.handle(kConfirm);
}

void test_menus() {
    SystemMenuSession m;
    open_settings(m, FrontendSettings{});
    auto v = m.view();
    check(SystemMenuSession::kKeyboardLightRow == 6 && SystemMenuSession::kSettingsRowCount == 7 &&
              v.line_count == 7 && std::strcmp(v.lines[6], "Keyboard Backlight: Off") == 0 &&
              std::strcmp(v.lines[5], "Music Volume: Unavailable") == 0,
          "M1", "System Menu > Settings: a seventh row, \"Keyboard Backlight: Off\", after the six it had");
    for (int i = 0; i < 6; ++i) m.handle(kNext);
    std::string seen;
    for (int i = 0; i < 4; ++i) {
        m.handle(kEast);
        seen += std::string(m.view().lines[6]) + "|";
    }
    const std::string footer = m.view().footer ? m.view().footer : "";
    check(seen == "Keyboard Backlight: Low|Keyboard Backlight: Medium|Keyboard Backlight: High|Keyboard Backlight: Max|" &&
              m.settings().keyboard_backlight == 4 && footer == "Left/right: Off to Max; Mic saves",
          "M2", "Right steps Low, Medium, High, Max; the footer names the range: " + seen);
    m.handle(kEast);
    const bool wrap_up = m.settings().keyboard_backlight == 0;
    m.handle(kWest);
    const bool wrap_down = m.settings().keyboard_backlight == 4;
    m.handle(kConfirm);
    check(wrap_up && wrap_down && m.settings().keyboard_backlight == 0, "M3",
          "the row cycles like Text / UI: Max -> Off on Right, Off -> Max on Left, Enter steps forward");
    m.handle(kEast);
    m.handle(kEast);
    m.handle(kBack);
    const auto intent = m.take_intent();
    check(intent.kind == SystemMenuIntentKind::PersistSettings && intent.settings.keyboard_backlight == 2 &&
              intent.settings.brightness == FrontendSettings{}.brightness,
          "M4", "leaving Settings persists the level (Medium); the display brightness is untouched");

    SystemMenuSession u;
    u.set_keyboard_light_available(false);
    open_settings(u, FrontendSettings{});
    for (int i = 0; i < 6; ++i) u.handle(kNext);
    u.handle(kEast);
    const std::string missing = u.view().footer ? u.view().footer : "";
    check(missing == "No keyboard found; saved for next boot" && u.settings().keyboard_backlight == 1 &&
              missing.size() <= 40,
          "M5", "no keyboard at boot: the row still edits and saves, and the footer says why: " + missing);

    FrontendSession title;
    title.start(0, false, {});
    UiAction s{};
    s.kind = UiActionKind::Character;
    s.character = u's';
    title.handle(kConfirm, 1);
    title.handle(s, 2);
    for (int i = 0; i < 6; ++i) title.handle(kNext, 3);
    title.handle(kEast, 3);
    title.handle(kEast, 3);
    title.handle(kEast, 3);
    const auto tv = title.view();
    check(title.state() == FrontendState::Settings && tv.line_count == 7 && tv.selected_line == 6 &&
              std::strcmp(tv.lines[6], "Keyboard Backlight: High") == 0 && title.settings().keyboard_backlight == 3 &&
              tv.footer && std::strcmp(tv.footer, "Left/right: Off to Max; Mic saves") == 0,
          "M6", "the title's Settings page has the same seventh row and footer");
    FrontendSession absent;
    absent.set_keyboard_light_available(false);
    absent.start(0, false, {});
    absent.handle(kConfirm, 1);
    absent.handle(s, 2);
    for (int i = 0; i < 6; ++i) absent.handle(kNext, 3);
    check(absent.view().footer && std::strcmp(absent.view().footer, "No keyboard found; saved for next boot") == 0,
          "M7", "the title says the same when no keyboard answered");
}
} // namespace

int main() {
    test_levels();
    test_json();
    test_policy();
    test_menus();
    std::printf("\nA4-POLISH3 keyboard light: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
