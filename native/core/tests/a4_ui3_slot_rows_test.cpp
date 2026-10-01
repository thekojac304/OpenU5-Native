// Alpha 4 A4-UI3 (targets/tdeck/ALPHA4_UI.md section 6) -- the save/load UX in
// the core: the shared slot rows, the CURRENT / LATEST / RECOVERED tags, the
// Save, Load and Overwrite flows of the System Menu, the title's Journey
// Onward / Load Game pages and the PC Save Transfer slot pickers. Pure
// sessions over hand-built catalogs; the device half (two rows a slot on the
// panel, the real card, the transcript) is a4_ui3_save_ux_runtime.
#include "openu5/frontend.h"
#include "openu5/system_menu.h"

#include <cstdio>
#include <cstring>
#include <string>

using namespace openu5;

namespace {
int g_checks = 0, g_failures = 0;
void check(bool ok, const char *id, const std::string &what) {
    ++g_checks;
    if (!ok) ++g_failures;
    std::printf("  %-5s %-5s %s\n", ok ? "GREEN" : "RED", id, what.c_str());
}
std::string q(const char *s) { return std::string("\"") + (s ? s : "(null)") + "\""; }
std::string str(const char *s) { return s ? s : ""; }

UiAction act(UiActionKind k) { UiAction a{}; a.kind = k; return a; }
UiAction ch(char c) { UiAction a{}; a.kind = UiActionKind::Character; a.character = uint16_t(uint8_t(c)); return a; }
const UiAction kUp = act(UiActionKind::Previous), kDown = act(UiActionKind::Next), kEnter = act(UiActionKind::Confirm),
               kBack = act(UiActionKind::Back), kMic = act(UiActionKind::Cancel);

void fill(FrontendSaveCatalog::Entry &e, SaveSlotStatus st, uint64_t seq, const char *name, const char *place) {
    e.status = st;
    e.sequence = seq;
    e.shown = FrontendSaveSlot{};
    if (st == SaveSlotStatus::Saved || st == SaveSlotStatus::Recovered) {
        e.shown.present = e.shown.valid = true;
        e.shown.sequence = seq;
        std::snprintf(e.shown.name, sizeof(e.shown.name), "%s", name);
        std::snprintf(e.shown.place, sizeof(e.shown.place), "%s", place);
        e.shown.year = 139; e.shown.month = 4; e.shown.day = 5; e.shown.hour = 12; e.shown.party = 2;
    }
}
// Kojac (Slot 1, seq 4), Avery (Slot 2, seq 7: the newest), Iolo (Slot 3, seq 5).
FrontendSaveCatalog three() {
    FrontendSaveCatalog c{};
    fill(c.slots[0], SaveSlotStatus::Saved, 4, "Kojac", "Lord British's Castle");
    fill(c.slots[1], SaveSlotStatus::Saved, 7, "Avery", "Britannia");
    fill(c.slots[2], SaveSlotStatus::Saved, 5, "Iolo", "Iolo's Hut");
    return c;
}
std::string row(const FrontendSaveCatalog &c, int slot, int marked = -1, const char *tag = nullptr) {
    char r[96], d[96];
    format_slot_rows(r, d, sizeof(r), c, slot, marked, tag);
    return std::string(r) + "|" + d;
}
std::string page(const FrontendView &v) {
    std::string out;
    for (size_t i = 0; i < v.line_count; ++i)
        out += (i ? " ; " : "") + str(v.lines[i]) + "|" + str(i < size_t(kSaveSlotCount) ? v.details[i] : nullptr);
    return out;
}
std::string identity(const FrontendSaveCatalog &c, int slot) {
    char out[96];
    format_slot_identity(out, sizeof(out), c, slot);
    return out;
}

void test_rows() {
    std::printf("\n[R] the slot rows\n");
    auto c = three();
    check(row(c, 0) == "Slot 1  Kojac|        Lord British's Castle" && row(c, 1) == "Slot 2  Avery|        Britannia" &&
              row(c, 2) == "Slot 3  Iolo|        Iolo's Hut",
          "R1", "three occupied slots: \"Slot N  <leader>\" over the place, indented under the name");
    fill(c.slots[2], SaveSlotStatus::Empty, 0, "", "");
    check(row(c, 2) == "Slot 3  EMPTY|", "R2", "an empty slot: EMPTY and no metadata at all (" + q(row(c, 2).c_str()) + ")");
    FrontendSaveCatalog stale = three();
    stale.slots[2].status = SaveSlotStatus::Empty;   // a summary left behind must not leak into an empty row
    check(row(stale, 2) == "Slot 3  EMPTY|", "R2b", "an empty slot shows no leftover name or place");
    FrontendSaveCatalog l{};
    fill(l.slots[0], SaveSlotStatus::Recovered, 9, "Shamino1", "Lord British's Castle");
    const std::string longest = row(l, 0, 0, kCurrentSlotTag);
    const size_t bar = longest.find('|');
    check(longest == "Slot 1  Shamino1  CURRENT, RECOVERED|        Lord British's Castle" && bar <= kSaveRowChars &&
              longest.size() - bar - 1 <= kSaveRowChars,
          "R3", "an 8-letter name with both tags and the longest place each fit 36 cells: " + q(longest.c_str()));
    fill(l.slots[0], SaveSlotStatus::Saved, 9, "Shamino12", "Palace of Blackthorn and more");
    const std::string clipped = row(l, 0, 0, kCurrentSlotTag);
    check(clipped == "Slot 1  Shamino1  CURRENT|        Palace of Blackthorn an" &&
              clipped.find('|') <= kSaveRowChars && clipped.size() - clipped.find('|') - 1 <= kSaveRowChars,
          "R4", "an over-long name keeps 8 letters; a place holds 23 characters, so its row is at most 31 cells: " +
                    q(clipped.c_str()));
    c = three();
    check(row(c, 1, 1, kCurrentSlotTag) == "Slot 2  Avery     CURRENT|        Britannia" &&
              row(c, 0, 1, kCurrentSlotTag) == "Slot 1  Kojac|        Lord British's Castle" &&
              row(c, 1, -1, kCurrentSlotTag) == "Slot 2  Avery|        Britannia",
          "R5", "the tag marks the marked slot only, and no slot when none is marked");
    c.slots[1].status = SaveSlotStatus::Recovered;
    check(row(c, 1) == "Slot 2  Avery     RECOVERED|        Britannia", "R6",
          "a recovered slot is tagged RECOVERED and still shows what loads: " + q(row(c, 1).c_str()));
    c.slots[2].status = SaveSlotStatus::Damaged;
    check(row(c, 2) == "Slot 3  DAMAGED|        Cannot be loaded" && !slot_loadable(c, 2) && slot_loadable(c, 1), "R7",
          "a slot with nothing loadable is DAMAGED (neither EMPTY nor a journey), with no metadata");
    check(identity(c, 0) == "Kojac, Lord British's Castle" && identity(c, 1) == "Avery, Britannia" &&
              identity(c, 2) == "Damaged save" && identity(FrontendSaveCatalog{}, 0) == "Empty slot",
          "R8", "the confirm pages' identity: leader and place of what a load restores");
}

void test_save() {
    std::printf("\n[S] System Menu -> Save Game\n");
    FrontendSaveCatalog c = three();
    c.slots[2] = FrontendSaveCatalog::Entry{};   // Slot 3 empty
    SystemMenuSession m;
    m.open(FrontendSettings{}, c, 1);
    m.handle(kDown);
    m.handle(kEnter);
    auto v = m.view();
    check(str(v.title) == "Save Game" && v.selected_line == 1 &&
              page(v) == "Slot 1  Kojac|        Lord British's Castle ; Slot 2  Avery     CURRENT|        Britannia ; Slot 3  EMPTY|",
          "S1", "Save Game: Slots 1-3, the live journey's slot CURRENT and selected: " + page(v));
    m.handle(kDown);
    v = m.view();
    check(v.selected_line == 2 && str(v.lines[1]) == "Slot 2  Avery     CURRENT" && str(v.lines[2]) == "Slot 3  EMPTY", "S1b",
          "CURRENT stays on the journey's slot when the cursor moves (it is not the selection)");
    m.handle(kEnter);
    auto i = m.take_intent();
    check(i.kind == SystemMenuIntentKind::Save && i.slot == 2, "S2", "an empty slot saves at once (no question)");
    m.open(FrontendSettings{}, c, 1);
    m.handle(kDown);
    m.handle(kEnter);
    m.handle(kUp);         // Slot 1 (Kojac), not the journey's
    m.handle(kEnter);
    v = m.view();
    check(str(v.title) == "Overwrite Slot 1?" && str(v.subtitle) == "Kojac, Lord British's Castle" && v.selected_line == 0 &&
              str(v.lines[0]) == "No, keep it" && m.take_intent().kind == SystemMenuIntentKind::None,
          "S3", "an occupied slot asks, naming THAT slot and its own save (" + q(v.title) + " / " + q(v.subtitle) + "), No selected");
    m.handle(kEnter);      // No
    v = m.view();
    check(m.take_intent().kind == SystemMenuIntentKind::None && str(v.title) == "Save Game" && v.selected_line == 0 &&
              str(v.footer) == "Slot 1 kept",
          "S4", "Enter on the default answer keeps the slot, back on the Save page on that slot");
    m.handle(kEnter);
    m.handle(kDown);       // Yes highlighted
    m.handle(kBack);
    v = m.view();
    check(m.take_intent().kind == SystemMenuIntentKind::None && str(v.title) == "Save Game" && v.selected_line == 0,
          "S5", "Back with Yes highlighted cancels: Back never confirms");
    m.handle(kEnter);
    m.handle(kDown);
    m.handle(kMic);
    check(m.take_intent().kind == SystemMenuIntentKind::None && str(m.view().title) == "Save Game", "S6",
          "the Mic (Cancel) with Yes highlighted cancels too");
    m.handle(kEnter);
    m.handle(kDown);
    m.handle(kEnter);
    i = m.take_intent();
    check(i.kind == SystemMenuIntentKind::Save && i.slot == 0, "S7", "Yes saves into the slot the question named (Slot 1)");
    // A damaged slot is still a slot the save may replace, after the question.
    c.slots[0].status = SaveSlotStatus::Damaged;
    m.open(FrontendSettings{}, c, -1);
    m.handle(kDown);
    m.handle(kEnter);
    v = m.view();
    check(v.selected_line == 2 && str(v.lines[1]) == "Slot 2  Avery" && str(v.lines[0]) == "Slot 1  DAMAGED", "S8",
          "with no live journey's slot nothing is CURRENT and the page starts on the first empty slot");
    m.handle(kUp); m.handle(kUp);
    m.handle(kEnter);
    check(str(m.view().title) == "Overwrite Slot 1?" && str(m.view().subtitle) == "Damaged save" && m.view().selected_line == 0,
          "S9", "replacing a damaged slot asks too (No first), naming it a damaged save");
}

void test_load() {
    std::printf("\n[L] System Menu -> Load Game\n");
    FrontendSaveCatalog c = three();
    c.slots[0].status = SaveSlotStatus::Recovered;
    c.slots[2] = FrontendSaveCatalog::Entry{};
    SystemMenuSession m;
    m.open(FrontendSettings{}, c, 1);
    m.handle(kDown); m.handle(kDown);
    m.handle(kEnter);
    auto v = m.view();
    check(str(v.title) == "Load Game" && v.selected_line == 1 &&
              page(v) == "Slot 1  Kojac     RECOVERED|        Lord British's Castle ; Slot 2  Avery     CURRENT|        Britannia ; Slot 3  EMPTY|",
          "L1", "Load Game: the Save page's rows and tags, on the journey's slot: " + page(v));
    m.handle(kDown);
    m.handle(kEnter);      // empty
    check(m.take_intent().kind == SystemMenuIntentKind::None && str(m.view().footer) == "Slot 3 is empty" && str(m.view().title) == "Load Game",
          "L2", "an empty slot refuses, politely, and the page stays");
    m.handle(kDown);       // wraps to Slot 1
    check(m.view().selected_line == 0, "L3", "Down from Slot 3 wraps to Slot 1");
    check(str(m.view().footer) == "Last save damaged; Enter loads the one before", "L4",
          "the recovered slot's footer says what Enter does");
    m.handle(kEnter);
    auto i = m.take_intent();
    check(i.kind == SystemMenuIntentKind::LoadSlot && i.slot == 0, "L5", "a recovered slot loads -- that slot (its fallback is the service's)");
    c.slots[0].status = SaveSlotStatus::Damaged;
    m.open(FrontendSettings{}, c, 1);
    m.handle(kDown); m.handle(kDown); m.handle(kEnter);
    m.handle(kUp);
    m.handle(kEnter);
    v = m.view();
    check(m.take_intent().kind == SystemMenuIntentKind::None && str(v.footer) == "Slot 1 is damaged and cannot load" &&
              str(v.lines[0]) == "Slot 1  DAMAGED",
          "L6", "a damaged slot is listed DAMAGED and refuses; nothing else is loaded in its place");
    // Manual Load never names another slot: whatever row Enter is on is the intent's slot.
    bool exact = true;
    for (int s = 0; s < kSaveSlotCount; ++s) {
        FrontendSaveCatalog all = three();
        SystemMenuSession k;
        k.open(FrontendSettings{}, all, s);
        k.handle(kDown); k.handle(kDown); k.handle(kEnter);
        k.handle(kEnter);
        const auto li = k.take_intent();
        exact = exact && li.kind == SystemMenuIntentKind::LoadSlot && li.slot == s;
    }
    check(exact, "L7", "Load's intent is always the selected slot (no cross-slot fallback is asked for)");
    m.open(FrontendSettings{}, c, 1);
    m.handle(kDown); m.handle(kDown); m.handle(kEnter);
    m.handle(kMic);
    check(m.active() && str(m.view().title) == "System Menu" && m.take_intent().kind == SystemMenuIntentKind::None, "L8",
          "the Mic on the Load page returns to the root (nothing loads)");
}

void test_title() {
    std::printf("\n[T] the title\n");
    FrontendSaveCatalog c = three();
    FrontendSession f;
    f.start(0, false, {});
    f.handle(kEnter, 1);
    f.set_save_catalog(c);
    f.handle(ch('j'), 2);
    auto v = f.view();
    check(f.state() == FrontendState::Continue && str(v.subtitle) == "Latest: Slot 2, Avery, Britannia" &&
              str(v.footer) == "Enter continues Slot 2; Mic returns",
          "T1", "Journey Onward names what Continue resumes: " + q(v.subtitle) + " / " + q(v.footer));
    f.handle(kEnter, 3);
    check(f.take_intent().kind == FrontendIntentKind::ContinueLatest, "T2", "Continue is still one Enter (ContinueLatest, no question)");
    FrontendSession g;
    g.start(0, false, {});
    g.handle(kEnter, 1);
    c.slots[1].status = SaveSlotStatus::Recovered;
    g.set_save_catalog(c);
    g.handle(ch('j'), 2);
    v = g.view();
    check(str(v.subtitle) == "Latest: Slot 2, Avery, Britannia" && str(v.footer) == "Latest save damaged; Enter loads the one before",
          "T3", "Continue on a recovered slot says the one before loads: " + q(v.footer));
    g.handle(kDown, 3);
    g.handle(kEnter, 4);
    v = g.view();
    check(g.state() == FrontendState::Load && v.selected_line == 1 &&
              page(v) == "Slot 1  Kojac|        Lord British's Castle ; Slot 2  Avery     LATEST, RECOVERED|        Britannia ; Slot 3  Iolo|        Iolo's Hut",
          "T4", "the title's Load Game marks Continue's slot LATEST and starts on it: " + page(v));
    g.handle(kUp, 5); g.handle(kUp, 5);
    check(g.view().selected_line == 2, "T5", "Up from Slot 1 wraps to Slot 3");
    g.handle(kMic, 6);
    check(g.state() == FrontendState::Continue && g.view().selected_line == 1, "T6", "the Mic leaves Load Game for Journey Onward, on Load Game");
    FrontendSession e;
    e.start(0, false, {});
    e.handle(kEnter, 1);
    e.set_save_catalog(FrontendSaveCatalog{});
    e.handle(ch('j'), 2);
    check(str(e.view().subtitle) == "No saved journey on this card", "T7", "a blank card: Journey Onward says so");
}

void test_pc() {
    std::printf("\n[P] PC Save Transfer\n");
    FrontendSaveCatalog c = three();
    c.slots[0] = FrontendSaveCatalog::Entry{};
    FrontendSession f;
    f.start(0, false, {});
    f.handle(kEnter, 1);
    f.set_save_catalog(c);
    f.handle(ch('p'), 2);
    const bool reached = f.state() == FrontendState::PcTransfer && f.take_intent().kind == FrontendIntentKind::InspectPcSaves;
    PcImportStatus ready{};
    ready.state = PcImportState::Ready;
    std::snprintf(ready.text, sizeof(ready.text), "%s", "Kojac, Lord British's Castle");
    f.set_pc_import_status(ready);
    f.handle(kEnter, 3);
    auto v = f.view();
    check(reached && f.state() == FrontendState::PcImportSlot && v.selected_line == 0 &&
              page(v) == "Slot 1  EMPTY| ; Slot 2  Avery     LATEST|        Britannia ; Slot 3  Iolo|        Iolo's Hut",
          "P1", "P reaches PC Save Transfer; Import lists the slots as the other pages do, on the first empty: " + page(v));
    f.handle(kDown, 4);
    f.handle(kEnter, 5);
    v = f.view();
    check(str(v.title) == "Replace Slot 2?" && str(v.subtitle) == "Avery, Britannia" && v.selected_line == 0 &&
              f.take_intent().kind == FrontendIntentKind::None,
          "P2", "importing over an occupied slot asks, naming its save; No first");
    f.handle(kMic, 6);
    check(f.take_intent().kind == FrontendIntentKind::None && str(f.view().footer) == "Import cancelled. Slot 2 is unchanged", "P3",
          "the Mic cancels the import question");
    ready.imported_slot = 2;
    f.set_pc_import_status(ready);
    f.handle(kUp, 7);
    f.handle(kEnter, 8);
    v = f.view();
    check(str(v.title) == "Import it again?" && str(v.subtitle) == "These PC files went into Slot 3 before" && v.selected_line == 0,
          "P4", "re-importing the same PC files asks first, No selected");
    f.handle(kBack, 9);
    f.handle(kBack, 9);
    f.handle(kDown, 10);
    f.handle(kEnter, 11);
    v = f.view();
    check(f.state() == FrontendState::PcExportSlot && v.selected_line == 1 && str(v.lines[1]) == "Slot 2  Avery     LATEST" &&
              str(v.footer) == "Enter writes /ultima5/export/slot2",
          "P5", "Export lists the same rows, on Continue's slot");
    f.handle(kUp, 12);
    f.handle(kEnter, 13);
    check(f.take_intent().kind == FrontendIntentKind::None && str(f.view().footer) == "Slot 1 is empty", "P6",
          "exporting an empty slot refuses");
}
} // namespace

int main() {
    test_rows();
    test_save();
    test_load();
    test_title();
    test_pc();
    std::printf("\na4_ui3_slot_rows: %d/%d checks GREEN, %d RED\n", g_checks - g_failures, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
