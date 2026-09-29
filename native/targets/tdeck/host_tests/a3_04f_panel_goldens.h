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
constexpr const char *kSource = "Alpha 4 UI Batch 1 restyle (the reviewed A4-UI1 Board)";
constexpr size_t kRenders = 2766;
constexpr size_t kPhaseCount = 8;
constexpr size_t kPhaseStart[8] = {1, 21, 132, 145, 165, 206, 228, 257};
constexpr size_t kCount = 297;
constexpr uint64_t kPanel[297] = {
    0x46ef8e664f719e25ull, 0x71fd234acd69a693ull, 0x548afadd22435d93ull, 0xa1712290a9866335ull,
    0xf478ab2fcebcfbebull, 0xe50e7fc527716263ull, 0xe520d4a2db35fdc5ull, 0xf59ff31908b55cc3ull,
    0x84c79d1c8d9679adull, 0x8ef55a16b6a9eddfull, 0xba3ad337da76706dull, 0x9f0cf1bd271a47a7ull,
    0x3a915b68d11ecfdfull, 0xee3effa9cbff9441ull, 0x832945d419899421ull, 0xe779356b73db9513ull,
    0xc72268085426df75ull, 0x71fd234acd69a693ull, 0x548afadd22435d93ull, 0xa1712290a9866335ull,
    0xf478ab2fcebcfbebull, 0x7a5a111c5bb35a37ull, 0x4eead4863d9f7719ull, 0x79abc011f49e66e7ull,
    0xe8c57785df061cd9ull, 0x074eb591d1287e6full, 0x120e864e29f349a1ull, 0x3989d261123510b1ull,
    0x33f81a2e15efbc1bull, 0x413e7af57151a40bull, 0xd83d11369bf26f25ull, 0x896e9f8a57d7f843ull,
    0x59e8cbaa17cbddd9ull, 0x39d085299d9449efull, 0xe32bfb4637128987ull, 0x002ed833b8e92be9ull,
    0x291adc2112681d99ull, 0x7214967e0f8d7cf3ull, 0x9e43ea8140033159ull, 0x7fac190092100a41ull,
    0x8b58cedff21c721bull, 0x72129c2e723eba6bull, 0xea2a47a0849c3dedull, 0xa5354f47b64204b5ull,
    0xb17b0f54e2abff75ull, 0xebba537794de6737ull, 0x54c8007153c72d0dull, 0x1f9736bb3b03b1cfull,
    0x20a115afe6d2a7edull, 0x9ead731baf3f96d7ull, 0x8d8ceae9a6a4cdc5ull, 0xf32c85dd0f87d467ull,
    0x93750155763aee1full, 0x7ebf0fa41eefc051ull, 0x75069dee30de8587ull, 0x0480028206591c49ull,
    0xda81a63a06ccb0cbull, 0xd0f534c1c7306329ull, 0xd3fffae8e175411bull, 0x16faf44ce3f5a161ull,
    0xe4a8b75e57cea8c9ull, 0x441a6012869af87bull, 0xfa29b005304fcc9bull, 0xbad96d3793659585ull,
    0x4bc44102c428e747ull, 0xed707bbb264b9545ull, 0x4d8a580a17dc36a7ull, 0xa2c9a19f04891359ull,
    0x7c8d137a86f2ba51ull, 0xbc0b2eb0dd70a103ull, 0x01b5dc21f1fbbe61ull, 0x5590ceb7fe464ef9ull,
    0x0097397bb74c9f2bull, 0xf61e5e0d827c35b7ull, 0x7e227ce58c777e09ull, 0x7f4edf1a40eca767ull,
    0x389512de3109bbc9ull, 0x1a5f36459a5b955full, 0xdc02bd0ea277c579ull, 0x82e124810d177ff1ull,
    0xe9599b445ee56995ull, 0xf0f8b0f15c594267ull, 0xdcf63a6b4f3b8831ull, 0x8d2edeac9eb53d5full,
    0x71fd942d5c13dfabull, 0x9a522011bd6571e9ull, 0xb689d3cbfd5d4069ull, 0x18c7048bfd82ae67ull,
    0x3780d14d3d1548c9ull, 0x8a264400a6ff79b3ull, 0x0f716a19a7f76a89ull, 0xf015f44dac1ae583ull,
    0x9cba86d2de1d82a5ull, 0xab884f67cc86c085ull, 0xadfa73b59bdc1797ull, 0x6fbd8e728172e5fbull,
    0xee8373f3da7ec6a9ull, 0xe000c7fe6b9d5c69ull, 0xcf0b963f53e698abull, 0x27ffca074bea6a85ull,
    0xf1d6cc4b80c20dc7ull, 0x5a7836dac029db55ull, 0x31e861fd880b40c7ull, 0xbfb74254d9cc0cfbull,
    0xb3f8c1e5b429bbc1ull, 0x75073d40a24fd3f3ull, 0x16ff7cf395d7f2cbull, 0x4143c70528459185ull,
    0x0348fc056e4ede97ull, 0x1b3f0d6a737ba099ull, 0xbdcbd95585775e17ull, 0xd67bbe60ddf74ee5ull,
    0x5ed7fe75054ea2c3ull, 0x84288e54d105d68bull, 0x35a799178eb540bfull, 0x111b2fb6fb83bcc9ull,
    0xfbc6f4eccfc1222bull, 0xe943d808f3d319e1ull, 0x2be5376fbe32309bull, 0x8bbdfab6c7bcdaa5ull,
    0x7ead708b93d0a19dull, 0x34aeb2bd1f87e08dull, 0x1bc492189a0f5d13ull, 0xfe5269aaeee91413ull,
    0xe9882dbddc3d0e4dull, 0x8e7d2a9ba7b176d7ull, 0xf40347e6ffd3b9d1ull, 0xfa7d5646de355a07ull,
    0xcc490c74e28fec6bull, 0x96187360c60dd369ull, 0x210ed1acc3907a0bull, 0x63ab9da5b1afeb1bull,
    0xc801bdad39d304a3ull, 0xf2a805dc3df45e25ull, 0xf04b84d0f4b2e5f7ull, 0x72690afb28afae05ull,
    0x59d3d44527c9c69dull, 0x3c71194e36e4440full, 0xee19d7b306b56fd3ull, 0x40f10ac4d5051571ull,
    0x12cb961d716fbe71ull, 0x364c1280b0a3b823ull, 0x01d234f9e9769c11ull, 0x668d6e3c16cfe9e3ull,
    0xda2a1daba390f79dull, 0xb9b6de68be7679a8ull, 0x1b37af5a459eca34ull, 0x6d00e64b3d05a6aaull,
    0xc73484349b09e136ull, 0xc9ede2692f8c3d64ull, 0x918771c5fdf62ceaull, 0x9ef941d7f17ab1e7ull,
    0xb5016d720cd654a5ull, 0x86dbf8caa940fda5ull, 0x759d1fcade702df7ull, 0xbffe23f96ffaa3c5ull,
    0x0b3e90f2b272f3b7ull, 0xb592000c0decd0d1ull, 0xaddae776691a8e9dull, 0x524d62267ecea7dfull,
    0xbe4f9e2e3dcc4d3dull, 0xc27b0f147b2c4c85ull, 0x9ef941d7f17ab1e7ull, 0xb5016d720cd654a5ull,
    0x86dbf8caa940fda5ull, 0x18de6e235c26cd15ull, 0x3b905ba490d4d8dcull, 0x9b75ace316021f90ull,
    0x9797fc10539224e8ull, 0xce993e1a2207a3caull, 0xaa2f1ffd24ae293aull, 0xa61472495f69aa74ull,
    0x4405b70fffb5a56eull, 0x19b913539eab8a76ull, 0xde9083eb02a23ef6ull, 0x4cfc5e383b4c064eull,
    0x98a2eb4ed38ee47aull, 0x3d753efdb7ad425eull, 0x8ed6b15d9fc0aa47ull, 0x85d44a7e81fdd6a2ull,
    0xe5af82b130a9ac8aull, 0x26a7d40dd6fcf1e6ull, 0x4228297d60703ff7ull, 0x532e987e1b939515ull,
    0xc382c4d8e8801f8eull, 0x29063feb3061807dull, 0xa48d807a1e298aa2ull, 0x80e7adaaf18d3a03ull,
    0x92f45aa1676a37d5ull, 0x0d21359f6492e043ull, 0x2897d917652d9f2bull, 0xf16be2a9e17d877dull,
    0x3dcfc23b920e760bull, 0x226e8e191b79f9ebull, 0xb1cf4b949cb7d14dull, 0xc4b7c45c7c1fd60bull,
    0xd59c8119af62404dull, 0xaef23a4dae72bd97ull, 0xb4657f012b1e5375ull, 0x5c9343211ff76357ull,
    0x3bc00c0127f2bba7ull, 0x1d6a614a6493a371ull, 0x49e715eb1c6a31e1ull, 0x80e7adaaf18d3a03ull,
    0x92f45aa1676a37d5ull, 0x0d21359f6492e043ull, 0x8f39b41eaf537e13ull, 0x35e41d2217ba7ed9ull,
    0x1910e18c192fea41ull, 0xa29328d80b072333ull, 0xc84e961b24db307full, 0x07ecd933d35f0a29ull,
    0x2b9fd4bed5b60247ull, 0xe401071d3ae3c16dull, 0x80f98e16e6c2336full, 0x251b4ee161a5d2a7ull,
    0x4a6227d4927b2ec9ull, 0x6b9f96c5de751cdfull, 0x5ffed8ba8e971587ull, 0xaa1dc141e1bc83d9ull,
    0xc444a7a08bdbe2e7ull, 0x2dc2e8cb21c1d409ull, 0x7f21afd6ba8f4db7ull, 0xdc05fe59c323f9e9ull,
    0xf7a279bf49a4270dull, 0xeaf967bd7857740full, 0x3cddd01323d3265full, 0xf95663afb9aefa39ull,
    0xc569ea5f35ecdd09ull, 0x78e436af05b77a4full, 0xd99e4a964c32a9e3ull, 0x6da081cf49a35db9ull,
    0xe73e3f82f2565d29ull, 0x7d46b9cfb5b717b3ull, 0x688698450665f831ull, 0x19c5b9398d007ac3ull,
    0xf3400d3e870281adull, 0x860dc47889b614dfull, 0x143efde91e29d3f5ull, 0xf3d7ae14c1f836d7ull,
    0xb6a96f9e48fdfc43ull, 0xc7657afb7f41da0dull, 0x11c1daf3817e6a03ull, 0x03528ca4d2332965ull,
    0x46edb73c87d3a7c3ull, 0x5b83ae27d6864d09ull, 0x04a62c7aa1f73459ull, 0xa7cbc57b39f3321bull,
    0x7a2d6dcb863a15ddull, 0xe49155bc438e9fe9ull, 0x786b78ac233f846full, 0x654039c3eeff7f97ull,
    0x7a526ca07b385ebbull, 0x424f6689f99cc9cfull, 0xd6505686ca9aee4dull, 0xb285abf8a530d1b3ull,
    0x08e0bdf8923e3abfull, 0x7a6a76bf0d85f0cdull, 0xf69e14c11919498bull, 0xccc998060623f65bull,
    0x276385dfc7d980a7ull, 0x12ec9ff0d0eb06e1ull, 0xe9bc8b3465354f3bull, 0xa6be0370b79938adull,
    0x31c11cd2dc869723ull, 0x59f94ed48a1ac0a3ull, 0x12907b69da237e17ull, 0xc959666cb36ad4bdull,
    0x56d14c027edd71fbull, 0x263249cadd9bafa3ull, 0x05425943b65373dfull, 0x74ea575913d53003ull,
    0xbf26bdccd45929e5ull, 0x04266ba307579913ull, 0x1ef89a792adaaa2full, 0x92fcf8fa03d7b1d9ull,
    0x14832ae133e22fe7ull, 0xf43c003f5ef39e2bull, 0x97132dead8f4dbc5ull, 0x1476fe6533197891ull,
    0xd627ef9f73dec415ull, 0xb75489c9cd1a34c3ull, 0x30551df8a30b35ebull, 0x739db70cc3d59f53ull,
    0x3cb96ca30c007525ull, 0x8d763fb24016fc79ull, 0x072d6415df856b23ull, 0x24a0996bf381cf75ull,
    0xcad40676d81f4805ull, 0x1b954d20956a199dull, 0x91c93aef873da9c7ull, 0xb62514929ccc4f75ull,
    0x12280b67d61c748bull, 0x3716495348f76487ull, 0xf4aa92427da890e9ull, 0xf68fff52efb79f25ull,
    0x6305dd250e9af8fbull,
};
} // namespace a3_04f_goldens
