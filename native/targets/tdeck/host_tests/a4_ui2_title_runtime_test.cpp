// Alpha 4 UI Batch 2 (targets/tdeck/ALPHA4_UI.md section 2.1) -- the title
// screen under the Ultima V / Warriors of Destiny art, read off the fake ST7789
// after a REAL AlphaRuntime render on the REAL tdeck_board.cpp.
//
//   T  the credit block ("Lord British presents", "Copyright 1988 Lord
//      British") in the game's own 8x8 IBM.CH, each line centred under the art;
//      "Press a key" centred beneath it as the prompt, no longer the bottom-left
//      footer; nothing else lit below the art; the art itself byte for byte the
//      pack's frames; the fire animation still resends the strip alone; the
//      Title -> intro -> attract timings and "any key opens the menu" unchanged
//
//   a4_ui2_title_runtime <openu5-alpha1-resources.bin> [--dump <dir>]
#include "a4_ui2_harness.h"

using namespace a4_ui2;

namespace {
// The approved layout (ALPHA4_UI.md section 2.1). Cells are 8 px (IBM.CH).
constexpr const char *kPresents = "Lord British presents";
constexpr const char *kCopyright = "Copyright 1988 Lord British";
constexpr const char *kPrompt = "Press a key";
constexpr int kArtTop = 4, kArtBottom = 113; // kCharacterTitleArtY / H: 4 + 110 - 1
constexpr int kPresentsY = 130, kCopyrightY = 144, kPromptY = 184;
int centred_x(const char *s) { return (320 - int(std::strlen(s)) * 8) / 2; }

/** Rows 4..113 equal one of the pack's four 320x110 title frames; returns it, or -1. */
int art_frame() {
    for (int f = 0; f < 4; ++f) {
        const uint16_t *art = pack->intro_title + size_t(f) * 320 * 110;
        bool same = true;
        for (int y = 0; y < 110 && same; ++y)
            for (int x = 0; x < 320 && same; ++x) same = px(x, kArtTop + y) == art[y * 320 + x];
        if (same) return f;
    }
    return -1;
}
/** FNV of everything below the art (the credit block, the prompt, the footer). */
uint64_t below_art_hash() {
    uint64_t h = 1469598103934665603ull;
    for (int y = kArtBottom + 1; y < 240; ++y)
        for (int x = 0; x < 320; ++x) h = (h ^ px(x, y)) * 1099511628211ull;
    return h;
}
/** Lit pixels below the art outside the three expected runs. */
int stray_below_art() {
    int k = 0;
    struct Run_ { int x, y, w; } runs[] = {
        {centred_x(kPresents), kPresentsY, int(std::strlen(kPresents)) * 8},
        {centred_x(kCopyright), kCopyrightY, int(std::strlen(kCopyright)) * 8},
        {centred_x(kPrompt), kPromptY, int(std::strlen(kPrompt)) * 8},
    };
    for (int y = kArtBottom + 1; y < 240; ++y)
        for (int x = 0; x < 320; ++x) {
            bool inside = false;
            for (const auto &r : runs) inside = inside || (y >= r.y && y < r.y + 8 && x >= r.x && x < r.x + r.w);
            if (!inside) k += px(x, y) != kBlack;
        }
    return k;
}

void test_title() {
    std::printf("T  the title screen\n");
    Run h({{"Avatar", 'G', 100}});
    h.return_to_title();
    const int64_t t0 = Run::now_ms();
    const bool title = h.rt->frontend_open() && h.rt->frontend_state() == FrontendState::Title;
    dump("title");
    const int frame0 = art_frame();
    check(title && frame0 >= 0 && lit(0, 0, 320, kArtTop) == 0,
          "T1 control: Return to Title shows the art unchanged -- rows 4..113 are one of the pack's four frames (frame " +
              n(frame0) + "), rows 0..3 black");
    const int presents_bad = ibm_mismatch(centred_x(kPresents), kPresentsY, kPresents, kWhite, kBlack);
    const int copyright_bad = ibm_mismatch(centred_x(kCopyright), kCopyrightY, kCopyright, kWhite, kBlack);
    check(presents_bad == 0, "T2 \"Lord British presents\" is IBM.CH (8x8, was the 5x7 compact font) in white, centred: "
                             "x=" + n(centred_x(kPresents)) + " y=" + n(kPresentsY) + " (" + n(presents_bad) +
                                 " pixels differ)");
    check(copyright_bad == 0, "T3 \"Copyright 1988 Lord British\" likewise, centred on the next line: x=" +
                                  n(centred_x(kCopyright)) + " y=" + n(kCopyrightY) + " (" + n(copyright_bad) +
                                  " pixels differ)");
    const int prompt_bad = ibm_mismatch(centred_x(kPrompt), kPromptY, kPrompt, kDim, kBlack);
    check(prompt_bad == 0, "T4 \"Press a key\" is the prompt under the credits, centred, IBM.CH in the footers' grey: x=" +
                               n(centred_x(kPrompt)) + " y=" + n(kPromptY) + " (" + n(prompt_bad) + " pixels differ)");
    const int stray = stray_below_art();
    check(stray == 0, "T5 nothing else is lit below the art: no left-aligned x=20 lines, no bottom-left footer (" +
                          n(stray) + " stray pixels)");
    int l = 0, t = 0, r = 0, b = 0;
    const bool inked = ink_box(0, kArtBottom + 1, 320, 240 - kArtBottom - 1, l, t, r, b);
    const int gap_top = t - kArtBottom - 1, gap_bottom = 239 - b;
    check(inked && std::abs((l - 0) - (319 - r)) <= 8 && gap_top >= 12 && gap_top <= 24 &&
              kCopyrightY - kPresentsY == 14 && kPromptY - (kCopyrightY + 8) >= 24 && gap_bottom >= 40,
          "T6 spacing: the block starts " + n(gap_top) + " px under the art, lines 14 px apart, the prompt " +
              n(kPromptY - kCopyrightY - 8) + " px below them, " + n(gap_bottom) + " px clear to the bottom; ink x " +
              n(l) + ".." + n(r));

    // The fire animation: the next frame resends the 288x49 strip alone.
    const uint64_t text = below_art_hash();
    int changed_frame = -1;
    size_t pixels = 0;
    for (int i = 0; i < 80 && changed_frame < 0; ++i) {
        h.run(5);
        const int f = art_frame();
        if (f >= 0 && f != frame0 && h.rt->frontend_state() == FrontendState::Title) {
            changed_frame = f;
            pixels = h.board.debug_last_pixels();
        }
    }
    check(changed_frame >= 0 && pixels == 288 * 49 && below_art_hash() == text,
          "T7 the fire animation still sends only its 288x49 strip (" + n(long(pixels)) +
              " px); the credit block and the prompt are not resent");

    // Timing (frontend.cpp, unchanged): 1,400 ms of Title, the startup intro (the same screen), attract at 5,600.
    while (Run::now_ms() - t0 < 1395) h.run(5);
    const bool still_title = h.rt->frontend_state() == FrontendState::Title;
    while (Run::now_ms() - t0 < 1405) h.run(5);
    const bool intro = h.rt->frontend_state() == FrontendState::IntroAnimation;
    const uint64_t intro_text = below_art_hash();
    dump("title-intro");
    while (Run::now_ms() - t0 < 5595) h.run(5);
    const bool before_attract = h.rt->frontend_state() == FrontendState::IntroAnimation;
    while (Run::now_ms() - t0 < 5610) h.run(5);
    const bool attract = h.rt->frontend_state() == FrontendState::AttractDemo;
    check(still_title && intro && before_attract && attract,
          "T8 timing unchanged: Title until 1,400 ms, the startup intro until 5,600 ms, then the attract demo");
    check(intro && intro_text == text, "T9 the startup intro shows the identical credit block and prompt");

    // Any key on the Title opens the main menu, which replaces the block.
    Run m({{"Avatar", 'G', 100}});
    m.return_to_title();
    m.key(' ');
    const bool menu = m.rt->frontend_state() == FrontendState::MainMenu;
    const int prompt_left = ibm_mismatch(centred_x(kPrompt), kPromptY, kPrompt, kDim, kBlack);
    dump("main-menu");
    check(menu && prompt_left > 0 && lit(0, 228, 320, 9) > 0,
          "T10 a key on the Title opens the main menu, whose rows and footer replace the credit block and prompt");
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: a4_ui2_title_runtime <pack> [--dump <dir>]\n");
        return 2;
    }
    g_dump = arg_after(argc, argv, "--dump");
    if (!load_pack(argv[1])) {
        std::printf("RED the pack does not load -- the title checks cannot run\n");
        return 1;
    }
    test_title();
    std::printf("A4-UI2 title runtime: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
