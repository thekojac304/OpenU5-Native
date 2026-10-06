#pragma once
// A3-04F (ALPHA3_AUDIO.md section 26): the panel after every render call of
// a3_04f_render_runtime's golden script (consecutive repeats folded), FNV-1a
// over all 320x240 pixels, with `a3_04f_render_runtime <pack> --record <this file>`.
// Re-recorded ONCE from the Alpha 4 UI Batch 1 Board (ALPHA4_UI.md): its restyle is
// the one intended change; the census (R/T/P) that guards A3-04F's efficiency is
// unchanged. Never regenerate it from a Board that changes how, not what, it draws.
#include <cstddef>
#include <cstdint>
namespace a3_04f_goldens {
// Alpha 4 A4-UI4 (ALPHA4_UI.md section 8): re-recorded for the console text alone (the DS-cited
// echoes, Look-North, no "ended" line): with the console region (x >= 182, y >= 87) masked,
// all 297 states equal the A4-UI1 recording (a4-ui4-census-masked.log).
// Alpha 4 A4-UI4 (section 8.20): re-recorded again for the console package the user chose
// (bullets, blank rows, bottom anchoring, the live prompt row and the wave cursor): 301 states,
// four more, all differing from their neighbours inside the console only; masked as above they
// fold back to 297 states, each equal to the pre-console recording (a4-ui4-console-golden-proof.log).
// A4-UI4 hardware follow-up (ALPHA4_UI.md section 8.22): re-recorded for the console's
// height alone (UiSession page_rows 12 -> the Board's row count): 301 states as before; with
// the console rectangle (x 184..318, y 88..239) blanked, the 297 masked states equal the
// previous recording's one for one (a4-ui4-hf1-golden-proof.log). Two recordings byte-identical.
// A4-UI5 (visual polish): re-recorded for the frame's new skin -- the glass below the map (x<181,
// y>=180), the cut corners (left taper x<4 y<8 and y 172..179, top-right x>=316 y<2), the location
// caption's 2 px indent (x 184..317, y 58..65), the status header rule (y=66) and the context bar's
// separators (y=215 and 227). With those regions masked all 301 states equal the HEAD recording
// one for one (a4-ui5-golden-proof.log).
constexpr const char *kSource = "A4-UI4 console height over the console package";
constexpr size_t kRenders = 2766;
constexpr size_t kPhaseCount = 8;
constexpr size_t kPhaseStart[8] = {1, 21, 132, 145, 165, 210, 232, 261};
constexpr size_t kCount = 301;
constexpr uint64_t kPanel[301] = {
    0x4cc8fabc8571a131ull, 0xe23d825ee1e97f5full, 0x859c39e7384d49dfull, 0xe060f654cc68d2f9ull,
    0x3ae33914c1e1bca7ull, 0xc0815e538d8c5d27ull, 0xefebbb229b052761ull, 0x14689ba45536666full,
    0x19390073f1705981ull, 0xb26b3be3397a304bull, 0xa5c493583affd2a1ull, 0xeab5d4ea828e5303ull,
    0x7c84ad511f0925cbull, 0x11e545ba9f42dfcdull, 0x7c76356edbd4c26dull, 0x76971944a65cf21full,
    0xed990213d9c9cf99ull, 0xe23d825ee1e97f5full, 0x859c39e7384d49dfull, 0xe060f654cc68d2f9ull,
    0x3ae33914c1e1bca7ull, 0x979a9b1c719eda83ull, 0x00cfa737c49b8b05ull, 0x0b80fb624880c62bull,
    0xfad678ba9b09687dull, 0x9d4e71be4f49baabull, 0xc27cb9d073380bc5ull, 0x773c89c888a10335ull,
    0x41816bae6ab6610full, 0xfe6b159ad722c1e7ull, 0x883d2b52da9586f9ull, 0x0677accf00dc4c0full,
    0x5e74aa37d6c2e395ull, 0xf27b5aaeda048fd3ull, 0x775da2cd439f1a83ull, 0x0619998a04304d2dull,
    0x7c287e2c85aeeb55ull, 0x91f2d7c2e3746167ull, 0xf8533ba21f8af555ull, 0x71739362bacc0bc5ull,
    0xe93f15baef989ebfull, 0x40902301d2d0f2efull, 0x7a715a5dcd7bae51ull, 0x0aa0f79488151e45ull,
    0x2cb6319b83bf4b05ull, 0x258853d0af2e147full, 0xf4a936aeb5479905ull, 0xe78bb770cb2c58c7ull,
    0xdaf2d088f3ba455dull, 0x8bf28da99c71ed07ull, 0xb280dfaf48eceed9ull, 0xfaf3cea4005a51ebull,
    0x69de8aa936af6ff3ull, 0xf5cdc1bc0981e44dull, 0xdfa14c66839884d1ull, 0x8e4353e825526f9bull,
    0x2171bfa549e3fc5dull, 0xbcccd92190c97d9bull, 0x7bc1849daae7bc6bull, 0x226ca2b736b46a31ull,
    0x29bcd8d5d5fba941ull, 0x3b4b4d3513ec7a93ull, 0xabe7b6e212ee7c47ull, 0x299201ed84606e09ull,
    0x26f9842a626a4283ull, 0xfa5858aeb8cb7b29ull, 0xf318ed7ad19f5a61ull, 0x67f5de2c56582c9bull,
    0xf24d6e79c063c8c3ull, 0x001608ee37c86eddull, 0x013617b1c5af0c9bull, 0xe19f6cb184bdf063ull,
    0xf418e47dae44bf05ull, 0x166513bda67467cbull, 0x0ae90173ce0f684dull, 0x82c1a5641bdbccf3ull,
    0xbff8e9c4a2f96445ull, 0xf8a1aa066e2787d3ull, 0x1e79a6e9594524c5ull, 0x528b65e98021f18dull,
    0xe05a8afe09e52f17ull, 0x606d613207f4e9f1ull, 0xec1fa35123b7aa7bull, 0x350dbad757c2e1b9ull,
    0x86bdfff0a8aae475ull, 0x53e47ad45b3ee203ull, 0xa35e661b5e35eb8bull, 0xcf7f323ca198b5f3ull,
    0x15477214756c2b45ull, 0xf45702d48184aadfull, 0x6e8066e98f668985ull, 0xa695a2ddc01ecc0full,
    0x1e15009aeb962ab1ull, 0x41d7204361818ad1ull, 0xadf30a8de1380fd3ull, 0xf0352300358f8a1dull,
    0xffe293afccabcfabull, 0x264274161d03a76bull, 0xfec6b63fdd088a0dull, 0x1f6eca92f31fa237ull,
    0x3b2619918de533c9ull, 0x6f198a6b7d4fc38full, 0x4328e9f5b084d991ull, 0x0dde90d504269407ull,
    0x18a9d4c82b74675dull, 0xd490f34593ab0a2full, 0xbc1e9b9f03be4397ull, 0x0cdac255d16e55d1ull,
    0xa5e2b0ce231522d3ull, 0x44b83b01bb47dbcdull, 0x113796a6cc538073ull, 0x92d4c327d1bb863full,
    0xc6184ab77341edddull, 0x83efa51040a8597dull, 0xa37ffff48548a5c1ull, 0x4154899718f85bfbull,
    0x60d669290f2178cdull, 0xbc9cf970a9f27be3ull, 0xc8c3c27b1b422eafull, 0xba55c8e9eaa4bc01ull,
    0xfa8c9b337aff0269ull, 0xb7cf84ac8fe1eb89ull, 0x3304323fac860077ull, 0x9e2c19fadb3618f7ull,
    0x678ea28b81e81a69ull, 0x8d8b6bdf305a1ba1ull, 0x8a8e3f6a73d1b12bull, 0x7588c23e1aa56f19ull,
    0x4d7cb4456ef83c25ull, 0x88defa4f3eed16a3ull, 0x26adc6e23b2e9a65ull, 0x2823b1b5e74a1755ull,
    0xb4c396c270ec7657ull, 0x02a8e16bbbfead19ull, 0xf3c7e4d050cf5853ull, 0xe700bbf0e708fb19ull,
    0x7322515ddfc19d41ull, 0xee4f3c36b7753fbbull, 0xbb1d891169210877ull, 0xdebe25dc9b96d01dull,
    0x2cb0c5ad78780ee5ull, 0x6abb994c1ad9943full, 0x44b1b0ea92a85b3dull, 0xb76c8418bac987c7ull,
    0x89c264f969a3c2e9ull, 0x410b48bf0d2fdbccull, 0xcc597e6b81f6cfa8ull, 0x728fd32238037016ull,
    0x66482a1fb5eece52ull, 0x194ecf19faab4fd8ull, 0xc8114ed947c79856ull, 0xa31929b42303331bull,
    0xa5e818187c6f2031ull, 0x453c5a74dc8aad29ull, 0xdf8b633510729113ull, 0xafe72c7ce676a611ull,
    0xa31315a58aba78abull, 0xb592000c0decd0d1ull, 0x2fb57c34e7715179ull, 0xa3c46976bb8a2e43ull,
    0xaebc9e80e8dda3d9ull, 0xf39d15829b0f62d1ull, 0xa31929b42303331bull, 0xa5e818187c6f2031ull,
    0x453c5a74dc8aad29ull, 0x9d40aa9ca0f72821ull, 0x97f0d93eb8faab70ull, 0x0fe9cb5e18f31490ull,
    0xc5243792b53ec500ull, 0x5374c26303a9ce60ull, 0x8685633d45935876ull, 0x447f3df61326ed12ull,
    0xf7ae22e7bb276b44ull, 0x925930b29cd1aeecull, 0x6cd5bd52fb2b679aull, 0x702de524f85ffdf2ull,
    0xdb9b9850944dc832ull, 0x7a2bf93b0b13de3aull, 0x1c65931be16bb28eull, 0xad938b07e70211f6ull,
    0x807710ada867651aull, 0x8830dfd54fad43feull, 0x72664465081d114dull, 0xa1e6c7cf20ea1752ull,
    0xc6ce4f29dfc61d3aull, 0x10839424faa84916ull, 0x12aecc927c3a052full, 0x2e1cdcd8fa5fd385ull,
    0x474f8261e3804046ull, 0x2ee14e9ba72f12d1ull, 0x102709ac4ca29b84ull, 0x48a1ab96c6a2a0b1ull,
    0x97063266589856e3ull, 0x018e274e4a8cc311ull, 0xe4ce50140a433829ull, 0x0302b6188dcf561bull,
    0x4cfa66e919f04f09ull, 0xeef8bdc54d9dc8e1ull, 0xadc08c1231cc39d3ull, 0x776bb6642138a369ull,
    0x9fd9eb96f0e7416bull, 0x3f018ffb76b242b5ull, 0x62847a24c290e263ull, 0xb7df5a73a3ac25d5ull,
    0x74f800eaaa873245ull, 0x1ec955ae69d05df7ull, 0x888807b6a6e78b87ull, 0x48a1ab96c6a2a0b1ull,
    0x97063266589856e3ull, 0x018e274e4a8cc311ull, 0x181b2e4a2a6268bdull, 0xd99bd93ad9d132a3ull,
    0xa780b1a054bc4c83ull, 0x36d23e02a6ec5f85ull, 0x207ef4c949e15a5bull, 0x95f20fa2f3df6fbdull,
    0xc7c114cb7393aaf3ull, 0x02a260b17e098a15ull, 0x75b0388297742bafull, 0x5696fce5204eb3d7ull,
    0x682b4e9b6dcd6c51ull, 0x56348bf36e3e0c87ull, 0x35fca26d74bfb59full, 0x0939895e14aae3e9ull,
    0xaf10431cdc294023ull, 0x91f95d1ea3debba5ull, 0xd7e9e0787e5a7eebull, 0xcda1ae30c10e31bdull,
    0x35678ced03e40b91ull, 0xb405a0ea52afb5dbull, 0xea2d31dc6e24b3abull, 0x953dc4f20ac27425ull,
    0x9bcf5f7e94b67745ull, 0x52994e2b61ffa94full, 0xceba8a942d28bc53ull, 0xd7f2a53fd80eb6adull,
    0x3b19186ccf00da7dull, 0x819ddbe82fca487full, 0x420ab1aafc066d21ull, 0xb1468fe112a6c2bfull,
    0x089c325ba449799dull, 0xba9739f8a87863d7ull, 0x74f4f97b3d1cd9e1ull, 0x41d50759135b75afull,
    0x526e4c431641850full, 0xf3ab62dfaca4d1d1ull, 0x7be3ec7bd7356edbull, 0x231f7d5eac927835ull,
    0x30743ebf81753e3full, 0x1b8da39b510353ddull, 0x242d44ea4a58fb05ull, 0x01dac47966fea86bull,
    0xc72496f2a1e74ba9ull, 0x56232565d21599b9ull, 0xc190985d22d7b403ull, 0x6ba762b13dbbf9d3ull,
    0xe5b8b686c9007ff3ull, 0xec9c42f71bee1933ull, 0xdfef7f69f3f22365ull, 0x42d2108820140fcbull,
    0xf1f84d8a25140423ull, 0x7b30601dea96c18bull, 0xfed6e74f1bf036b9ull, 0x0c901590fbc14819ull,
    0x96bc7ff5d731f399ull, 0xc02ebf670d77cb07ull, 0xeb5b5f1864b5dd9dull, 0xee64558aff6ff4e5ull,
    0xeeacad11b03396d3ull, 0xed9620c23e8466a3ull, 0xca642a327dcd57d1ull, 0xec392594b2237743ull,
    0x55b99ce5d68e0d39ull, 0x059802c02ddf9ee7ull, 0xc9d0e903b7f405f3ull, 0x1fab4fd267228f47ull,
    0x9c1da0d1705ba7ddull, 0x9890e89d9dc81dafull, 0x257ba041386bd5bbull, 0x7a6d95e9842266b1ull,
    0x9196bf7bed073877ull, 0x51a93bba535e02efull, 0xd314fa0d52db2319ull, 0xc0a6bc9a7a8d6dd7ull,
    0xce634d3531f9bac7ull, 0x3a9c32926bbe480dull, 0xbc25799e038bd483ull, 0xc14e24c5391187bbull,
    0x7cdbd88061e69581ull, 0xba7ca7ecd54cf165ull, 0x4354c04edae75673ull, 0x58095e5690f577b9ull,
    0x5ef0d493132a286bull, 0xb066f3d024de1f53ull, 0x9166f71beacc7301ull, 0x15792d30ca1e194bull,
    0xd6d7efcad7667da1ull, 0xc11636a87fcbec11ull, 0xdd49101bba8a0f03ull, 0x4207dd12e86d6defull,
    0x487f45375098fd75ull,
};
} // namespace a3_04f_goldens
