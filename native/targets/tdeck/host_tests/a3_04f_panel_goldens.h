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
constexpr const char *kSource = "A4-UI4 console height over the console package";
constexpr size_t kRenders = 2766;
constexpr size_t kPhaseCount = 8;
constexpr size_t kPhaseStart[8] = {1, 21, 132, 145, 165, 210, 232, 261};
constexpr size_t kCount = 301;
constexpr uint64_t kPanel[301] = {
    0xf9a3a50e50b2d9b5ull, 0xe77761e7523a6433ull, 0xca053979a7141b33ull, 0x8bf2c5dccb50aeb5ull,
    0x259e403c558e915bull, 0x7b370e41395418cbull, 0x201a46dc7ea6b4ddull, 0x257b5ec772649c53ull,
    0xaff571f7bb8494cdull, 0xdad6a1ff1db6b9dfull, 0x5e9bac384fb6d39dull, 0x2cad9e786e2605f7ull,
    0x281431d0dc44cecfull, 0xfdaf815d86268e69ull, 0xfda2a5771021dc49ull, 0x1c2106c91f2670a3ull,
    0xbbe400f091eb6ef5ull, 0xe77761e7523a6433ull, 0xca053979a7141b33ull, 0x8bf2c5dccb50aeb5ull,
    0x259e403c558e915bull, 0x33d5a7e8eba53b1full, 0xa07f799039eeb249ull, 0x50816bfd26f4e947ull,
    0x493a37942d0fa6a1ull, 0x023cd2783ff9b717ull, 0xeba021bf7cad54e9ull, 0xb8ca296f3ba14ab9ull,
    0x245f23fbf0fef03bull, 0x1f3e6bb934393bcbull, 0xb3bdfd2d0e95affdull, 0xe6e9e45a64ab5b13ull,
    0x011ccf7a89e669c9ull, 0xe465fe9f834b403full, 0xcc3c0b02960d1b7full, 0x15c3077a4c0e70a1ull,
    0xdb58dc03e3598191ull, 0x38b9af5bc7885d4bull, 0xb5cf7e906a48a2e1ull, 0xe752b4171665b999ull,
    0x3786814e3f1ee6fbull, 0xf7e404696786718bull, 0x7580e36c859b1e85ull, 0xfa2909241dc8bad1ull,
    0x7fa88f9e84b04691ull, 0xf3641bf462933633ull, 0x1ffefc703b6deb81ull, 0xd5467c53ec72c47bull,
    0x3554aea85e658ed9ull, 0x58013961b3640f8bull, 0x5adf3688d86eff85ull, 0x8c56889a89e679ffull,
    0x8d91bbbad034f157ull, 0x90ebcc114e97d6a9ull, 0x85b67b84a4a9d7fdull, 0x14f9e40c7ef1881full,
    0x67a599b9ebd41199ull, 0x27bb749e22e9228full, 0xb80571c09219be7full, 0x7982e5ea25a5da0dull,
    0x27b4c9e80e07eb4dull, 0xbd20f74eed2bf147ull, 0xd928d6bcaa4ab3f3ull, 0xc30d45801e66597dull,
    0x14c5d29fab5a6d8full, 0x0a2a13403cb68acdull, 0x031c56846dc32c15ull, 0xf2cfff31861ab547ull,
    0xb5acfffad9d2773full, 0x44acee8bb18f9691ull, 0xc1339a0fdf7f298full, 0xe36cfa729a741fe7ull,
    0x048831c9ae798171ull, 0xe62864520f11b117ull, 0xc299429a46a5bc31ull, 0x71d7e67dabdf3c5full,
    0x7673b35afb6db7e9ull, 0x2044a2e0297c6f07ull, 0x7c7671d53e0f9b01ull, 0x238e4e84416977d9ull,
    0x9f79c1a7a25271c3ull, 0xac471edafc9db5d5ull, 0x9a19e9bb4aedc497ull, 0x35b493b043a914ddull,
    0x8763abcd124fe3f1ull, 0x10b83f575cc3a4c7ull, 0x1466454d3663d18full, 0x0b500bef6875435full,
    0x755f71ca077944e9ull, 0xb192ed3103f150fbull, 0x12294693ba5bd9f9ull, 0x69c54ee3cda43f3bull,
    0xb05b1181fe9ae675ull, 0x1cd0222ca2887f55ull, 0xd275998fbd83d58full, 0x0e97f470a6fcf649ull,
    0x071af4aff5cf053full, 0x49f3fdd77e80bbffull, 0x8063c3ff3a19def9ull, 0x2349f3b7023d340bull,
    0xc4925de929066495ull, 0x4329b12021eb7b83ull, 0x7390c641032568bdull, 0x4823b5e3965fd543ull,
    0xd6dfe77741ba3851ull, 0x3a11229bcc40d4ebull, 0xdc3f3c1a7340bf83ull, 0xb28b99c9fe475055ull,
    0x27c421df8ff69c8full, 0xa719593644f134a1ull, 0xb8e962ae20e68d5full, 0xf20af713518ad47bull,
    0xf3140f9921506651ull, 0x13d7b2ae34175421ull, 0xbdbbec2f9aaa0795ull, 0x0b94c6ee216d86a7ull,
    0x53d26aefcaa71c01ull, 0x005fa799f374550full, 0xfdf9c37032d5fab3ull, 0x9e86c135b2269f15ull,
    0x2beb5d20ab26caedull, 0x14e6e038a3243bbdull, 0xa0c0deecb5ff420bull, 0x834eb67f0ad8f90bull,
    0xdd726c21ea75807dull, 0x2a2f1c6e66c924e5ull, 0x9c3de4796c8e7acfull, 0x7f4c8dc0a80b073dull,
    0x9dbf20c6524914b9ull, 0x2facea171c426437ull, 0x695c03b9828210c9ull, 0x5f4e03b591414919ull,
    0xfba89f6fb4a0973bull, 0xe7e28fce032dbeedull, 0xd1c7da1c3b52c357ull, 0x01cc061aadf1b65dull,
    0x061df86808f77bb5ull, 0xf28378d3bf82502full, 0xc3ea83900bd7ba13ull, 0x031013f040b13799ull,
    0xf101a3a9da9a4361ull, 0xa84a01772ab5a0bbull, 0x4c87790a89d6bef9ull, 0x0c042d592add3123ull,
    0x867441ce84beacb5ull, 0xb9b6de68be7679a8ull, 0x1b37af5a459eca34ull, 0x6d00e64b3d05a6aaull,
    0xc73484349b09e136ull, 0xc9ede2692f8c3d64ull, 0x918771c5fdf62ceaull, 0xb548db2907a21ec7ull,
    0xede654564bf7cffdull, 0x6e9960ac88fd97d5ull, 0xf36e565b7b63b2dfull, 0xe6010cf7e5ff9c9dull,
    0x7a0cf40dc1810317ull, 0xb592000c0decd0d1ull, 0x709720f2d5d04135ull, 0x047d6974c0a5c1bfull,
    0xac4e51932673d225ull, 0xd2bbd81b1d4e4bedull, 0xb548db2907a21ec7ull, 0xede654564bf7cffdull,
    0x6e9960ac88fd97d5ull, 0x5d7d9eb86bc5d005ull, 0x1871aa5e087aaf78ull, 0xe97b97a4c1536528ull,
    0x5055256cf7cb8d18ull, 0xe25f3b7efb9c1698ull, 0xeb98f63aa7f955c6ull, 0xcb94d7744a53f082ull,
    0xc69793c10113612cull, 0x6bf7308e8f8a8564ull, 0xbe169dbd50ccdd7aull, 0xf8cff581dd99f372ull,
    0x11046d16288737c2ull, 0xc23b76707e4cb5baull, 0x7e3a3a4363ff7d8eull, 0xea6ea720cc194e36ull,
    0x4f2bfdf36612025aull, 0xff2b3346dc2749feull, 0x252fc90acaefc301ull, 0x6ed5ecd9d7377c62ull,
    0xd534fa00e54d349aull, 0x42e0d08dcf0ea936ull, 0x75bc354667174e0dull, 0xcf5963dee6ec167bull,
    0x66b74a3d2977de26ull, 0x864375a483f6a1b7ull, 0x9704d30d146ffcb4ull, 0x76105160bc0bd7a9ull,
    0x1b0bf77746d3282bull, 0x45f22ad4038861b9ull, 0x46a9a4540603d551ull, 0x90ea2ec7b1c78823ull,
    0x4df41f64914a91a1ull, 0x9d456fb43d3dda29ull, 0xec440a076e700e2bull, 0x5e4fccde5dcef1a1ull,
    0x91b60dd9ec8cde53ull, 0x670077ac40b1a8fdull, 0x95a0a899bf1d3d9bull, 0x0332114ac0480c2dull,
    0x93a42673079e6bddull, 0xd02cadb63eaa0d7full, 0x0c0c21dc5130680full, 0x76105160bc0bd7a9ull,
    0x1b0bf77746d3282bull, 0x45f22ad4038861b9ull, 0xc14ef86c13adb9adull, 0x8587afe16e27c5b3ull,
    0xc5cfc5045879f163ull, 0xd805f6ea6707a225ull, 0xb2b0abc778933323ull, 0x10bb216ba6d55355ull,
    0xad0953a0b145490bull, 0xf7971e6a475970e5ull, 0x2a04a30df44ab6d7ull, 0x2310a405ba1f34bfull,
    0xff3e0a33979fe891ull, 0x09c3ebbbd0d8cc27ull, 0x50d7a538573be6ffull, 0x8977c7dad2c4ac61ull,
    0xf83f187268f7406bull, 0x40e8eff95bd09835ull, 0xa665351601188b43ull, 0x69074d998b273dfdull,
    0x8492897991bc9ab9ull, 0x8dd893467c87b7fbull, 0xd3954a11777d890bull, 0x85534e3bd5be8d0dull,
    0xa97f53b35f5f223dull, 0x837f030103c06d9full, 0xc0cafabd73a1686bull, 0x982d38be3aaf284dull,
    0xb42cb38263edeb1dull, 0x8a30d0a74e46cd27ull, 0xf42e64d9f9243639ull, 0x262fa207dc6e3f67ull,
    0xc25b4babebcee715ull, 0x14ef85caa9db15a7ull, 0xde2b7128f1ad3bd1ull, 0x04f011e186aea66full,
    0x8e3afb96c67f2107ull, 0xb96c81b911268db1ull, 0x43e3ec50b1f7029bull, 0x9a16eb204ceb0a8dull,
    0x8d390bca6d3f1957ull, 0xeaf03638faddaacdull, 0xee55f68cdff03b35ull, 0xd1563ab7bb4de633ull,
    0x8b7df51c4c41bff1ull, 0x5d09cf54f6ef8c31ull, 0x162da5a00211288bull, 0x0af4f95828cbd363ull,
    0x0fbba80e31f010dbull, 0x791a2345a6016e93ull, 0xd19823157626a92dull, 0xd83236abe908b25bull,
    0x35f3db034b9c446bull, 0xaa0bc16e632484fbull, 0x428139e207bf48e9ull, 0x97373fa078c0fc09ull,
    0x49ca15c347daf871ull, 0xdaf39fa64ebc2827ull, 0x3265cd33562a2d05ull, 0x4615a7f54174d705ull,
    0x38f6bfd700dda503ull, 0xe877ec4f23cf41a3ull, 0xc300dbec6476a6e1ull, 0x82815fce306d4e23ull,
    0x155e6d5b1b667319ull, 0x41a5657de4afd7cfull, 0xb1b1ea70671fcc03ull, 0x62ac5f54c4141c3full,
    0xe382afda63211475ull, 0x61ef5db7db96d7ffull, 0x4482e23ae311c91bull, 0xa5c5b7ed7a4514b1ull,
    0xa7933364b09227bfull, 0x41e8794bea701147ull, 0x6c9e3619d92d1539ull, 0x8cd3214e2a4b8fafull,
    0x8c6e44365ea84c17ull, 0x42f03869e927b79dull, 0xec693806a397d71bull, 0x078f6568cd6464c3ull,
    0xd91894f038d0df69ull, 0x3b1c1b23f4f7e27dull, 0xe236fc43e1c5eea3ull, 0x005f1a50b883f3b1ull,
    0x4c2531e30ea8c753ull, 0x9f9dbff5703c5e8bull, 0xaa45b219334d2f71ull, 0xf4bf773fbb45d23bull,
    0xb091724f791d4be1ull, 0x8b96c0dc1f4f3239ull, 0x5b4306407b28c4abull, 0x12680373a9dc34cfull,
    0x3c05b503b20919f5ull,
};
} // namespace a3_04f_goldens
