#define GAME_DEAD_SPACE_2 1

#define GEOMETRY_SHADER_SUPPORT 0
#define DISABLE_AUTO_DEBUGGER 1
// Core's "SceneMotionVectors" reads the shaders from the patch context
#define LUMA_PATCH_BYTECODE_SYNC 1
// Makes "original_draw_dispatch_func" non-null: the scene's draws run again with the motion vector shaders
#define ENABLE_POST_DRAW_DISPATCH_CALLBACK 1
// The motion vector draw key reads the draw's arguments ("last_draw_dispatch_data")
#define ENABLE_DRAW_DISPATCH_DATA_CACHE 1
// A "Super Resolution" choice next to the SR bridge's DLSS, FSR and XeSS, drawn in process on any GPU
#define ENABLE_LUMA_TAA 1
// Optional sharpening of the upscaler's or SMAA's output (DLSS-Best-Practices SHP-2)
#define ENABLE_RCAS 1
// SMAA ULTRA (1x or T2x) in MarkerPatch's SMAA slot or at the scene's end (see "RunSMAA"); Core registers its 6 passes from
// Luma_SMAA_impl.hlsl
#define ENABLE_SMAA 1
// SMAA's area texture without the U-shape smoothing (see Luma_SMAA_impl.hlsl)
#define SMAA_SMOOTH_U_SHAPES 0

#include "..\..\Core\core.hpp"
#include "MotionVectorPatches.h"
#include "UberLutPatch.h"
#include "UberPixelShaders.h"
#include "PostPixelShaders.h"
#include "ForwardPixelShaders.h"
#include "..\..\Core\includes\patched_draws.h"

#include <d3d9.h> // The game's device (see "GammaRamp")

// Dead Space 2 (2011, Steam) is x86 Visceral engine on D3D9: it runs through dgVoodoo 2.87.6 (D3D9 -> D3D11), so every hash is a
// dgVoodoo ps_5_0 translation (same hashes as 2.87.5, mapped offline from the game's D3D9 shaders).
// HDR: the game has no tonemap and every target is RGBA8. They get fp16 mirrors, the uber post's LUT tail keeps what's above 1
// ("WrapUberLut"), and the display map (Mirror's Edge's) runs where the scene ends, before the UI.

// The light pre-pass' material apply (every PS reading "g_DeferredLightingSampler", the light buffer; 62 D3D9 shaders, 3 dgVoodoo
// specializations each, offline map): the meshes drawn again into the scene color after the G-buffer and the lights. Their target is
// the scene the motion vector draws write, told apart from the G-buffer's normals and the light buffer (all three are output sized
// RGBA8).
constexpr std::array<uint32_t, 186> apply_pixel_shaders = {
   0x009EEF56,
   0x0155E90E,
   0x029C2E42,
   0x02E1106C,
   0x05C3BEDA,
   0x06C19140,
   0x07517C18,
   0x08590425,
   0x090DA438,
   0x09260D24,
   0x0B98D56D,
   0x0E2D3DA5,
   0x14487BF0,
   0x17433A12,
   0x17C8B7F6,
   0x188F3AAC,
   0x19310E33,
   0x1A319D4A,
   0x1B0C195D,
   0x1B2ECD96,
   0x1CC25EF8,
   0x210EE9D0,
   0x232AFD51,
   0x243F5E8B,
   0x24D39A4A,
   0x25CD600D,
   0x27FC5B3F,
   0x2ABAAD42,
   0x2CAC2CE6,
   0x2CF50D75,
   0x2DEF9D7B,
   0x2EF6FDD0,
   0x2FD91D0B,
   0x350A0934,
   0x384D9895,
   0x39DED21E,
   0x3B9CBA8E,
   0x3C0A733E,
   0x3E18912C,
   0x40053029,
   0x43725EA2,
   0x44691E34,
   0x45ADCCAA,
   0x46059A50,
   0x467D8D0E,
   0x473C60A6,
   0x481B82CC,
   0x4B71F1C7,
   0x4BC8B571,
   0x4FFF13D2,
   0x50D17AB8,
   0x51D70DFE,
   0x5234894F,
   0x523C8971,
   0x525FC44F,
   0x5265348D,
   0x55BC4C8C,
   0x5940609B,
   0x5A95B7B0,
   0x5B23FA2B,
   0x5B422861,
   0x5DEF08A9,
   0x5E1E8E5D,
   0x5F9FD3C5,
   0x60533DEA,
   0x62F20231,
   0x645A4A6C,
   0x64D8479B,
   0x65F5BF25,
   0x67B076DD,
   0x6816C3AD,
   0x6A2614AE,
   0x6AE36E49,
   0x6B21775C,
   0x6C07FADA,
   0x6CB87D5C,
   0x6E1C00B4,
   0x6F07A9F7,
   0x7110D5F8,
   0x715A558D,
   0x719D40E0,
   0x72B1EA81,
   0x73736F62,
   0x753E1071,
   0x757EC3AD,
   0x76E28630,
   0x77A69DCA,
   0x7B70CF2D,
   0x7C164AB6,
   0x7CDAA95A,
   0x7CE31AAC,
   0x7E4F8507,
   0x7E9D39CD,
   0x7F380254,
   0x7F8D9112,
   0x80C38E87,
   0x80C4CBAD,
   0x8192DD9C,
   0x845A130A,
   0x867C4BA8,
   0x887719C3,
   0x89CF4618,
   0x8AB5B66F,
   0x8AC85463,
   0x8B6EB5F8,
   0x8D584407,
   0x8DADA89C,
   0x8EFB8929,
   0x8F54436C,
   0x8FC90783,
   0x917DD9A1,
   0x9336C77D,
   0x9607E526,
   0x9A037EEF,
   0x9CE5C445,
   0x9D91E102,
   0xA0EB1A3F,
   0xA0EBDF64,
   0xA16B46E3,
   0xA16D687D,
   0xA190A83A,
   0xA31BFA8F,
   0xA3E788CA,
   0xA412035D,
   0xA4541A52,
   0xA541335D,
   0xA6042F5A,
   0xA6FCFF3B,
   0xA92E5C3A,
   0xAA46C905,
   0xAA5B772F,
   0xAA7EC0FD,
   0xAAE066F8,
   0xAC550A39,
   0xAD72AF3D,
   0xAE70D7E3,
   0xAEDCA9E4,
   0xB323EC9B,
   0xB3A3C8E0,
   0xB7566A98,
   0xB9193D42,
   0xBD27A773,
   0xBF33685A,
   0xBFA73DBC,
   0xC06EE3F1,
   0xC1BC42C7,
   0xC5A06177,
   0xC71FDF10,
   0xC917895F,
   0xCA1169E5,
   0xCA17D265,
   0xCB27F448,
   0xCB7F7464,
   0xCF88AAF8,
   0xD00CE0F3,
   0xD1C933C6,
   0xD25D0D84,
   0xD476A453,
   0xD591D6CD,
   0xD609C9CD,
   0xDBC8E237,
   0xE3141E5A,
   0xE453FB76,
   0xE516C852,
   0xE5CC1263,
   0xE77CB316,
   0xE897AD76,
   0xE9B64F72,
   0xEBA5534F,
   0xED3AD8B4,
   0xEF427B7E,
   0xEF457AC2,
   0xEFF1CA52,
   0xF2CE8A3A,
   0xF47BEDA6,
   0xF51C25BE,
   0xF766B3CF,
   0xF7ED87BF,
   0xF85FDF34,
   0xFAF4A3A8,
   0xFB7C18C3,
   0xFC6AB3BB,
   0xFC88E84C,
   0xFCB8D90A,
   0xFDE9A867,
   0xFE8ABD76,
};
// The light pre-pass' lights (every PS reading the G-buffer's normals "g_nBuffer" and writing "g_lightColor"; 194 D3D9 shaders, 582
// dgVoodoo specializations, offline map): they add into the light buffer, kept above 1 in its fp16 mirror (see "DllMain")
constexpr std::array<uint32_t, 582> light_pixel_shaders = {
   0x001E7616,
   0x00269F38,
   0x008E295F,
   0x00C65254,
   0x01875763,
   0x01A9C51F,
   0x01B14B76,
   0x02B54A12,
   0x03016D8E,
   0x0316E0A5,
   0x032E5D66,
   0x03654DBB,
   0x03A877B3,
   0x03BC2251,
   0x03DBC9B5,
   0x03F2E1CE,
   0x04119162,
   0x041FB85A,
   0x046D333F,
   0x048BC724,
   0x04D9B5FD,
   0x04FD12F9,
   0x053688E0,
   0x05BABD8D,
   0x05E0D315,
   0x06152F85,
   0x064B48BD,
   0x069FFF9A,
   0x0723F635,
   0x0748D6D3,
   0x08C1EFF2,
   0x09A00941,
   0x09BA2923,
   0x0A7BD00B,
   0x0AE4D2D8,
   0x0AEDDD5D,
   0x0BFAAE6E,
   0x0C69258F,
   0x0CB91651,
   0x0E062F74,
   0x0ECCB25C,
   0x0F2266B5,
   0x0F2689F4,
   0x10C3945A,
   0x113A31C0,
   0x113C2431,
   0x12B2E814,
   0x12BB0FCF,
   0x130378ED,
   0x13076D78,
   0x135C3EE7,
   0x1375B2CE,
   0x13ED4EA2,
   0x14084FC7,
   0x143CD175,
   0x1482C390,
   0x14E3E2B9,
   0x150A399B,
   0x15328ADE,
   0x15C43A30,
   0x1628C170,
   0x16893815,
   0x1692E72C,
   0x1718BD3E,
   0x172A0403,
   0x1746B22F,
   0x185B30A4,
   0x18924CEB,
   0x18D9AAEF,
   0x191BA7A5,
   0x19453FBA,
   0x1955FA19,
   0x196FC431,
   0x1B13C22B,
   0x1B38BCEA,
   0x1BC84687,
   0x1BD467AB,
   0x1C6B4C53,
   0x1CCEF35C,
   0x1D2D0A84,
   0x1D2F012E,
   0x1E7E1FC2,
   0x1F08217A,
   0x1F09B08A,
   0x21184E78,
   0x2131A728,
   0x21C992C2,
   0x21CAD289,
   0x21F3F014,
   0x2485D5D5,
   0x251D5001,
   0x25A74CAD,
   0x25B76C58,
   0x25C3ECDB,
   0x269C58A0,
   0x2741C829,
   0x276B1C5D,
   0x27EA6D32,
   0x2A4B2D95,
   0x2A7BC329,
   0x2ABF81E3,
   0x2ACD41A1,
   0x2AD1EBF8,
   0x2B522AB2,
   0x2B795807,
   0x2BA9A3C7,
   0x2BC081A5,
   0x2BCB56AE,
   0x2D08503B,
   0x2D7A09D6,
   0x2DC86B66,
   0x2E874902,
   0x2EAACB98,
   0x2FA08BCC,
   0x2FDB1D87,
   0x305CFE64,
   0x305FFDC6,
   0x3089DBFE,
   0x314E8D9C,
   0x319E7A5B,
   0x31A2C541,
   0x32CC66D2,
   0x339FB3D0,
   0x33BD7C86,
   0x3466A9AA,
   0x34BCC751,
   0x3541E391,
   0x35939E78,
   0x35A9391C,
   0x35B963FA,
   0x365C56C6,
   0x3684994D,
   0x368E2817,
   0x369CF639,
   0x36ADD1DA,
   0x379331AD,
   0x37A55DE1,
   0x37F7E4E2,
   0x380AD781,
   0x38283DD0,
   0x38599015,
   0x387A0852,
   0x38AE85FE,
   0x38D83332,
   0x38E8AB7E,
   0x3A3403FC,
   0x3AEF7B80,
   0x3BC88C9F,
   0x3C26E1DB,
   0x3C970BAE,
   0x3CAA0022,
   0x3CB3CC39,
   0x3D1D66EE,
   0x3E13D7AB,
   0x3E8D02E8,
   0x41077BA9,
   0x41360B1D,
   0x41387845,
   0x416284CA,
   0x41836D53,
   0x4252FF09,
   0x42556C35,
   0x4260DEF7,
   0x42D841C9,
   0x432EF3CC,
   0x4353BCB4,
   0x437EF94A,
   0x43E8AE02,
   0x447841B1,
   0x447CE0B0,
   0x44A86DDE,
   0x44E4435C,
   0x45603999,
   0x46389421,
   0x46C7A3F6,
   0x46FB5C08,
   0x47E35944,
   0x483DF50C,
   0x48A42B0B,
   0x48FFCFB3,
   0x49335398,
   0x4962B3D9,
   0x497BEB1C,
   0x49963B79,
   0x49ADD689,
   0x49EF4624,
   0x4A510505,
   0x4A74D25F,
   0x4B17CCA6,
   0x4BE6E4C3,
   0x4CDD410D,
   0x4CFF83EB,
   0x4D4F49C7,
   0x4E2BB1D5,
   0x4E435BD8,
   0x4EDDDC78,
   0x4F872C03,
   0x5017D1B7,
   0x50C12793,
   0x517817E0,
   0x52151CC5,
   0x527AACB1,
   0x533A9169,
   0x53B9497D,
   0x53D6C11B,
   0x542C9267,
   0x54A78904,
   0x55735ECF,
   0x558D4324,
   0x56526C23,
   0x565B1378,
   0x565D5AFE,
   0x580AE0D0,
   0x583F9491,
   0x58FF2CD0,
   0x5A057BAD,
   0x5A3C7FB7,
   0x5A722FED,
   0x5AA212CA,
   0x5AD8F514,
   0x5B0ECCB6,
   0x5B37A5A9,
   0x5C153AC8,
   0x5C18724E,
   0x5D289E36,
   0x5D713A32,
   0x5D8A892C,
   0x5DE6133A,
   0x5E413469,
   0x5E5C6BC8,
   0x5F2C005D,
   0x5FCCB796,
   0x5FED1F55,
   0x607097B8,
   0x615F3944,
   0x61F2154E,
   0x624A0413,
   0x627319FB,
   0x62EB3221,
   0x63074A31,
   0x63252E75,
   0x634ABE3B,
   0x63C5DD8E,
   0x63C83346,
   0x63ED8A13,
   0x64C2C32D,
   0x64C974EA,
   0x66BB3CF5,
   0x676C7AA7,
   0x6842F504,
   0x68529407,
   0x68715C39,
   0x6A62476A,
   0x6AE1654E,
   0x6AF464D8,
   0x6B1CAE64,
   0x6B3E7920,
   0x6B5033C8,
   0x6B74DC44,
   0x6BB96720,
   0x6C630EAC,
   0x6D09AAE3,
   0x6D0FF683,
   0x6D193884,
   0x6DB7074B,
   0x6E0190C1,
   0x6E7F5E9B,
   0x6ECA105A,
   0x6EF8E5A4,
   0x700E5C87,
   0x714C1F7A,
   0x719F52D5,
   0x72C6D3C0,
   0x749074F0,
   0x75645CEF,
   0x7564D6C8,
   0x76FE79B1,
   0x77B9D6D6,
   0x78C53701,
   0x790CC218,
   0x795E5B0A,
   0x79DFA37B,
   0x7A052DD0,
   0x7A9ABF94,
   0x7AABA118,
   0x7B320AB8,
   0x7B99801F,
   0x7C02BE99,
   0x7C8FB91F,
   0x7DAA54A7,
   0x7DC1CC51,
   0x7DC8645F,
   0x7E4641BC,
   0x7EEE6761,
   0x7EF586DA,
   0x7FFD4C12,
   0x806D46BA,
   0x8236CBD4,
   0x841C2B86,
   0x8467A46C,
   0x848E16B3,
   0x84B12988,
   0x8512E501,
   0x8576F68B,
   0x85F92A55,
   0x869ECC48,
   0x87357574,
   0x878574F9,
   0x88B4DA43,
   0x88BC6CAB,
   0x88D42CB7,
   0x892EBA0D,
   0x89A59EA0,
   0x89CF4403,
   0x89DFB2E5,
   0x8A25AF5A,
   0x8B3F5962,
   0x8B79457D,
   0x8C52998D,
   0x8C604EEE,
   0x8CC46563,
   0x8CD9D1AF,
   0x8CEEB344,
   0x8D5B07D7,
   0x8DFFF6F1,
   0x8E32814A,
   0x8E4D090D,
   0x8EFB7D42,
   0x8F822566,
   0x900F914B,
   0x90ABD998,
   0x91B6123D,
   0x92EC3B4E,
   0x930880AF,
   0x93236693,
   0x93CFF01D,
   0x93E131AC,
   0x94C293D9,
   0x9795406E,
   0x97D2E28D,
   0x9925F02A,
   0x995D8F27,
   0x99D8F155,
   0x9A2D987D,
   0x9AB5773D,
   0x9B4BB438,
   0x9D357CA1,
   0x9E3BE157,
   0x9E9DCB7D,
   0x9EE2913B,
   0x9F39F884,
   0x9FDA5998,
   0x9FF48BBF,
   0xA106EB0D,
   0xA1FA07F2,
   0xA20659D3,
   0xA235252B,
   0xA2E4AABB,
   0xA2EC20F1,
   0xA346942E,
   0xA3A0A759,
   0xA40B41B3,
   0xA4261BDF,
   0xA4D8D952,
   0xA5F00F33,
   0xA69708BE,
   0xA6C5CD1A,
   0xA701098B,
   0xA71C62C8,
   0xA887BC2C,
   0xA8A10CEB,
   0xAA841122,
   0xAB0B5EF0,
   0xAB1C3370,
   0xAB2B101C,
   0xABB31C41,
   0xAC54A785,
   0xAC82C3A1,
   0xACE43B54,
   0xADABE876,
   0xADEB7E0B,
   0xAE10B696,
   0xAED74589,
   0xAED8252D,
   0xAF2E9F1C,
   0xAF42C814,
   0xAF6796FE,
   0xAF7C47C2,
   0xB00EA649,
   0xB01DD0BC,
   0xB05A7FDA,
   0xB0C01CE8,
   0xB1204114,
   0xB12983C9,
   0xB2027CC7,
   0xB2816B63,
   0xB2BC9078,
   0xB2FAB657,
   0xB33E735D,
   0xB400241F,
   0xB41C2DC7,
   0xB4318748,
   0xB447BEB7,
   0xB4934774,
   0xB497ACDC,
   0xB49DC27B,
   0xB49E7410,
   0xB4A6CC40,
   0xB4C37F71,
   0xB4F7DC51,
   0xB543EFF5,
   0xB56B633F,
   0xB5B187CB,
   0xB5FA6367,
   0xB68AEA31,
   0xB6FA581D,
   0xB72A0E32,
   0xB7AD8858,
   0xB7CF4EDB,
   0xB7DF2FF5,
   0xB82A0C60,
   0xB9710E9C,
   0xBA4B91CF,
   0xBA7966FB,
   0xBAB0B0F4,
   0xBACA6962,
   0xBAD4C435,
   0xBB427DA8,
   0xBB79F8C3,
   0xBCA7E4D5,
   0xBCF7BA17,
   0xBD182E2C,
   0xBD544C4C,
   0xBD9AB80D,
   0xBE2A15CB,
   0xBE42E8C0,
   0xBE663DC3,
   0xBE6BC00F,
   0xBEA9B27E,
   0xBEB31CE7,
   0xBF4CFFF9,
   0xBFB512C3,
   0xBFE2BABF,
   0xC14643DE,
   0xC1490EC7,
   0xC186CEBF,
   0xC19A2867,
   0xC1C1ABA2,
   0xC1F3152E,
   0xC26D6826,
   0xC3CD9419,
   0xC40DB496,
   0xC41865F4,
   0xC490A1F1,
   0xC57EB745,
   0xC58B2B69,
   0xC6CED73B,
   0xC6D7A80D,
   0xC6E60AD3,
   0xC77815F5,
   0xC7D1D4B3,
   0xC88C2571,
   0xC8A5E358,
   0xC8A8ED0D,
   0xC8F1BD2D,
   0xC926DD70,
   0xCA240C45,
   0xCA30954B,
   0xCA33C836,
   0xCACD5EBE,
   0xCB570577,
   0xCB61BE43,
   0xCB6D0B2C,
   0xCBADE241,
   0xCBFBF98D,
   0xCC4A50F7,
   0xCD571895,
   0xCDEED32E,
   0xCE20D4EC,
   0xCE44AC7B,
   0xCF30C4E4,
   0xCF5C52DD,
   0xD0787139,
   0xD0C3B9FA,
   0xD1275196,
   0xD174FD85,
   0xD1C47D4C,
   0xD2F26D37,
   0xD32736A1,
   0xD33B311A,
   0xD35FE2AC,
   0xD4C838E4,
   0xD5A6FF6C,
   0xD60CD6A4,
   0xD6E9D226,
   0xD70E99C0,
   0xD77CBD90,
   0xD799BB83,
   0xD7C96560,
   0xD86C4574,
   0xD965BF15,
   0xDA09E5BB,
   0xDA5F90EB,
   0xDA858CC4,
   0xDB39DAAD,
   0xDB50DF8D,
   0xDBA4880F,
   0xDBE7817C,
   0xDC4FBEC4,
   0xDCE55AD1,
   0xDE4E3884,
   0xDF3BA616,
   0xDF47871F,
   0xDF740605,
   0xDFD09FF5,
   0xE0003EB9,
   0xE0F3FD8D,
   0xE1672BEB,
   0xE194A3F5,
   0xE1D2F413,
   0xE358202C,
   0xE3A3ED99,
   0xE400F223,
   0xE401B99B,
   0xE42DDFD9,
   0xE4450028,
   0xE46C7A6B,
   0xE4C3B0AF,
   0xE4EE6338,
   0xE5271816,
   0xE52E5769,
   0xE65910CC,
   0xE76A3E52,
   0xE7F9EBE1,
   0xE88E684E,
   0xE8A77D36,
   0xE8E7143B,
   0xE9D0653A,
   0xEBFAED57,
   0xEC341E59,
   0xEC3E38D6,
   0xEC7CFEE0,
   0xED0E5A15,
   0xED5F79D9,
   0xED8DA1FB,
   0xEEB54D12,
   0xEF2A3B9D,
   0xEF51C4D2,
   0xEF57FBAD,
   0xEFA63463,
   0xEFB2CF82,
   0xEFE5D995,
   0xEFEA4C73,
   0xF077A56F,
   0xF0CFBA12,
   0xF14DA2DA,
   0xF1AB4CBF,
   0xF1FB9DA0,
   0xF294E56B,
   0xF2BA95D3,
   0xF487C600,
   0xF4AA522E,
   0xF4FC0D3E,
   0xF53B66DD,
   0xF5BD656C,
   0xF64578E0,
   0xF7747D68,
   0xF988379A,
   0xF9C62762,
   0xF9F1996E,
   0xFB187448,
   0xFB384DEB,
   0xFB7F439A,
   0xFC072679,
   0xFC897C87,
   0xFCA7BC7C,
   0xFD58647D,
   0xFD63AD82,
   0xFEB98900,
   0xFEE17604,
   0xFFBDA2EA,
   0xFFDDEF5F,
};
// The frame passes Luma replaces (or routes) by hash, see "OnDrawOrDispatch"
constexpr uint32_t VIDEO_PS = 0xD49CB77E;             // VP6 video logs and videos (Video_0xD49CB77E)
constexpr uint32_t MARKERPATCH_FXAA_PS = 0x0A91885E;  // FXAA_0x0A91885E
constexpr uint32_t SMAA_NEIGHBORHOOD_PS = 0xBE49E10A; // MarkerPatch's SMAA neighborhood blending (SMAANeighborhood_0xBE49E10A)
constexpr uint32_t PRESENT_BLIT_PS = 0x880A17D3;      // dgVoodoo's present blit (Present_0x880A17D3)
constexpr uint32_t SSAA_DOWNSAMPLE_PS = 0x113CFB69;   // MarkerPatch's SSAA downsample (SSAADownsample_0x113CFB69)
// The passes after the scene (live traces), the first of which ends it (the fill and the upscaler run before it): the scene
// copy for the uber post, the camera motion blur's downsample, the uber post permutations seen live, MarkerPatch's SMAA and FXAA and
// the video. Without any of them the scene ends at the game's copy of it ("OverrideCopyResource").
const std::unordered_set<uint32_t> scene_end_pixel_shaders = {0x934360AD, 0xDE99AE79, 0x583C9562, 0xC76B708E, 0x8736599C, MARKERPATCH_FXAA_PS, VIDEO_PS};
// MarkerPatch's SMAA edges and weights: skipped once an upscaler antialiased the frame or Luma's SMAA takes its slot (its neighborhood
// blending still draws, for the display map, see "display_mapped")
const std::unordered_set<uint32_t> markerpatch_smaa_pixel_shaders = {0x8736599C, 0x764C97BB};

#if DEVELOPMENT
static bool g_mv_enable = false;       // Motion vectors without an upscaler
static bool g_mv_debug_view = false;   // Core's debug draw of the motion vector target
static bool g_mv_force_jitter = false; // The projection jitter without an upscaler
static bool g_sr_reactive_enable = true;
static float g_sr_reactive_scale = 1.f;      // The alpha blended draws' reactivity, scaled (AMD's default 1)
static float g_sr_reactive_threshold = 0.5f; // Under it 0, over it 0.9 (Mass Effect 2007's and Mirror's Edge's); 0: the scaled reactivity, capped at 0.9
static bool g_sr_reactive_debug_view = false;
#else
constexpr bool g_mv_enable = false;
constexpr bool g_mv_force_jitter = false;
constexpr bool g_sr_reactive_enable = true;
constexpr float g_sr_reactive_scale = 1.f;
constexpr float g_sr_reactive_threshold = 0.5f;
#endif
static bool g_smaa_enable = true;
static bool g_smaa_t2x = false;      // SMAA T2x: a two phase jitter and the previous frame, through the upscalers' motion vectors
static float g_rcas_sharpness = 0.f; // RCAS on the upscaler's or SMAA's output, 0 = off
#if DEVELOPMENT
// Predication from the scene depth, and its plane-deviation tolerance ("Luma_DS2_DepthEdges.hlsl")
static bool g_smaa_predication = true;
static float g_smaa_pred_tolerance = 0.02f;
#else
constexpr bool g_smaa_predication = true;
constexpr float g_smaa_pred_tolerance = 0.02f;
#endif
// SMAA's and T2x's resources and the reactive masks go after this many frames without their pass (MEM-11)
constexpr uint32_t IDLE_RELEASE_FRAMES = 600;

using SceneMotionVectors::PixelShaderPatch;

// A row vector view projection that projects (w depends on the position), not an orthographic transform (2D passes)
template <typename T>
static bool IsProjecting(const T* m)
{
   return std::abs(m[3]) + std::abs(m[7]) + std::abs(m[11]) >= T(0.5);
}

// The game's brightness slider (settings.txt "Window.Gamma", 0.5 = identity) is a D3D9 SetGammaRamp of pow(x, 1.5 - gamma) (FUN_013cb3e0),
// which dgVoodoo forwards to the OS ramp, distorting HDR output. The hook keeps the ramp off the OS and hands its exponent to the
// display map ("GammaRampExponent").
namespace GammaRamp
{
   constexpr size_t SET_GAMMA_RAMP_SLOT = 21; // IDirect3DDevice9 vtable
   using SetGammaRampFunc = void(STDMETHODCALLTYPE*)(IDirect3DDevice9*, UINT, DWORD, const D3DGAMMARAMP*);
   static SetGammaRampFunc original = nullptr;
   static void** patched_slot = nullptr;
   static std::atomic<float> exponent = 1.f;

   // From the ramp's middle entry (16 bit): the slider's range is 0.5-1.5, clamped wider against odd ramps
   static float ExponentOf(uint32_t middle_entry)
   {
      const float ramp_value = (std::max)(middle_entry / 65535.f, 1e-4f);
      return std::clamp(std::log(ramp_value) / std::log(128.f / 255.f), 0.25f, 4.f);
   }

   static void STDMETHODCALLTYPE HookSetGammaRamp(IDirect3DDevice9* device, UINT swap_chain, DWORD flags, const D3DGAMMARAMP* ramp)
   {
      if (ramp)
      {
         exponent = ExponentOf(ramp->red[128]);
      }
   }

   static void Install(IDirect3DDevice9* device)
   {
      void** const vtable = *reinterpret_cast<void***>(device);
      if (vtable[SET_GAMMA_RAMP_SLOT] == reinterpret_cast<void*>(&HookSetGammaRamp))
         return;
      // The ramp the game set before the hook; dgVoodoo's GetGammaRamp returns it divided by 257
      D3DGAMMARAMP ramp = {};
      device->GetGammaRamp(0, &ramp);
      exponent = ExponentOf(ramp.red[128] * 257u);
      original = reinterpret_cast<SetGammaRampFunc>(vtable[SET_GAMMA_RAMP_SLOT]);
      // That ramp already reached the OS: back to identity
      D3DGAMMARAMP identity;
      for (WORD i = 0; i < 256; i++)
      {
         identity.red[i] = identity.green[i] = identity.blue[i] = WORD(i * 257);
      }
      original(device, 0, D3DSGR_NO_CALIBRATION, &identity);
      const void* const hook = reinterpret_cast<const void*>(&HookSetGammaRamp);
      System::PatchMemory(&vtable[SET_GAMMA_RAMP_SLOT], &hook, sizeof(hook), System::PatchMemoryType::Data);
      patched_slot = &vtable[SET_GAMMA_RAMP_SLOT];
   }

   static void Uninstall()
   {
      if (!patched_slot)
         return;
      System::PatchMemory(patched_slot, &original, sizeof(original), System::PatchMemoryType::Data);
      patched_slot = nullptr;
   }
} // namespace GammaRamp

struct DeadSpace2GameDeviceData final : public GameDeviceData
{
   // ---- DLAA / FSR / XeSS / Luma TAA with motion vectors and jitter from patched shaders (Mirror's Edge's path, one per-draw buffer:
   // dgVoodoo's vc4) ----
   // DLSS, FSR and XeSS run in the x64 helper of Core's SR bridge (the game is 32-bit, see "SRBridge.h"), Luma TAA in process.
   // An upscaler is picked and hasn't failed, latched at present for the whole frame
   bool sr_active = false;
   // The HDR display map ran this frame, before the UI: in MarkerPatch's SMAA neighborhood or FXAA replacement, or "DrawDisplayMap".
   // The present blit then doesn't map the frame again (Present_0x880A17D3). Immediate context only.
   bool display_mapped = false;
   // The uber post drew this frame (a full screen pass): its LUT tail ("WrapUberLut") already carries the vanilla clip's hue turn, so
   // the display map then takes no hue from its reference (LumaData.CustomData2, Includes/DisplayMap.hlsl). Immediate context only.
   bool uber_drawn = false;
   // The last post pass' target (the frame: uber, depth of field or motion blur), and the copy of it "DrawDisplayMap" reads
   com_ptr<ID3D11Resource> post_target;
   com_ptr<ID3D11Texture2D> display_map_source;
   com_ptr<ID3D11ShaderResourceView> display_map_source_srv;
   // The upscaler's output goes back into the scene without its alpha, drawn from this view of it
   com_ptr<ID3D11BlendState> sr_rgb_blend_state;
   com_ptr<ID3D11ShaderResourceView> sr_output_srv;
   // The upscalers' input: the scene decoded to linear ("Luma_DS2_SRGamma.hlsl"), RCAS' output afterwards; and the scene's view for it
   com_ptr<ID3D11Texture2D> sr_input;
   com_ptr<ID3D11ShaderResourceView> sr_input_srv;
   com_ptr<ID3D11RenderTargetView> sr_input_rtv;
   com_ptr<ID3D11Resource> sr_scene;
   com_ptr<ID3D11ShaderResourceView> sr_scene_srv;

   // Motion vectors and jitter this frame (an upscaler or the DEV toggle, set at present) and their state machine (Core's)
   SceneMotionVectors::State mv;
   // The upscaler drew the last frame: only then the scene jitters (frames it skips would reach the screen jittered)
   bool mv_jitter_allowed = false;
   float sr_vert_fov = 1.0471976f; // FSR's vertical FOV (radians): the last camera's, 60 degrees until one is seen
   // The upscalers' near and far: the last camera's with a usable projection, Core's defaults until one is seen
   float sr_near_plane = SR::SuperResolutionImpl::DrawData{}.near_plane;
   float sr_far_plane = SR::SuperResolutionImpl::DrawData{}.far_plane;
   // The scene depth (D24S8, dgVoodoo's INTZ texture) and the fill's view of it, kept while the scene depth is the same resource
   com_ptr<ID3D11Resource> scene_depth;
   com_ptr<ID3D11ShaderResourceView> scene_depth_srv;
   // This frame's scene color: the material apply's target (see "apply_pixel_shaders"), null until its first draw. Only draws into it
   // take motion vectors; the G-buffer's and the lights' are only jittered.
   com_ptr<ID3D11Resource> scene_target;
   // The depth view "OpenScene" refused (not output sized), reset at present
   ID3D11DepthStencilView* open_refused_dsv = nullptr;
   // The scene depth was larger than the output this frame (MarkerPatch's SSAA), latched at present into "ssaa_active": T2x can't run
   bool ssaa_drawn = false;
   bool ssaa_active = false;
   // SMAA predication's linearization of the scene depth: 1 / the projection's A and its near, from the last frame camera (0: none
   // seen, plain ULTRA)
   float pred_depth_scale = 0.f;
   float pred_near_plane = 0.f;

   // ---- Luma SMAA (see "RunSMAA") ----
   // "markerpatch_smaa_drawn": MarkerPatch's SMAA neighborhood blending drew this frame (read at present). "smaa_slot_a": this
   // frame's SMAA runs in its slot, else at the scene's end. "aa_drawn": the upscaler or Luma's SMAA antialiased this frame
   // (MarkerPatch's FXAA is then a copy).
   bool markerpatch_smaa_drawn = false;
   bool smaa_slot_a = false;
   bool aa_drawn = false;
#if DEVELOPMENT
   // The MCP counters ("aa.*"): this frame's, and the last complete frame's
   struct AAStats
   {
      uint32_t smaa_slot_a = 0;
      uint32_t smaa_slot_b = 0;
      uint32_t t2x_resolves = 0;
      uint32_t fxaa_copies = 0;
      uint32_t markerpatch_smaa = 0;
      uint32_t ssaa = 0;
      uint32_t smaa_slot_a_chosen = 0;
      uint32_t t2x_active = 0;
   };
   AAStats aa_stats;
   AAStats last_aa_stats;
#endif
   // SMAA metrics CB (b1), predication edge-ness and its CB, written every SMAA frame
   com_ptr<ID3D11Buffer> cb_smaa_metrics;
   com_ptr<ID3D11Buffer> cb_pred;
   com_ptr<ID3D11Texture2D> tex_pred;
   com_ptr<ID3D11UnorderedAccessView> uav_pred;
   com_ptr<ID3D11ShaderResourceView> srv_pred;
   // SMAA's output (fp16, encoded as its input), and RCAS' input when it runs after SMAA
   com_ptr<ID3D11Texture2D> tex_smaa_out;
   com_ptr<ID3D11RenderTargetView> tex_smaa_out_rtv;
   com_ptr<ID3D11ShaderResourceView> tex_smaa_out_srv;
   com_ptr<ID3D11Texture2D> tex_rcas_in;
   com_ptr<ID3D11RenderTargetView> tex_rcas_in_rtv;
   com_ptr<ID3D11ShaderResourceView> tex_rcas_in_srv;
   // The scene's copy SMAA reads at the scene's end (it writes the scene)
   com_ptr<ID3D11Texture2D> tex_snapshot;
   com_ptr<ID3D11ShaderResourceView> srv_snapshot;
   // A Texture2D view of the game's frame (dgVoodoo's views are one-slice arrays), kept while the frame is the same resource
   com_ptr<ID3D11Resource> frame_resource;
   com_ptr<ID3D11ShaderResourceView> frame_srv;
   uint32_t smaa_frame = 0; // The last frame SMAA ran, for "IDLE_RELEASE_FRAMES"

   // SMAA T2x, set at present for the whole frame like "sr_active". The scene jitters only after a frame the resolve ran. The phase is
   // chosen when the scene opens, -1 if it didn't jitter.
   bool t2x_active = false;
   int t2x_phase = -1;
   bool mv_filled = false; // The fill wrote this frame's motion vectors (reset at present)
   // SMAA's output of this frame and of the previous one, alternating: linear RGB with the velocity length in alpha. The previous one
   // is the history only if "t2x_frame", the last frame the resolve ran, was the frame before.
   com_ptr<ID3D11Texture2D> t2x_frames[2];
   com_ptr<ID3D11RenderTargetView> t2x_frame_rtvs[2];
   com_ptr<ID3D11ShaderResourceView> t2x_frame_srvs[2];
   uint32_t t2x_frame = 0;

   void ReleaseT2xFrames()
   {
      for (int i = 0; i < 2; i++)
      {
         t2x_frames[i].reset();
         t2x_frame_rtvs[i].reset();
         t2x_frame_srvs[i].reset();
      }
   }

   // SMAA's own resources, apart from Core's ("ReleaseSMAAIntermediates"); recreated at their next use
   void ReleaseSMAAScratch()
   {
      tex_pred.reset();
      uav_pred.reset();
      srv_pred.reset();
      tex_smaa_out.reset();
      tex_smaa_out_rtv.reset();
      tex_smaa_out_srv.reset();
      tex_rcas_in.reset();
      tex_rcas_in_rtv.reset();
      tex_rcas_in_srv.reset();
      tex_snapshot.reset();
      srv_snapshot.reset();
      frame_resource.reset();
      frame_srv.reset();
   }
};

static DeadSpace2GameDeviceData& GetGameDeviceData(DeviceData& device_data)
{
   return *static_cast<DeadSpace2GameDeviceData*>(device_data.game);
}

static const DeadSpace2GameDeviceData& GetGameDeviceData(const DeviceData& device_data)
{
   return *static_cast<const DeadSpace2GameDeviceData*>(device_data.game);
}

class DeadSpace2 final : public Game
{
   // The frame camera's view projection in vc4 (row vectors, the engine's, see "OpenScene")
   static constexpr size_t VIEW_PROJECTION_OFFSET = MotionVectorPatches::frame_camera.row * 16;

   // The game's motion vector state for Core's buffer and bind events, null before the device data exists
   static SceneMotionVectors::State* GetSceneMotionVectors(reshade::api::device* device)
   {
      DeviceData* const device_data = device->get_private_data<DeviceData>();
      return ((device_data && device_data->game) ? &GetGameDeviceData(*device_data).mv : nullptr);
   }

   // What "SceneMotionVectors" asks of the game
   struct MotionVectorGame
   {
      static constexpr const char* log_tag = "Dead Space 2";
      static constexpr MotionVectorPatch::Layout layout = MotionVectorPatches::layout;
      static constexpr auto cameras = MotionVectorPatches::cameras;
      static constexpr auto tie_break_rows = MotionVectorPatches::tie_break_rows;
      static constexpr auto frame_camera = MotionVectorPatches::frame_camera;
      static constexpr const char* patch_names[size_t(PixelShaderPatch::COUNT)] = {"MV PS", "MV PS (mask)", "MV PS (mask)", "MV PS (blended)", "MV PS (blended, mask)", "", "", ""};
      // The particles are rewritten on the CPU every frame, so they never match their previous selves: their motion is unknown, they
      // write the reactive mask (FSR-Best-Practices FMSK-7, MGRR's and Vanquish's)
      static constexpr PixelShaderPatch blended_mask_patch = PixelShaderPatch::GAME_0;
      // The characters are skinned on the CPU: their vertices are rewritten every frame, their motion comes from the previous ones
      static constexpr uint32_t previous_vertices_slot = MotionVectorPatches::previous_vertices_slot;

      // The scene: the fp16 mirror of the game's RGBA8 target, or the target itself where it isn't upgraded; single sample, with a
      // shader resource view (the upscaler reads it). The caller only lets the material apply's target through.
      static bool AcceptSceneColor(const D3D11_RENDER_TARGET_VIEW_DESC& rtv_desc, const D3D11_TEXTURE2D_DESC& color_desc)
      {
         const bool format = rtv_desc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT || rtv_desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM || rtv_desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
         return format && color_desc.SampleDesc.Count == 1 && (color_desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) != 0;
      }

      static bool IsExcludedPixelShader(uint32_t pixel_shader_hash)
      {
         return false;
      }

      // The main view's camera projects. Its determinant is negative: the engine's projection mirrors x (_11 < 0). Row vectors: w is
      // each row's 4th value.
      static bool AcceptFrameCamera(const std::vector<uint8_t>& constants)
      {
         const Math::Matrix44D view_projection = SceneMotionVectors::ReadMatrix(constants, VIEW_PROJECTION_OFFSET);
         return IsProjecting(view_projection.GetData());
      }

      // The upscalers that read the reactive mask the alpha blended draws write: FSR and Luma TAA (DLSS's current presets ignore it,
      // DLSS-Best-Practices TRN-2)
      static bool ReactiveMaskUsed(const DeviceData& device_data)
      {
         return g_sr_reactive_enable && GetGameDeviceData(device_data).sr_active && (device_data.sr_type == SR::Type::FSR || device_data.sr_type == SR::Type::LumaTAA);
      }

      static bool MaskUnmatchedBlended(DeviceData& device_data)
      {
         return ReactiveMaskUsed(device_data);
      }

      static bool CanFill(DeviceData& device_data)
      {
         return GetGameDeviceData(device_data).scene_depth_srv && HasShaders(device_data.native_compute_shaders, CompileTimeStringHash("DS2 Motion Vector Fill CS"));
      }

      // The marker texels' camera motion from the opaque scene's depth (the end of the scene fills the rest and the masks)
      static void FillMarkersBeforeBlend(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data)
      {
         FillMotionVectors(native_device, native_device_context, device_data, /* write_reactive */ false);
      }

      // A game pixel shader's "patch", from the bytecode its shader object was created with; empty with "error" if it can't be patched
      static std::vector<uint8_t> PatchPixelShader(PixelShaderPatch patch, const uint8_t* code, size_t code_size, std::string* error)
      {
         switch (patch)
         {
         case PixelShaderPatch::MOTION_VECTORS:
            return MotionVectorPatch::PatchPixelShader(code, code_size, MotionVectorPatches::layout, error, /* targets_only */ true);
         case PixelShaderPatch::MASK_ALPHA:
         case PixelShaderPatch::MASK_ADDITIVE:
            return MotionVectorPatch::PatchPixelShaderReactive(code, code_size, MotionVectorPatches::layout, MotionVectorPatches::reactive_slot,
               (patch == PixelShaderPatch::MASK_ADDITIVE ? MotionVectorPatch::ReactiveMode::ADDITIVE : MotionVectorPatch::ReactiveMode::ALPHA), error);
         case PixelShaderPatch::BLENDED:
            return MotionVectorPatch::PatchPixelShaderReactive(code, code_size, MotionVectorPatches::layout, UINT32_MAX, MotionVectorPatch::ReactiveMode::ALPHA_MOTION_VECTORS, error);
         case blended_mask_patch:
            return MotionVectorPatch::PatchPixelShaderReactive(code, code_size, MotionVectorPatches::layout, MotionVectorPatches::reactive_slot, MotionVectorPatch::ReactiveMode::ALPHA_MOTION_VECTORS, error);
         }
         return {};
      }
   };

   // The frame camera: gtGlobals' view projection (TS_GlobalVariables::viewProj), the immediate transform state the renderer uploads
   // from, current on the D3D thread at the G-buffer's first draw (the opaque draws read only their own ModelViewProj). Found through
   // its getter (013cf4b0: mov eax, 0x0211d340 + 0x1c0; ret; no ASLR, the DRM zeroes the TimeDateStamp); null on another build.
   static const float* GetEngineViewProjection()
   {
      static const float* const engine_view_projection = []() -> const float*
      {
         constexpr uint8_t getter_code[] = {0xB8, 0x00, 0xD5, 0x11, 0x02, 0xC3};
         if (reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)) != 0x400000 || std::memcmp(reinterpret_cast<const void*>(0x13cf4b0), getter_code, sizeof(getter_code)) != 0)
         {
            reshade::log::message(reshade::log::level::warning, "[Dead Space 2] Unknown exe build (not the Steam one): no camera, DLAA, FSR, XeSS and Luma TAA stay off");
            return nullptr;
         }
         return reinterpret_cast<const float*>(0x211d500);
      }();
      return engine_view_projection;
   }

   // The scene depth (the G-buffer's), from the scene's open or, without motion vectors, the material apply. The fill and SMAA's
   // predication read it through their own view (the game samples dgVoodoo's INTZ copy of it; DoF reads it like this), kept while it
   // is the same resource; the predication linearizes it with the frame camera's projection.
   static void TrackSceneDepth(ID3D11Device* native_device, DeadSpace2GameDeviceData* gd, ID3D11Resource* depth)
   {
      if (depth != gd->scene_depth.get())
      {
         gd->scene_depth = depth;
         gd->scene_depth_srv.reset();
         const D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(D3D11_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_R24_UNORM_X8_TYPELESS);
         native_device->CreateShaderResourceView(depth, &srv_desc, &gd->scene_depth_srv);
      }
      const float* const view_projection = GetEngineViewProjection();
      // An orthographic transform left current would give a meaningless A
      if (!view_projection || !IsProjecting(view_projection))
         return;
      const SR::ViewProjectionCamera camera = SR::GetViewProjectionCamera(view_projection, /* row_vectors */ true);
      if (camera.depth_a > 0.0 && camera.near_plane > 0.0)
      {
         gd->pred_depth_scale = float(1.0 / camera.depth_a);
         gd->pred_near_plane = float(camera.near_plane);
      }
   }

   // A Texture2D view of the game's bound "game_srv" (dgVoodoo's are one-slice Texture2DArray views, which Luma's shaders don't
   // declare), in its format
   static ID3D11ShaderResourceView* GetFrameView(ID3D11Device* native_device, DeadSpace2GameDeviceData* gd, ID3D11ShaderResourceView* game_srv)
   {
      com_ptr<ID3D11Resource> resource;
      game_srv->GetResource(&resource);
      if (resource != gd->frame_resource)
      {
         gd->frame_resource = resource;
         gd->frame_srv.reset();
         D3D11_SHADER_RESOURCE_VIEW_DESC game_desc = {};
         game_srv->GetDesc(&game_desc);
         const D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(D3D11_SRV_DIMENSION_TEXTURE2D, game_desc.Format, 0, 1);
         native_device->CreateShaderResourceView(resource.get(), &srv_desc, &gd->frame_srv);
      }
      return gd->frame_srv.get();
   }

   // SMAA ULTRA, 1x or T2x, then RCAS, of "source_srv" into "tex_smaa_out" (Mirror's Edge's chain: predication CS -> DrawSMAA -> T2x
   // resolve -> RCAS). "source_srv" isn't written: sRGB encoded in MarkerPatch's slot ("slot_a"), else gamma 2.2; the output is encoded
   // the same way. The caller caches and restores the state. False if it didn't draw.
   static bool RunSMAA(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, ID3D11ShaderResourceView* source_srv, bool slot_a)
   {
      auto& gd = GetGameDeviceData(device_data);
      const uint2 size = GetViewTextureSize(source_srv);
      if (size.x == 0 || size.y == 0)
         return false;

      // "DrawSMAA" looks its passes up with ".at()": a shader reload must not release them mid-chain (Sunset Overdrive)
      const std::shared_lock lock_shader_objects(s_mutex_shader_objects);
      auto* const copy_vs = FindShader(device_data.native_vertex_shaders, CompileTimeStringHash("Copy VS"));
      // All or nothing: skip SMAA this frame while any pass is missing (async load, live reload)
      if (!copy_vs || !HasSMAAShaders(device_data))
         return false;
      const uint32_t frame_index = cb_luma_global_settings.FrameIndex;
      gd.smaa_frame = frame_index;

      const auto create_target = [&](com_ptr<ID3D11Texture2D>* texture, com_ptr<ID3D11RenderTargetView>* rtv, com_ptr<ID3D11ShaderResourceView>* srv)
      {
         if (*srv && GetViewTextureSize(srv->get()) == size)
            return true;
         texture->reset();
         rtv->reset();
         srv->reset();
         const D3D11_TEXTURE2D_DESC desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R16G16B16A16_FLOAT, size.x, size.y, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);
         if (FAILED(native_device->CreateTexture2D(&desc, nullptr, &(*texture))) || FAILED(native_device->CreateRenderTargetView(texture->get(), nullptr, &(*rtv))) ||
             FAILED(native_device->CreateShaderResourceView(texture->get(), nullptr, &(*srv))))
         {
            texture->reset();
            rtv->reset();
            srv->reset();
            return false;
         }
         return true;
      };
      if (!create_target(std::addressof(gd.tex_smaa_out), std::addressof(gd.tex_smaa_out_rtv), std::addressof(gd.tex_smaa_out_srv)))
         return false;
      const bool sharpen = g_rcas_sharpness > 0.f && PrepareRCAS(native_device, device_data) && create_target(std::addressof(gd.tex_rcas_in), std::addressof(gd.tex_rcas_in_rtv), std::addressof(gd.tex_rcas_in_srv));

      // The depth and the motion vectors may be bound as targets (an SRV of a bound target reads as null)
      native_device_context->OMSetRenderTargets(0, nullptr, nullptr);

      // Predication from the scene depth, which maps 1:1 only at the same size. Plain ULTRA when any input is missing (never scale 2.0
      // with a null texture)
      auto* const pred_cs = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("DS2 Depth Edges CS"));
      bool pred_ok = g_smaa_predication && pred_cs && gd.scene_depth_srv && gd.pred_depth_scale > 0.f && GetViewTextureSize(gd.scene_depth_srv.get()) == size;
      if (pred_ok)
      {
         const CB::DepthEdgesConstants constants = {.tolerance = g_smaa_pred_tolerance, .depth_scale = gd.pred_depth_scale, .near_plane = gd.pred_near_plane};
         pred_ok = PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd.cb_pred), &constants, sizeof(constants));
         if (!gd.srv_pred || GetViewTextureSize(gd.srv_pred.get()) != size)
         {
            gd.tex_pred.reset();
            gd.uav_pred.reset();
            gd.srv_pred.reset();
            const D3D11_TEXTURE2D_DESC desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R16_FLOAT, size.x, size.y, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
            if (SUCCEEDED(native_device->CreateTexture2D(&desc, nullptr, &gd.tex_pred)))
            {
               native_device->CreateUnorderedAccessView(gd.tex_pred.get(), nullptr, &gd.uav_pred);
               native_device->CreateShaderResourceView(gd.tex_pred.get(), nullptr, &gd.srv_pred);
            }
         }
         pred_ok = pred_ok && gd.uav_pred && gd.srv_pred;
      }
      // Unbinding the mask's UAV before "DrawSMAA" is what makes it readable: an SRV of a resource still bound as a UAV reads as null
      if (pred_ok)
      {
         ID3D11ShaderResourceView* const cs_srv = gd.scene_depth_srv.get();
         ID3D11UnorderedAccessView* const cs_uav = gd.uav_pred.get();
         ID3D11Buffer* const cs_cb = gd.cb_pred.get();
         native_device_context->CSSetUnorderedAccessViews(0, 1, &cs_uav, nullptr);
         native_device_context->CSSetShaderResources(0, 1, &cs_srv);
         native_device_context->CSSetConstantBuffers(0, 1, &cs_cb);
         native_device_context->CSSetShader(pred_cs, nullptr, 0);
         native_device_context->Dispatch((size.x + 7) / 8, (size.y + 7) / 8, 1);
         ID3D11UnorderedAccessView* const null_uav = nullptr;
         ID3D11ShaderResourceView* const null_srv = nullptr;
         native_device_context->CSSetUnorderedAccessViews(0, 1, &null_uav, nullptr);
         native_device_context->CSSetShaderResources(0, 1, &null_srv);
      }

      // SMAA T2x: SMAA into this frame's linear target with the velocity, then the resolve with the previous frame into the output. It
      // needs this frame's motion vectors from the fill, at the frame's size. Without them this frame is 1x and the next one doesn't
      // jitter.
      auto* const t2x_weight_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("DS2 SMAA T2x Blending Weight Calculation PS"));
      auto* const t2x_blend_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("DS2 SMAA T2x Neighborhood Blending PS"));
      auto* const t2x_resolve_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("DS2 SMAA T2x Resolve PS"));
      bool t2x = gd.t2x_active && gd.mv_filled && gd.mv.srv && t2x_weight_ps && t2x_blend_ps && t2x_resolve_ps && GetViewTextureSize(gd.mv.srv.get()) == size;
      const int t2x_current = int(frame_index & 1);
      bool t2x_history = gd.t2x_frames[0] && gd.t2x_frame + 1 == frame_index;
      if (t2x && GetViewTextureSize(gd.t2x_frame_srvs[0].get()) != size)
      {
         t2x_history = false;
         for (int i = 0; i < 2 && t2x; i++)
         {
            t2x = create_target(std::addressof(gd.t2x_frames[i]), std::addressof(gd.t2x_frame_rtvs[i]), std::addressof(gd.t2x_frame_srvs[i]));
         }
         if (!t2x)
         {
            gd.ReleaseT2xFrames();
         }
      }

      // b1: the metrics, the predication threshold scale, the source's encoding and T2x's subsample indices for the jitter phase (see
      // "OpenScene"), 0 for 1x
      const float subsample_indices = ((t2x && gd.t2x_phase >= 0) ? float(gd.t2x_phase + 1) : 0.f);
      const CB::SmaaMetrics metrics = {
         .rt_metrics = {1.f / float(size.x), 1.f / float(size.y), float(size.x), float(size.y)},
         .predication = {(pred_ok ? 2.f : 1.f), (slot_a ? 1.f : 0.f), 0.f, 0.f},
         .subsample_indices = {subsample_indices, subsample_indices, subsample_indices, 0.f},
      };
      if (!PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd.cb_smaa_metrics), &metrics, sizeof(metrics)))
         return false;
      // Metrics at VS+PS b1, bound here rather than through "DrawSMAA()" (which restores the slot): the T2x resolve reads them too
      ID3D11Buffer* const metrics_cb = gd.cb_smaa_metrics.get();
      native_device_context->VSSetConstantBuffers(1, 1, &metrics_cb);
      native_device_context->PSSetConstantBuffers(1, 1, &metrics_cb);

      ID3D11RenderTargetView* const output_rtv = (sharpen ? gd.tex_rcas_in_rtv.get() : gd.tex_smaa_out_rtv.get());
      const SMAAT2xPasses t2x_passes = {.blending_weight_calculation_ps = t2x_weight_ps, .neighborhood_blending_ps = t2x_blend_ps, .velocity = gd.mv.srv.get()};
      DrawSMAA(native_device, native_device_context, device_data, (t2x ? gd.t2x_frame_rtvs[t2x_current].get() : output_rtv), source_srv /* neighborhood blend (decoded) */,
         source_srv /* edge detection (encoded) */, (pred_ok ? gd.srv_pred.get() : nullptr), nullptr /* metrics, bound above */, (t2x ? &t2x_passes : nullptr));

      if (t2x)
      {
         // The resolve: t0 this frame, t1 the previous one or this one again without a history, t2 the motion vectors, s0 linear and
         // s1 point
         native_device_context->OMSetRenderTargets(0, nullptr, nullptr);
         ID3D11ShaderResourceView* const resolve_srvs[2] = {gd.t2x_frame_srvs[t2x_history ? (t2x_current ^ 1) : t2x_current].get(), gd.mv.srv.get()};
         native_device_context->PSSetShaderResources(1, 2, resolve_srvs);
         ID3D11SamplerState* const resolve_samplers[2] = {device_data.sampler_state_linear.get(), device_data.sampler_state_point.get()};
         native_device_context->PSSetSamplers(0, 2, resolve_samplers);
         DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr, copy_vs, t2x_resolve_ps,
            gd.t2x_frame_srvs[t2x_current].get(), output_rtv, size.x, size.y, false);
         ID3D11ShaderResourceView* const null_srvs[3] = {};
         native_device_context->PSSetShaderResources(0, 3, null_srvs);
         gd.t2x_frame = frame_index;
#if DEVELOPMENT
         gd.aa_stats.t2x_resolves++;
#endif
      }

      if (sharpen)
      {
         DrawRCAS(native_device_context, device_data, gd.tex_rcas_in_srv.get(), gd.tex_smaa_out_rtv.get(), g_rcas_sharpness);
      }
      return true;
   }

   // Opens the scene at the frame's first mesh draw into output sized depth (the G-buffer's; shadows and the environment cube have their
   // own): takes the scene depth, rolls the last frame's camera and objects over and picks the jitter the whole scene draws with. Once
   // per present.
   static void OpenScene(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, const CommandListData& cmd_list_data, DeviceData& device_data, uint32_t vertex_shader_hash, ID3D11DepthStencilView* dsv)
   {
      auto& gd = GetGameDeviceData(device_data);
      com_ptr<ID3D11Resource> depth;
      dsv->GetResource(&depth);
      D3D11_TEXTURE2D_DESC depth_desc = {};
      if (com_ptr<ID3D11Texture2D> depth_texture; SUCCEEDED(depth->QueryInterface(&depth_texture)))
      {
         depth_texture->GetDesc(&depth_desc);
      }
      const uint2 depth_size = {depth_desc.Width, depth_desc.Height};
      if (depth_desc.SampleDesc.Count != 1 || depth_desc.Format != DXGI_FORMAT_R24G8_TYPELESS || depth_size.x != uint32_t(device_data.output_resolution.x) || depth_size.y != uint32_t(device_data.output_resolution.y))
      {
         static bool ssaa_logged = false;
         if (depth_desc.Format == DXGI_FORMAT_R24G8_TYPELESS && depth_size.x > uint32_t(device_data.output_resolution.x))
         {
            gd.ssaa_drawn = true;
            if (!std::exchange(ssaa_logged, true))
            {
               reshade::log::message(reshade::log::level::warning, "[Dead Space 2] MarkerPatch's SSAA is on (ImprovedAntiAliasingMode 3 or 4): DLAA, FSR, XeSS, Luma TAA and SMAA T2x stay off");
            }
         }
         gd.open_refused_dsv = dsv;
         return;
      }
      UINT viewport_count = 1;
      D3D11_VIEWPORT viewport = {};
      native_device_context->RSGetViewports(&viewport_count, &viewport);
      if (viewport_count == 0 || viewport.Width != float(depth_size.x) || viewport.Height != float(depth_size.y) || !SceneMotionVectors::GetPatchedVertexShader<MotionVectorGame>(&gd.mv, native_device, cmd_list_data, device_data, vertex_shader_hash).shader)
         return;
      TrackSceneDepth(native_device, &gd, depth.get());
      // Halton (2, 3) over the upscaler's phase count; pixels to NDC (y up). None until the upscaler is ready (the bridge's helper
      // starting shows the scene as it is).
      const SR::InstanceData* sr_instance_data = (gd.sr_active ? device_data.GetSRInstanceData() : nullptr);
      if (sr_instance_data && !sr_implementations[device_data.sr_type]->IsReady(sr_instance_data))
      {
         sr_instance_data = nullptr;
      }
      std::array<float, 2> jitter = (((sr_instance_data && gd.mv_jitter_allowed) || g_mv_force_jitter) ? GetSRJitter(device_data, sr_instance_data) : std::array<float, 2>{});
      // SMAA T2x: the sample offsets (0.25, 0.25) then (-0.25, -0.25) in y down pixels, for "SMAA.hlsl"'s T2x subsample indices 1 and
      // 2 (Mirror's Edge's). Only after a frame the resolve ran.
      gd.t2x_phase = ((gd.t2x_active && gd.t2x_frames[0] && gd.t2x_frame + 1 == cb_luma_global_settings.FrameIndex) ? int(cb_luma_global_settings.FrameIndex & 1) : -1);
      if (gd.t2x_phase >= 0)
      {
         const float shift = (gd.t2x_phase == 0 ? -0.25f : 0.25f);
         jitter = {shift, shift};
      }
      if (!SceneMotionVectors::BeginFrame(&gd.mv, native_device, native_device_context, dsv, depth_size, jitter))
      {
         gd.t2x_phase = -1;
         return;
      }
      if (const float* const engine_view_projection = GetEngineViewProjection())
      {
         auto camera = std::make_shared<std::vector<uint8_t>>(VIEW_PROJECTION_OFFSET + SceneMotionVectors::CAMERA_SIZE);
         std::memcpy(camera->data() + VIEW_PROJECTION_OFFSET, engine_view_projection, SceneMotionVectors::CAMERA_SIZE);
         SceneMotionVectors::SetFrameCamera<MotionVectorGame>(&gd.mv, std::move(camera));
      }
   }

   // DLAA, FSR, XeSS or Luma TAA on the jittered scene, its depth (from the fill) and the motion vectors; the result goes back into the
   // scene's color, its alpha kept. False if it didn't draw (missing input, or the upscaler failed). "reactive_mask": the fill wrote
   // this scene's masks.
   static bool DrawUpscaler(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, bool reactive_mask)
   {
      auto& gd = GetGameDeviceData(device_data);
      auto* const copy_vs = FindShader(device_data.native_vertex_shaders, CompileTimeStringHash("Copy VS"));
      auto* const decode_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("DS2 SR Decode PS"));
      auto* const encode_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("DS2 SR Encode PS"));
      if (!gd.mv.texture || !gd.mv.camera || !gd.mv.device_depth || !gd.mv.scene_color || !gd.mv.scene_rtv || !copy_vs || !decode_ps || !encode_ps)
         return false;
      com_ptr<ID3D11Texture2D> scene;
      if (FAILED(gd.mv.scene_color->QueryInterface(&scene)))
         return false;
      D3D11_TEXTURE2D_DESC scene_desc = {};
      scene->GetDesc(&scene_desc);
      SR::InstanceData* const sr_instance_data = device_data.GetSRInstanceData();
      if (!sr_instance_data)
         return false;

      // The scene's view for the decode, in its render target view's format (the target may be typeless)
      if (gd.sr_scene != gd.mv.scene_color)
      {
         gd.sr_scene = gd.mv.scene_color;
         gd.sr_scene_srv.reset();
         D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {};
         gd.mv.scene_rtv->GetDesc(&rtv_desc);
         const D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(D3D11_SRV_DIMENSION_TEXTURE2D, rtv_desc.Format, 0, 1);
         native_device->CreateShaderResourceView(gd.sr_scene.get(), &srv_desc, &gd.sr_scene_srv);
      }
      D3D11_TEXTURE2D_DESC output_desc = {};
      if (device_data.sr_output_color)
      {
         device_data.sr_output_color->GetDesc(&output_desc);
      }
      if (output_desc.Width != scene_desc.Width || output_desc.Height != scene_desc.Height || !gd.sr_input)
      {
         device_data.sr_output_color.reset();
         gd.sr_output_srv.reset();
         gd.sr_input.reset();
         gd.sr_input_srv.reset();
         gd.sr_input_rtv.reset();
         output_desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R16G16B16A16_FLOAT, scene_desc.Width, scene_desc.Height, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
         if (SUCCEEDED(SRBridge::CreateSharableTexture(native_device, output_desc, &device_data.sr_output_color)))
         {
            native_device->CreateShaderResourceView(device_data.sr_output_color.get(), nullptr, &gd.sr_output_srv);
         }
         const D3D11_TEXTURE2D_DESC input_desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R16G16B16A16_FLOAT, scene_desc.Width, scene_desc.Height, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);
         if (SUCCEEDED(SRBridge::CreateSharableTexture(native_device, input_desc, &gd.sr_input)))
         {
            native_device->CreateShaderResourceView(gd.sr_input.get(), nullptr, &gd.sr_input_srv);
            native_device->CreateRenderTargetView(gd.sr_input.get(), nullptr, &gd.sr_input_rtv);
         }
      }
      if (!device_data.sr_output_color || !gd.sr_output_srv || !gd.sr_input_srv || !gd.sr_input_rtv || !gd.sr_scene_srv)
      {
         device_data.sr_suppressed = true;
         return false;
      }
      if (!gd.sr_rgb_blend_state)
         return false;

      const SR::ViewProjectionCamera camera = SR::GetViewProjectionCamera(reinterpret_cast<const float*>(gd.mv.camera->data() + VIEW_PROJECTION_OFFSET), /* row_vectors */ true);
      const SR::SettingsData settings_data = {
         .output_width = scene_desc.Width,
         .output_height = scene_desc.Height,
         .render_width = scene_desc.Width,
         .render_height = scene_desc.Height,
         // The fp16 mirror of the game's RGBA8 scene (1 = SDR white, the values above it kept by the upgrade), decoded to linear
         .hdr = true,
         // The motion vectors are UV deltas, previous minus current
         .mvs_x_scale = float(scene_desc.Width),
         .mvs_y_scale = float(scene_desc.Height),
         .auto_exposure = device_data.sr_type != SR::Type::FSR,
         // DLAA on HDR input takes preset J or K, never L or M (DLSS-Best-Practices PRE-4): "Default" picks K (11); an explicit choice
         // is kept
         .render_preset = (dlss_render_preset != 0 ? dlss_render_preset : 11u),
      };
      sr_implementations[device_data.sr_type]->UpdateSettings(sr_instance_data, native_device_context, settings_data);

      // FSR requires a FOV (it errors on 0): a camera without an up axis or a usable projection keeps the last ones
      if (camera.vert_fov > 0.0)
      {
         gd.sr_vert_fov = float(camera.vert_fov);
      }
      if (camera.near_plane > 0.0)
      {
         gd.sr_near_plane = float(camera.near_plane);
         gd.sr_far_plane = float(camera.far_plane);
      }
      DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr, copy_vs, decode_ps,
         gd.sr_scene_srv.get(), gd.sr_input_rtv.get(), scene_desc.Width, scene_desc.Height, true);
      const SR::SuperResolutionImpl::DrawData draw_data = {
         .reset = device_data.force_reset_sr,
         .output_color = device_data.sr_output_color.get(),
         .source_color = gd.sr_input.get(),
         .motion_vectors = gd.mv.texture.get(),
         .depth_buffer = gd.mv.device_depth.get(),
         .bias_mask = (reactive_mask ? gd.mv.reactive.get() : nullptr),
         .transparency_alpha = (reactive_mask ? gd.mv.transparency.get() : nullptr),
         // As applied (pixels, +y down)
         .jitter_x = gd.mv.jitter[0],
         .jitter_y = gd.mv.jitter[1],
         .vert_fov = gd.sr_vert_fov,
         .near_plane = gd.sr_near_plane,
         .far_plane = gd.sr_far_plane,
      };
      if (!sr_implementations[device_data.sr_type]->Draw(sr_instance_data, native_device_context, draw_data))
      {
         device_data.sr_suppressed = true;
         return false;
      }
      // RCAS on the linear output into the input (the upscaler is done reading it), then encoded back into the scene. The RGB write mask
      // keeps the scene's alpha (unknown use, not opaque).
      ID3D11ShaderResourceView* encode_source = gd.sr_output_srv.get();
      if (g_rcas_sharpness > 0.f && PrepareRCAS(native_device, device_data))
      {
         DrawRCAS(native_device_context, device_data, gd.sr_output_srv.get(), gd.sr_input_rtv.get(), g_rcas_sharpness);
         encode_source = gd.sr_input_srv.get();
      }
      DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), gd.sr_rgb_blend_state.get(), nullptr, copy_vs, encode_ps,
         encode_source, gd.mv.scene_rtv.get(), scene_desc.Width, scene_desc.Height, true);
      // Not while the bridge's helper starts (the color copied as it is): the next frame resets
      device_data.has_drawn_sr = sr_implementations[device_data.sr_type]->IsReady(sr_instance_data);
      return true;
   }

   // The depth and camera motion fill (see "Luma_DS2_MotionVectorFill.hlsl"), with the reactive masks if "write_reactive". The caller
   // caches and restores the state. False if it didn't run.
   static bool FillMotionVectors(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data, bool write_reactive)
   {
      auto& gd = GetGameDeviceData(device_data);
      auto* const fill_shader = FindShader(device_data.native_compute_shaders, CompileTimeStringHash("DS2 Motion Vector Fill CS"));
      if (!fill_shader || !gd.mv.uav || !gd.mv.device_depth_uav || !gd.mv.camera || !gd.scene_depth_srv)
         return false;
      // The upscalers' depth as an infinite projection with the same near (FSR-Best-Practices FCAM-2): device z / the projection's A
      const double depth_a = SR::GetViewProjectionCamera(reinterpret_cast<const float*>(gd.mv.camera->data() + VIEW_PROJECTION_OFFSET), /* row_vectors */ true).depth_a;
      CB::MotionVectorFillConstants constants = {
         .jitter_ndc = {gd.mv.jitter_ndc[0], gd.mv.jitter_ndc[1]},
         .reactive_scale = g_sr_reactive_scale,
         .reactive_threshold = g_sr_reactive_threshold,
         .reactive_enabled = (write_reactive ? 1.f : 0.f),
         .depth_scale = (depth_a > 0.0 ? float(1.0 / depth_a) : 1.f),
      };
      for (int i = 0; i < 16; i++)
      {
         constants.reprojection.GetData()[i] = float(gd.mv.reprojection.GetData()[i]);
      }
      if (!PatchedDraws::WriteDynamicConstants(native_device, native_device_context, std::addressof(gd.mv.fill_buffer), &constants, sizeof(constants)))
         return false;
      // The depth and the motion vectors may be bound as targets
      native_device_context->OMSetRenderTargets(0, nullptr, nullptr);
      ID3D11Buffer* const buffer = gd.mv.fill_buffer.get();
      ID3D11ShaderResourceView* const srvs[2] = {gd.scene_depth_srv.get(), (write_reactive ? gd.mv.reactive_target_srv.get() : nullptr)};
      ID3D11UnorderedAccessView* const uavs[4] = {gd.mv.uav.get(), gd.mv.device_depth_uav.get(), (write_reactive ? gd.mv.reactive_uav.get() : nullptr), (write_reactive ? gd.mv.transparency_uav.get() : nullptr)};
      native_device_context->CSSetConstantBuffers(0, 1, &buffer);
      native_device_context->CSSetShaderResources(0, UINT(std::size(srvs)), srvs);
      native_device_context->CSSetUnorderedAccessViews(0, UINT(std::size(uavs)), uavs, nullptr);
      native_device_context->CSSetShader(fill_shader, nullptr, 0);
      const uint2 size = gd.mv.scene_size;
      native_device_context->Dispatch((size.x + 7) / 8, (size.y + 7) / 8, 1);
      ID3D11UnorderedAccessView* const null_uavs[std::size(uavs)] = {};
      ID3D11ShaderResourceView* const null_srvs[std::size(srvs)] = {};
      native_device_context->CSSetUnorderedAccessViews(0, UINT(std::size(null_uavs)), null_uavs, nullptr);
      native_device_context->CSSetShaderResources(0, UINT(std::size(null_srvs)), null_srvs);
      gd.mv_filled = true;
      if (write_reactive)
      {
         gd.mv.sr_reactive_frame = cb_luma_global_settings.FrameIndex;
      }
      return true;
   }

   // Ends the scene at the first pass after it: the fill (the last camera motion, the depth, the reactive masks), then the upscaler,
   // both before that pass reads the scene
   static void EndScene(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, DeviceData& device_data)
   {
      auto& gd = GetGameDeviceData(device_data);
      gd.mv.scene_open = false;
      gd.mv.scene_done = true;
      DrawStateStack<DrawStateStackType::FullGraphics> graphics_state;
      DrawStateStack<DrawStateStackType::Compute> compute_state;
      graphics_state.Cache(native_device_context, device_data.uav_max_count);
      compute_state.Cache(native_device_context, device_data.uav_max_count);
      // The reactive and transparency & composition masks, written by the fill from what the alpha blended draws wrote
      const bool reactive = MotionVectorGame::ReactiveMaskUsed(device_data) && gd.mv.reactive_target_srv;
      const uint2 size = gd.mv.scene_size;
      if (reactive && GetViewTextureSize(gd.mv.reactive_target_srv.get()) == size)
      {
         D3D11_TEXTURE2D_DESC desc = {};
         if (gd.mv.reactive)
         {
            gd.mv.reactive->GetDesc(&desc);
         }
         if (desc.Width != size.x || desc.Height != size.y)
         {
            const std::unique_lock lock(gd.mv.mutex);
            gd.mv.reactive.reset();
            gd.mv.reactive_uav.reset();
            gd.mv.transparency.reset();
            gd.mv.transparency_uav.reset();
            desc = CD3D11_TEXTURE2D_DESC(DXGI_FORMAT_R8_UNORM, size.x, size.y, 1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS);
            if (SUCCEEDED(SRBridge::CreateSharableTexture(native_device, desc, &gd.mv.reactive)))
            {
               native_device->CreateUnorderedAccessView(gd.mv.reactive.get(), nullptr, &gd.mv.reactive_uav);
            }
            if (SUCCEEDED(SRBridge::CreateSharableTexture(native_device, desc, &gd.mv.transparency)))
            {
               native_device->CreateUnorderedAccessView(gd.mv.transparency.get(), nullptr, &gd.mv.transparency_uav);
            }
         }
      }
      const bool write_reactive = reactive && gd.mv.reactive_uav && gd.mv.transparency_uav;
      const bool filled = std::exchange(gd.mv.fill_pending, false) && FillMotionVectors(native_device, native_device_context, device_data, write_reactive);
      // The upscaler's depth comes from the fill
      if (gd.sr_active && filled)
      {
         [[maybe_unused]] const bool drawn = DrawUpscaler(native_device, native_device_context, device_data, write_reactive);
#if DEVELOPMENT
         gd.mv.stats.sr_draws += drawn;
#endif
         gd.aa_drawn = device_data.has_drawn_sr;
      }
      // Luma's SMAA where MarkerPatch's doesn't draw (its FXAA, no MarkerPatch): on a copy of the scene (gamma 2.2 encoded, the values
      // above 1 kept by the fp16 mirror), back into it without its alpha
      else if (g_smaa_enable && !gd.sr_active && !gd.smaa_slot_a && gd.mv.scene_color && gd.mv.scene_rtv && gd.sr_rgb_blend_state)
      {
         com_ptr<ID3D11Texture2D> scene;
         D3D11_TEXTURE2D_DESC scene_desc = {};
         if (SUCCEEDED(gd.mv.scene_color->QueryInterface(&scene)))
         {
            scene->GetDesc(&scene_desc);
         }
         D3D11_TEXTURE2D_DESC snapshot_desc = {};
         if (gd.tex_snapshot)
         {
            gd.tex_snapshot->GetDesc(&snapshot_desc);
         }
         if (scene && (snapshot_desc.Width != scene_desc.Width || snapshot_desc.Height != scene_desc.Height || snapshot_desc.Format != scene_desc.Format))
         {
            gd.tex_snapshot.reset();
            gd.srv_snapshot.reset();
            snapshot_desc = CD3D11_TEXTURE2D_DESC(scene_desc.Format, scene_desc.Width, scene_desc.Height, 1, 1, D3D11_BIND_SHADER_RESOURCE);
            if (SUCCEEDED(native_device->CreateTexture2D(&snapshot_desc, nullptr, &gd.tex_snapshot)))
            {
               // In the render target view's format (the scene may be typeless)
               D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {};
               gd.mv.scene_rtv->GetDesc(&rtv_desc);
               const D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(D3D11_SRV_DIMENSION_TEXTURE2D, rtv_desc.Format, 0, 1);
               native_device->CreateShaderResourceView(gd.tex_snapshot.get(), &srv_desc, &gd.srv_snapshot);
            }
         }
         auto* const copy_vs = FindShader(device_data.native_vertex_shaders, CompileTimeStringHash("Copy VS"));
         auto* const copy_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("Copy PS"));
         if (scene && gd.srv_snapshot && copy_vs && copy_ps)
         {
            native_device_context->CopyResource(gd.tex_snapshot.get(), scene.get());
            if (RunSMAA(native_device, native_device_context, device_data, gd.srv_snapshot.get(), /* slot_a */ false))
            {
               DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), gd.sr_rgb_blend_state.get(), nullptr, copy_vs, copy_ps,
                  gd.tex_smaa_out_srv.get(), gd.mv.scene_rtv.get(), scene_desc.Width, scene_desc.Height, true);
               gd.aa_drawn = true;
#if DEVELOPMENT
               gd.aa_stats.smaa_slot_b++;
#endif
            }
         }
      }
      compute_state.Restore(native_device_context);
      graphics_state.Restore(native_device_context);
   }

   // The HDR display map in place on the frame ("target"), from a copy of it; see "Luma_DS2_DisplayMap.hlsl"
   static void DrawDisplayMap(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, ID3D11RenderTargetView* target)
   {
      auto& gd = GetGameDeviceData(device_data);
      auto* const copy_vs = FindShader(device_data.native_vertex_shaders, CompileTimeStringHash("Copy VS"));
      auto* const display_map_ps = FindShader(device_data.native_pixel_shaders, CompileTimeStringHash("DS2 Display Map PS"));
      if (!copy_vs || !display_map_ps)
         return;
      com_ptr<ID3D11Resource> frame_resource;
      target->GetResource(&frame_resource);
      com_ptr<ID3D11Texture2D> frame;
      frame_resource->QueryInterface(&frame);
      if (!frame)
         return;
      D3D11_TEXTURE2D_DESC frame_desc = {};
      frame->GetDesc(&frame_desc);
      D3D11_TEXTURE2D_DESC source_desc = {};
      if (gd.display_map_source)
      {
         gd.display_map_source->GetDesc(&source_desc);
      }
      if (source_desc.Width != frame_desc.Width || source_desc.Height != frame_desc.Height || source_desc.Format != frame_desc.Format)
      {
         gd.display_map_source.reset();
         gd.display_map_source_srv.reset();
         const D3D11_TEXTURE2D_DESC desc = CD3D11_TEXTURE2D_DESC(frame_desc.Format, frame_desc.Width, frame_desc.Height, 1, 1, D3D11_BIND_SHADER_RESOURCE);
         if (FAILED(native_device->CreateTexture2D(&desc, nullptr, &gd.display_map_source)))
            return;
         // In the render target view's format (the frame may be typeless)
         D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {};
         target->GetDesc(&rtv_desc);
         const D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(D3D11_SRV_DIMENSION_TEXTURE2D, rtv_desc.Format, 0, 1);
         if (FAILED(native_device->CreateShaderResourceView(gd.display_map_source.get(), &srv_desc, &gd.display_map_source_srv)))
         {
            gd.display_map_source.reset();
            return;
         }
      }
      DrawStateStack<DrawStateStackType::FullGraphics> graphics_state;
      graphics_state.Cache(native_device_context, device_data.uav_max_count);
      native_device_context->CopyResource(gd.display_map_source.get(), frame.get());
      SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaSettings);
      SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, reshade::api::shader_stage::pixel, LumaConstantBufferType::LumaData, 0, gd.uber_drawn);
      DrawCustomPixelShader(native_device_context, device_data.default_depth_stencil_state.get(), device_data.default_blend_state.get(), nullptr, copy_vs, display_map_ps,
         gd.display_map_source_srv.get(), target, frame_desc.Width, frame_desc.Height);
      graphics_state.Restore(native_device_context);
      gd.display_mapped = true;
   }

public:
   // Core's motion vector events: the motion vector target in every blend state, the tracked bindings and the vc4 copies
   static void SetEvents(bool enable)
   {
      SceneMotionVectors::SetEvents<GetSceneMotionVectors>(enable);
   }

   std::unique_ptr<std::byte[]> PatchShaderBytecodeSync(const std::byte* code, size_t& size, reshade::api::pipeline_subobject_type type, uint64_t shader_hash, const std::byte* shader_object, size_t shader_object_size) override
   {
      if (type != reshade::api::pipeline_subobject_type::pixel_shader)
         return nullptr;
      if (std::ranges::binary_search(apt_pixel_shaders, uint32_t(shader_hash)) || std::ranges::binary_search(hologram_pixel_shaders, uint32_t(shader_hash)))
         return DXBC::ClampOutput0(code, &size, DXBC::Output0Clamp::SATURATE);
      if (std::ranges::binary_search(blend_input_pixel_shaders, uint32_t(shader_hash)))
         return DXBC::ClampOutput0(code, &size, DXBC::Output0Clamp::BLEND_INPUT);
      return WrapUberLut(code, &size);
   }

   void OnInit(bool async) override
   {
      // The scene stays gamma encoded (1 = paper white) up to the swapchain copy. The HDR display map runs where the scene ends, before
      // the UI (see "display_mapped"), pre-scaled by Game/UI paper white (UI_DRAW_TYPE 2); the composition encodes scRGB.
      // The vanilla gamma ramp (the brightness slider) reaches the display map as "GammaRampExponent" (see "GammaRamp").
      GetShaderDefineData(POST_PROCESS_SPACE_TYPE_HASH).SetDefaultValue('0');
      GetShaderDefineData(EARLY_DISPLAY_ENCODING_HASH).SetDefaultValue('0');
      GetShaderDefineData(VANILLA_ENCODING_TYPE_HASH).SetDefaultValue('1'); // Gamma 2.2 in and out, as the other D3D9 games under the same wrapper
      GetShaderDefineData(GAMMA_CORRECTION_TYPE_HASH).SetDefaultValue('1');
      GetShaderDefineData(GAMUT_MAPPING_TYPE_HASH).SetDefaultValue('1');
      GetShaderDefineData(UI_DRAW_TYPE_HASH).SetDefaultValue('2');

      // Scene and UI paper white sliders (the UI one needs UI_DRAW_TYPE >= 1 and no OS reference white)
      use_os_reference_white_level = false;

      default_luma_global_game_settings.Contrast = 1.f;
      default_luma_global_game_settings.Saturation = 1.f;
      default_luma_global_game_settings.HighlightsDesaturation = 0.f;
      default_luma_global_game_settings.Dithering = 1.f;
      // The videos bypass the scene: without AutoHDR they sit flat at paper white. BL2's and Mirror's Edge's pair.
      default_luma_global_game_settings.VideoAutoHDREnable = 1.f;
      default_luma_global_game_settings.VideoAutoHDRBoost = 0.5f;
      default_luma_global_game_settings.GammaRampExponent = 1.f;
      cb_luma_global_settings.GameSettings = default_luma_global_game_settings;

      // Every dgVoodoo 2.87.x translation of the game's and MarkerPatch's shaders binds b0-b4 only (offline census: 17973 PS, 186 VS)
      luma_settings_cbuffer_index = 13;
      luma_data_cbuffer_index = 12;
      luma_ui_cbuffer_index = -1;

      // DLSS/FSR/XeSS/Luma TAA: their depth and the camera motion from the scene depth, the CPU copies of vc4 (dgVoodoo maps it or
      // updates it), and the motion vector target written by every blend state
      sr_game_tooltip = "DLSS, FSR and XeSS require Luma-Upscaler.exe next to the game's exe (Luma TAA doesn't).\nNone of them runs with MarkerPatch's SSAA (ImprovedAntiAliasingMode 3 or 4).\n";
      native_shaders_definitions.emplace(CompileTimeStringHash("DS2 Motion Vector Fill CS"),
         ShaderDefinition{.file_name = "Luma_DS2_MotionVectorFill", .type = reshade::api::pipeline_subobject_type::compute_shader});
      native_shaders_definitions.emplace(CompileTimeStringHash("DS2 SR Decode PS"),
         ShaderDefinition{.file_name = "Luma_DS2_SRGamma", .type = reshade::api::pipeline_subobject_type::pixel_shader, .function_name = "decode_ps"});
      native_shaders_definitions.emplace(CompileTimeStringHash("DS2 SR Encode PS"),
         ShaderDefinition{.file_name = "Luma_DS2_SRGamma", .type = reshade::api::pipeline_subobject_type::pixel_shader, .function_name = "encode_ps"});
      native_shaders_definitions.emplace(CompileTimeStringHash("DS2 Display Map PS"),
         ShaderDefinition{.file_name = "Luma_DS2_DisplayMap", .type = reshade::api::pipeline_subobject_type::pixel_shader});
      // Core registers the 6 SMAA passes and RCAS; these are this game's own. SMAA predication: the scene depth -> R16F plane-deviation
      // edge-ness
      native_shaders_definitions.emplace(CompileTimeStringHash("DS2 Depth Edges CS"),
         ShaderDefinition{.file_name = "Luma_DS2_DepthEdges", .type = reshade::api::pipeline_subobject_type::compute_shader});
      // SMAA T2x's own passes ("SMAA_T2X" in "Luma_SMAA_impl.hlsl"). The edge detection and the vertex shaders are 1x's.
      native_shaders_definitions.emplace(CompileTimeStringHash("DS2 SMAA T2x Blending Weight Calculation PS"),
         ShaderDefinition{.file_name = "Luma_SMAA_impl", .type = reshade::api::pipeline_subobject_type::pixel_shader, .function_name = "smaa_blending_weight_calculation_ps", .defines_data = {{"SMAA_T2X", "1"}}});
      native_shaders_definitions.emplace(CompileTimeStringHash("DS2 SMAA T2x Neighborhood Blending PS"),
         ShaderDefinition{.file_name = "Luma_SMAA_impl", .type = reshade::api::pipeline_subobject_type::pixel_shader, .function_name = "smaa_neighborhood_blending_ps", .defines_data = {{"SMAA_T2X", "1"}}});
      native_shaders_definitions.emplace(CompileTimeStringHash("DS2 SMAA T2x Resolve PS"),
         ShaderDefinition{.file_name = "Luma_SMAA_impl", .type = reshade::api::pipeline_subobject_type::pixel_shader, .function_name = "smaa_resolve_ps", .defines_data = {{"SMAA_T2X", "1"}}});
      SetEvents(true);

#if DEVELOPMENT
      // For the MCP "luma_dev_values" tool
      Mcp::RegisterToggles({{"smaa_enable", &g_smaa_enable}, {"smaa_t2x", &g_smaa_t2x}, {"smaa_predication", &g_smaa_predication}, {"mv_enable", &g_mv_enable}, {"mv_debug_view", &g_mv_debug_view},
         {"mv_force_jitter", &g_mv_force_jitter}, {"sr_reactive_enable", &g_sr_reactive_enable}, { "sr_reactive_debug_view",
            &g_sr_reactive_debug_view }});
      Mcp::RegisterValues({{"rcas_sharpness", &g_rcas_sharpness, 0.f, 1.f}, {"smaa_pred_tolerance", &g_smaa_pred_tolerance, 0.002f, 0.2f}, {"sr_reactive_scale", &g_sr_reactive_scale, 0.f, 4.f},
         {"sr_reactive_threshold", &g_sr_reactive_threshold, 0.f, 1.f}});
      Mcp::RegisterTextures({MCP_GAME_TEXTURE("smaa.output", tex_smaa_out), MCP_GAME_TEXTURE("smaa.pred_mask", tex_pred), MCP_GAME_TEXTURE("mv.velocity", mv.texture),
         MCP_GAME_TEXTURE("mv.depth", mv.device_depth), MCP_GAME_TEXTURE("sr.reactive", mv.reactive), MCP_GAME_TEXTURE("sr.transparency", mv.transparency),
         MCP_GAME_TEXTURE("sr.draws_mask", mv.reactive_target), MCP_GAME_TEXTURE("sr.input", sr_input), MCP_GAME_TEXTURE("sr.output", sr_output_srv),
         MCP_GAME_TEXTURE("smaa.input", frame_resource), MCP_GAME_TEXTURE("smaa.snapshot", tex_snapshot), MCP_GAME_TEXTURE("smaa.rcas_input", tex_rcas_in),
         MCP_GAME_TEXTURE("smaa.t2x_frame0", t2x_frames[0]), MCP_GAME_TEXTURE("smaa.t2x_frame1", t2x_frames[1]), MCP_GAME_TEXTURE("scene.depth", scene_depth)});
#endif
   }

   void OnCreateDevice(ID3D11Device* native_device, DeviceData& device_data) override
   {
      device_data.game = new DeadSpace2GameDeviceData;
      // The upscaler's and the scene's end SMAA's output go back into the scene without its alpha (unknown use, not opaque)
      D3D11_BLEND_DESC blend_desc = CD3D11_BLEND_DESC(D3D11_DEFAULT);
      blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;
      native_device->CreateBlendState(&blend_desc, &GetGameDeviceData(device_data).sr_rgb_blend_state);
#if DEVELOPMENT
      // For the MCP "luma_dev_values" tool: the last complete frame's counts
      SceneMotionVectors::RegisterDevValues(GetGameDeviceData(device_data).mv, &device_data);
      const auto& stats = GetGameDeviceData(device_data).last_aa_stats;
      Mcp::RegisterCounters({{"aa.smaa_slot_a", &stats.smaa_slot_a}, {"aa.smaa_slot_b", &stats.smaa_slot_b}, {"aa.t2x_resolves", &stats.t2x_resolves},
                               {"aa.fxaa_copies", &stats.fxaa_copies}, {"aa.markerpatch_smaa", &stats.markerpatch_smaa}, {"aa.ssaa", &stats.ssaa},
                               {"aa.smaa_slot_a_chosen", &stats.smaa_slot_a_chosen}, { "aa.t2x_active",
                                  &stats.t2x_active }},
         &device_data);
#endif
   }

   void OnDestroyDeviceData(DeviceData& device_data) override
   {
#if DEVELOPMENT
      Mcp::Unregister(&device_data);
#endif
      GammaRamp::Uninstall();
      Game::OnDestroyDeviceData(device_data);
   }

   // The game's copy of the scene (MarkerPatch's SMAA input, the copy to the UI canvas) ends a scene no post pass ended
   bool OverrideCopyResource(ID3D11Device* native_device, DeviceData& device_data, uint64_t& dst_resource, uint64_t& src_resource) override
   {
      auto& gd = GetGameDeviceData(device_data);
      if (gd.mv.active && gd.mv.scene_open && gd.mv.scene_color && src_resource == reinterpret_cast<uintptr_t>(gd.mv.scene_color.get()))
      {
#if DEVELOPMENT
         gd.mv.stats.ended_by = 0;
#endif
         com_ptr<ID3D11DeviceContext> native_device_context;
         native_device->GetImmediateContext(&native_device_context);
         EndScene(native_device, native_device_context.get(), device_data);
      }
      return false;
   }

   void OnPresent(ID3D11Device* native_device, DeviceData& device_data) override
   {
      auto& gd = GetGameDeviceData(device_data);
      gd.open_refused_dsv = nullptr;
      gd.scene_target.reset();

      // The game's device pointer (Steam build only, as the camera); installed once the device exists, again after a new one
      if (GetEngineViewProjection())
      {
         if (IDirect3DDevice9* const d3d9_device = *reinterpret_cast<IDirect3DDevice9* const*>(0x211cdcc))
         {
            GammaRamp::Install(d3d9_device);
         }
      }
      if (const float exponent = GammaRamp::exponent; cb_luma_global_settings.GameSettings.GammaRampExponent != exponent)
      {
         cb_luma_global_settings.GameSettings.GammaRampExponent = exponent;
         device_data.cb_luma_global_settings_dirty = true;
      }

      // DLSS/FSR/XeSS/Luma TAA: the history restarts after any frame it didn't draw (menus, loading, just picked); the selection and the
      // motion vector state are fixed here for the next frame (see "sr_active")
      gd.mv_jitter_allowed = device_data.has_drawn_sr;
      gd.sr_active = LatchSRFrame(device_data);
      // Luma's SMAA gives way to an upscaler. It takes MarkerPatch's SMAA slot if that drew last frame, else the scene's end (with the
      // motion vector path's scene tracking). T2x runs on the upscalers' motion vectors, not with SSAA ("OpenScene" refuses its scene);
      // turned off, its frames go, and the motion vectors go as when no upscaler is picked.
      const bool markerpatch_smaa = std::exchange(gd.markerpatch_smaa_drawn, false);
      gd.ssaa_active = std::exchange(gd.ssaa_drawn, false);
      // Without its shaders (still compiling, failed) Luma's SMAA can't replace MarkerPatch's, whose edges and weights then still draw
      const bool smaa_available = g_smaa_enable && HasSMAAShaders(device_data);
      gd.smaa_slot_a = smaa_available && !gd.sr_active && markerpatch_smaa;
#if DEVELOPMENT
      gd.aa_stats.markerpatch_smaa = markerpatch_smaa;
      gd.aa_stats.ssaa = gd.ssaa_active;
      gd.aa_stats.smaa_slot_a_chosen = gd.smaa_slot_a;
      gd.aa_stats.t2x_active = gd.t2x_active;
      gd.last_aa_stats = std::exchange(gd.aa_stats, {});
#endif
      gd.aa_drawn = false;
      gd.display_mapped = false;
      gd.uber_drawn = false;
      gd.post_target.reset();
      gd.t2x_phase = -1;
      gd.mv_filled = false;
      const bool t2x_was_active = gd.t2x_active;
      gd.t2x_active = g_smaa_enable && g_smaa_t2x && !gd.sr_active && !gd.ssaa_active;
      if (t2x_was_active && !gd.t2x_active)
      {
         gd.ReleaseT2xFrames();
         device_data.release_sr_resources = true;
      }
      if (gd.t2x_frames[0] && cb_luma_global_settings.FrameIndex - gd.t2x_frame > IDLE_RELEASE_FRAMES)
      {
         gd.ReleaseT2xFrames();
      }
      // SMAA's resources go once it stopped running (MEM-11)
      if (gd.tex_smaa_out && cb_luma_global_settings.FrameIndex - gd.smaa_frame > IDLE_RELEASE_FRAMES)
      {
         gd.ReleaseSMAAScratch();
         ReleaseSMAAIntermediates(device_data);
      }
      // ponytail: the scene's end SMAA 1x draws the motion vectors it doesn't use; split the scene tracking from the motion vector draws
      // if its cost shows
      const bool smaa_scene_end = smaa_available && !gd.sr_active && !markerpatch_smaa;
      // The uploads the buffer hooks missed while motion vectors were off make every earlier copy stale
      const bool mv_active = gd.sr_active || gd.t2x_active || g_mv_enable || smaa_scene_end;
      if (!gd.mv.active.exchange(mv_active) && mv_active)
      {
         gd.mv.constants.Restart();
      }
      // None picked: Core stopped the SR bridge's helper ("ReleaseResources"), our upscaler inputs and output go too. Recreated when an
      // upscaler is picked again (the helper takes seconds to start).
      if (device_data.sr_type == SR::Type::None && device_data.release_sr_resources.exchange(false))
      {
         gd.sr_output_srv.reset();
         gd.sr_input.reset();
         gd.sr_input_srv.reset();
         gd.sr_input_rtv.reset();
         gd.sr_scene.reset();
         gd.sr_scene_srv.reset();
         if (!g_mv_enable && !gd.t2x_active && !smaa_scene_end)
         {
            const std::unique_lock lock(gd.mv.mutex);
            gd.mv.ReleaseMotionVectorTargets();
            // The game's scene and depth (taken again at the next scene), so a resize after None doesn't keep the old ones alive; and
            // the vc4 copies the object tables and the cameras hold
            gd.mv.depth.reset();
            gd.mv.scene_color.reset();
            gd.mv.scene_rtv.reset();
            gd.scene_depth.reset();
            gd.scene_depth_srv.reset();
            gd.mv.objects.Reset();
            gd.mv.camera.reset();
            gd.mv.previous_camera.reset();
            gd.mv.fill_buffer.reset();
            gd.mv.jitter_buffer.reset();
            gd.mv.previous_constants = {}; // Its 4 MiB dynamic ring is address space too
            gd.mv.constants.ReleasePool();
         }
      }
      gd.mv.constants.TrimPool(gd.mv.active);
#if DEVELOPMENT
      gd.mv.stats.constants_pool = uint32_t(gd.mv.constants.pool.size());
#endif
      // The reactive masks go once the fill stopped writing them (DLSS, the masks off)
      if (gd.mv.reactive_target && cb_luma_global_settings.FrameIndex - gd.mv.sr_reactive_frame > IDLE_RELEASE_FRAMES)
      {
         const std::unique_lock lock(gd.mv.mutex);
         gd.mv.ReleaseReactiveMasks();
      }
      // A scene nothing ended ends here: its jitter must not reach the next frame's draws before its first mesh
      gd.mv.scene_open = false;
      gd.mv.scene_done = false;
      gd.mv.frame_ended = true;
      gd.mv.fill_pending = false;
      // The upscalers' mip bias, once the upscaler draws (Core biases the upgraded samplers)
      SetTextureMipLodBias(nullptr, device_data, ((gd.sr_active && gd.mv_jitter_allowed) ? SR::GetMipLODBias(device_data.sr_type, device_data.output_resolution.y, device_data.output_resolution.y) : 0.f));
#if DEVELOPMENT
      gd.mv.last_stats = std::exchange(gd.mv.stats, {});
      // "MV Debug View": Core's debug draw of the target, absolute values in pixels
      {
         const std::shared_lock lock(gd.mv.mutex);
         if (g_mv_debug_view && gd.mv.texture)
         {
            D3D11_TEXTURE2D_DESC desc = {};
            gd.mv.texture->GetDesc(&desc);
            debug_draw_auto_clear_texture = false;
            debug_draw_options |= (uint32_t)DebugDrawTextureOptionsMask::Abs | (uint32_t)DebugDrawTextureOptionsMask::UVToPixelSpace;
            device_data.debug_draw_texture = gd.mv.texture.get();
            device_data.debug_draw_texture_format = desc.Format;
            device_data.debug_draw_texture_size = {desc.Width, desc.Height, 1, 1};
         }
         else if (g_sr_reactive_debug_view && gd.mv.reactive)
         {
            debug_draw_auto_clear_texture = false;
            debug_draw_options |= (uint32_t)DebugDrawTextureOptionsMask::RedOnly;
            debug_draw_options &= ~((uint32_t)DebugDrawTextureOptionsMask::Abs | (uint32_t)DebugDrawTextureOptionsMask::UVToPixelSpace);
            device_data.debug_draw_texture = gd.mv.reactive.get();
            device_data.debug_draw_texture_format = DXGI_FORMAT_R8_UNORM;
            device_data.debug_draw_texture_size = {uint32_t(device_data.output_resolution.x), uint32_t(device_data.output_resolution.y), 1, 1};
         }
         else if (device_data.debug_draw_texture && (device_data.debug_draw_texture.get() == gd.mv.texture.get() || device_data.debug_draw_texture.get() == gd.mv.reactive.get()))
         {
            device_data.debug_draw_texture = nullptr;
         }
      }
#endif
   }

   void LoadConfigs() override
   {
      auto& gs = cb_luma_global_settings.GameSettings;
      reshade::get_config_value(nullptr, NAME, "Contrast", gs.Contrast);
      reshade::get_config_value(nullptr, NAME, "Saturation", gs.Saturation);
      reshade::get_config_value(nullptr, NAME, "HighlightsDesaturation", gs.HighlightsDesaturation);
      reshade::get_config_value(nullptr, NAME, "Dithering", gs.Dithering);
      reshade::get_config_value(nullptr, NAME, "VideoAutoHDREnable", gs.VideoAutoHDREnable);
      reshade::get_config_value(nullptr, NAME, "VideoAutoHDRBoost", gs.VideoAutoHDRBoost);
      reshade::get_config_value(nullptr, NAME, "SMAAEnable", g_smaa_enable);
      reshade::get_config_value(nullptr, NAME, "SMAAT2x", g_smaa_t2x);
      reshade::get_config_value(nullptr, NAME, "RCASSharpness", g_rcas_sharpness);
#if DEVELOPMENT
      reshade::get_config_value(nullptr, NAME, "SMAAPredication", g_smaa_predication);
      reshade::get_config_value(nullptr, NAME, "SMAAPredicationTolerance", g_smaa_pred_tolerance);
#endif
   }

   void DrawImGuiSettings(DeviceData& device_data) override
   {
      auto& gs = cb_luma_global_settings.GameSettings;
      const auto& gs_def = default_luma_global_game_settings;
      ImGui::SeparatorText("Anti-Aliasing");
      const auto& gd = GetGameDeviceData(device_data);
      // The upscaler (Super Resolution, in the Settings tab) replaces SMAA
      DrawSettingCheckbox("SMAA Enable", "SMAAEnable", &g_smaa_enable, true, "Replaces the game's anti-aliasing with SMAA (sharper, more complete edges; not used with DLSS/FSR/XeSS or Luma TAA).", gd.sr_active);
      // T2x needs the motion vectors, which MarkerPatch's SSAA scene doesn't get
      const bool t2x_unavailable = gd.sr_active || gd.ssaa_active;
      ImGui::BeginDisabled(!g_smaa_enable || t2x_unavailable);
      DrawSettingCheckbox("SMAA T2x", "SMAAT2x", &g_smaa_t2x, false,
         "Smoother edges and less shimmering, by blending each frame with the previous one.\nCan look slightly softer in motion. Not used with DLSS/FSR/XeSS, Luma TAA or MarkerPatch's SSAA.", t2x_unavailable);
      ImGui::EndDisabled();
      ImGui::BeginDisabled(!g_smaa_enable && !gd.sr_active);
      DrawSettingSlider("RCAS Sharpness", "RCASSharpness", &g_rcas_sharpness, 0.f, 0.f, 1.f, "Sharpening applied on top of anti-aliasing (0 = off).");
      ImGui::EndDisabled();
#if DEVELOPMENT
      DrawSettingCheckbox("SMAA Predication", "SMAAPredication", &g_smaa_predication, true, nullptr);
      DrawSettingSlider("SMAA Predication Tolerance", "SMAAPredicationTolerance", &g_smaa_pred_tolerance, 0.02f, 0.002f, 0.2f, nullptr, "%.3f", ImGuiSliderFlags_Logarithmic);
#endif

      ImGui::SeparatorText("Grade");
      DrawSettingSlider("Contrast", "Contrast", &gs.Contrast, gs_def.Contrast, 0.f, 2.f, "Overall image contrast, HDR only (1 = vanilla).");
      DrawSettingSlider("Saturation", "Saturation", &gs.Saturation, gs_def.Saturation, 0.f, 2.f, "Color saturation, HDR only (1 = vanilla).");
      DrawSettingSlider("Highlights Desaturation", "HighlightsDesaturation", &gs.HighlightsDesaturation, gs_def.HighlightsDesaturation, 0.f, 1.f,
         "How far the brightest sources fade to neutral white, HDR only (0 = keep color at any brightness).");

      ImGui::SeparatorText("Effects");
      DrawSettingCheckbox("Video AutoHDR", "VideoAutoHDREnable", &gs.VideoAutoHDREnable, gs_def.VideoAutoHDREnable, "Adds HDR highlights to pre-rendered videos (HDR only).");
      if (gs.VideoAutoHDREnable > 0.5f)
      {
         DrawSettingSlider("Video HDR Boost", "VideoAutoHDRBoost", &gs.VideoAutoHDRBoost, gs_def.VideoAutoHDRBoost, 0.f, 1.f, "Video highlight strength (0 = off).");
      }
      DrawSettingCheckbox("Dithering", "Dithering", &gs.Dithering, gs_def.Dithering, "Reduces gradient banding.");

#if DEVELOPMENT
      ImGui::SeparatorText("Motion Vectors (DLSS/FSR/Luma TAA)");
      ImGui::Checkbox("MV Enable", &g_mv_enable);
      if (ImGui::IsItemHovered())
      {
         ImGui::SetTooltip("Draws the scene with the motion vector shaders without an upscaler. The image must not change; the debug view is\nblack with a static camera and lights up only what moves. Not saved.");
      }
      ImGui::Checkbox("MV Debug View", &g_mv_debug_view);
      ImGui::Checkbox("MV Force Jitter", &g_mv_force_jitter);
      ImGui::Checkbox("Reactive Mask", &g_sr_reactive_enable);
      ImGui::Checkbox("Reactive Mask Debug View", &g_sr_reactive_debug_view);
#endif
   }

   DrawOrDispatchOverrideType OnDrawOrDispatch(ID3D11Device* native_device, ID3D11DeviceContext* native_device_context, CommandListData& cmd_list_data, DeviceData& device_data, reshade::api::shader_stage stages, const ShaderHashesList<OneShaderPerPipeline>& original_shader_hashes, bool is_custom_pass, bool& updated_cbuffers, std::function<void()>* original_draw_dispatch_func) override
   {
      const bool is_immediate = native_device_context->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE;
      auto& gd = GetGameDeviceData(device_data);
      const uint32_t pixel_shader_hash = uint32_t(original_shader_hashes.pixel_shaders[0]);
      // A game draw that can be drawn again with patched shaders (not Luma's, nor one whose shader Luma replaced: "is_custom_pass")
      const bool game_draw = !is_custom_pass && original_draw_dispatch_func && *original_draw_dispatch_func;
      // The scene rendered this frame (DLSS-Best-Practices BK-1): Core's SR icon then warns when the upscaler didn't draw (SSAA, another
      // exe build). Not on the video.
      const bool scene_end = scene_end_pixel_shaders.contains(pixel_shader_hash);
      if (is_immediate && pixel_shader_hash != VIDEO_PS && scene_end)
      {
         device_data.has_drawn_main_post_processing = true;
      }
      const bool uber = std::ranges::binary_search(uber_pixel_shaders, pixel_shader_hash);
      if (is_immediate && uber)
      {
         gd.uber_drawn = true;
      }
      // Areas without the uber post end with depth of field or motion blur; their half and quarter resolution passes aren't the frame.
      // MarkerPatch's SSAA downsample (a replaced pass) writes the output sized frame the UI then draws on.
      const bool post = uber || pixel_shader_hash == SSAA_DOWNSAMPLE_PS || std::ranges::binary_search(post_pixel_shaders, pixel_shader_hash);
      if (is_immediate && post)
      {
         com_ptr<ID3D11RenderTargetView> rtv;
         native_device_context->OMGetRenderTargets(1, &rtv, nullptr);
         if (rtv && GetViewTextureSize(rtv.get()).x >= uint32_t(device_data.output_resolution.x))
         {
            gd.post_target.reset();
            rtv->GetResource(&gd.post_target);
         }
      }
      // What draws over the scene (menus, RIG and pickup holograms, video logs) starts with the first game draw on the frame after the
      // last post pass, or with the first APT draw on a screen sized target when no post pass drew (the pause menu stops them). Unless
      // MarkerPatch's antialiasing slot (its SMAA edges and weights; the replaced passes are custom passes) mapped the frame there, the
      // display map runs first, so those keep the UI paper white and the present blit doesn't map them with the scene.
      const bool scene_over = (gd.post_target ? (!post && !markerpatch_smaa_pixel_shaders.contains(pixel_shader_hash))
                                              : std::ranges::binary_search(apt_pixel_shaders, pixel_shader_hash));
      if (is_immediate && scene_over && !gd.display_mapped && !is_custom_pass)
      {
         com_ptr<ID3D11RenderTargetView> rtv;
         native_device_context->OMGetRenderTargets(1, &rtv, nullptr);
         com_ptr<ID3D11Resource> target;
         if (rtv)
         {
            rtv->GetResource(&target);
         }
         bool frame = target && target == gd.post_target;
         if (target && !gd.post_target)
         {
            const auto size = GetViewTextureSize(rtv.get());
            frame = size.x == uint32_t(device_data.output_resolution.x) && size.y == uint32_t(device_data.output_resolution.y);
         }
         if (frame)
         {
            DrawDisplayMap(native_device, native_device_context, cmd_list_data, device_data, rtv.get());
         }
      }

      // SMAA's predication without a scene opened (no motion vectors, e.g. in MarkerPatch's slot, or SSAA): the scene depth at the
      // material apply's first draw ("scene_target" then marks it done for the frame)
      if (is_immediate && g_smaa_enable && !gd.sr_active && !gd.mv.scene_open && !gd.scene_target && game_draw && std::ranges::binary_search(apply_pixel_shaders, pixel_shader_hash))
      {
         com_ptr<ID3D11RenderTargetView> rtv;
         com_ptr<ID3D11DepthStencilView> dsv;
         native_device_context->OMGetRenderTargets(1, &rtv, &dsv);
         if (rtv && dsv)
         {
            rtv->GetResource(&gd.scene_target);
            if (GetViewTextureSize(dsv.get()).x > uint32_t(device_data.output_resolution.x))
            {
               gd.ssaa_drawn = true;
            }
            com_ptr<ID3D11Resource> depth;
            dsv->GetResource(&depth);
            TrackSceneDepth(native_device, &gd, depth.get());
         }
      }

      // DLAA/FSR/XeSS/Luma TAA: the scene's draws with motion vectors or jitter (see "DrawWithMotionVectors"), and the fill and upscaler
      // at the first pass after it
      if (gd.mv.active && is_immediate)
      {
         // With a vertex shader, which the patched shaders can redo
         const bool game_mesh_draw = game_draw && (stages & reshade::api::shader_stage::vertex) == reshade::api::shader_stage::vertex;
         if (!gd.mv.scene_done && scene_end)
         {
            if (gd.mv.scene_open)
            {
#if DEVELOPMENT
               gd.mv.stats.ended_by = pixel_shader_hash;
#endif
               EndScene(native_device, native_device_context, device_data);
            }
         }
         // Until the scene opens only the depth view matters (the shadow maps and the environment cube come first)
         else if (!gd.mv.scene_done && !gd.mv.scene_open && game_mesh_draw)
         {
            com_ptr<ID3D11DepthStencilView> scene_dsv;
            native_device_context->OMGetRenderTargets(0, nullptr, &scene_dsv);
            if (scene_dsv && scene_dsv.get() != gd.open_refused_dsv)
            {
               OpenScene(native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes.vertex_shaders[0], scene_dsv.get());
            }
         }
         if (gd.mv.scene_open && game_mesh_draw)
         {
            com_ptr<ID3D11RenderTargetView> rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
            com_ptr<ID3D11DepthStencilView> dsv;
            native_device_context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, &rtvs[0], &dsv);
            com_ptr<ID3D11Resource> target;
            if (rtvs[0])
            {
               rtvs[0]->GetResource(&target);
            }
            if (!gd.scene_target && target && std::ranges::binary_search(apply_pixel_shaders, pixel_shader_hash))
            {
               gd.scene_target = target;
            }
            const bool motion_vectors = target && target == gd.scene_target &&
                                        SceneMotionVectors::DrawWithMotionVectors<MotionVectorGame>(&gd.mv, native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes, *original_draw_dispatch_func, rtvs, dsv.get());
            // An alpha blending state blends the motion vector target by the alpha written there: a draw that doesn't write it (refused
            // above) must not have it bound
            if (!motion_vectors && rtvs[SceneMotionVectors::target_slot] && rtvs[SceneMotionVectors::target_slot] == gd.mv.rtv)
            {
               SceneMotionVectors::ClassifyBoundBlend(native_device_context, &gd.mv);
               if (gd.mv.alpha_blended)
               {
                  rtvs[SceneMotionVectors::target_slot].reset();
                  ID3D11RenderTargetView* targets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
                  for (UINT slot = 0; slot < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; slot++)
                  {
                     targets[slot] = rtvs[slot].get();
                  }
                  native_device_context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, targets, dsv.get());
               }
            }
            // Only the meshes placed by a camera jitter: the screen passes that test the scene depth (vPosScaleTranslationVS) keep their
            // pixels
            bool jitter = false;
            if (!motion_vectors && SceneMotionVectors::GetPatchedVertexShader<MotionVectorGame>(&gd.mv, native_device, cmd_list_data, device_data, original_shader_hashes.vertex_shaders[0]).camera_offset != 0)
            {
               jitter = SceneMotionVectors::DrawWithJitter<MotionVectorGame>(&gd.mv, native_device, native_device_context, cmd_list_data, device_data, original_shader_hashes, *original_draw_dispatch_func, rtvs, dsv.get());
            }
#if DEVELOPMENT
            Mcp::Annotate(cmd_list_data, motion_vectors ? "mv" : (jitter ? "jitter" : "unpatched"));
            if (const SceneMotionVectors::MotionVectorReject reject = std::exchange(gd.mv.draw_reject, SceneMotionVectors::REJECT_COUNT); reject != SceneMotionVectors::REJECT_COUNT)
            {
               Mcp::Annotate(cmd_list_data, "mv_reject", int(reject));
            }
#endif
            if (motion_vectors || jitter)
               return DrawOrDispatchOverrideType::Replaced;
         }
      }
      // Every draw without a patched shader: the game's own, if the last patched draw's are still bound
      if (is_immediate)
      {
         PatchedDraws::RestoreGameShader(native_device_context, &gd.mv.bound_vertex_shader);
         PatchedDraws::RestoreGameShader(native_device_context, &gd.mv.bound_pixel_shader);
      }
      // The HDR display map, where the scene ends: in MarkerPatch's SMAA neighborhood or FXAA replacement (their antialiasing only runs
      // when neither an upscaler nor Luma's SMAA did); else "DrawDisplayMap" before the UI, and the present blit for frames nothing mapped
      if (pixel_shader_hash == MARKERPATCH_FXAA_PS && is_immediate)
      {
         // Both: "updated_cbuffers" skips Core's own bind of the settings, which the display map reads. LumaData.CustomData3: "DrawDisplayMap"
         // already mapped the frame
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaSettings);
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaData, gd.aa_drawn, gd.uber_drawn, gd.display_mapped ? 1.f : 0.f);
         updated_cbuffers = true;
         gd.display_mapped = true;
#if DEVELOPMENT
         gd.aa_stats.fxaa_copies += gd.aa_drawn;
#endif
         return DrawOrDispatchOverrideType::None;
      }
      if (pixel_shader_hash == SMAA_NEIGHBORHOOD_PS || pixel_shader_hash == PRESENT_BLIT_PS)
      {
         const bool neighborhood = pixel_shader_hash == SMAA_NEIGHBORHOOD_PS;
         // Luma's SMAA in MarkerPatch's slot: from its copy of the frame (t0) into our output, which the replacement then reads instead
         com_ptr<ID3D11ShaderResourceView> frame_srv;
         bool smaa_drawn = false;
         if (neighborhood && is_immediate)
         {
            gd.markerpatch_smaa_drawn = true;
            native_device_context->PSGetShaderResources(0, 1, &frame_srv);
            // Not "game_draw": this pass is replaced, so it counts as a custom pass
            ID3D11ShaderResourceView* const source_srv = ((gd.smaa_slot_a && original_draw_dispatch_func && *original_draw_dispatch_func && frame_srv) ? GetFrameView(native_device, &gd, frame_srv.get()) : nullptr);
            if (source_srv)
            {
               DrawStateStack<DrawStateStackType::FullGraphics> graphics_state;
               DrawStateStack<DrawStateStackType::Compute> compute_state;
               graphics_state.Cache(native_device_context, device_data.uav_max_count);
               compute_state.Cache(native_device_context, device_data.uav_max_count);
               smaa_drawn = RunSMAA(native_device, native_device_context, device_data, source_srv, /* slot_a */ true);
               compute_state.Restore(native_device_context);
               graphics_state.Restore(native_device_context);
            }
         }
         // The neighborhood replacement only display maps once MarkerPatch's blending weights were skipped (see below), whether or not our
         // SMAA drew
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaSettings);
         SetLumaConstantBuffers(native_device_context, cmd_list_data, device_data, stages, LumaConstantBufferType::LumaData, neighborhood ? (device_data.has_drawn_sr || gd.smaa_slot_a) : gd.display_mapped, gd.uber_drawn,
            gd.display_mapped ? 1.f : 0.f);
         updated_cbuffers = true;
         gd.display_mapped = neighborhood;
         if (!smaa_drawn)
            return DrawOrDispatchOverrideType::None;
         gd.aa_drawn = true;
#if DEVELOPMENT
         gd.aa_stats.smaa_slot_a++;
#endif
         ID3D11ShaderResourceView* const smaa_srv = gd.tex_smaa_out_srv.get();
         native_device_context->PSSetShaderResources(0, 1, &smaa_srv);
         (*original_draw_dispatch_func)();
         ID3D11ShaderResourceView* const game_srv = frame_srv.get();
         native_device_context->PSSetShaderResources(0, 1, &game_srv);
         return DrawOrDispatchOverrideType::Replaced;
      }
      // TODO: the vanilla EdgeAA blur (no MarkerPatch AA, game option on) draws only where a level effect requests it (MarkerPatch forces
      // the request every frame, "OnScreenEdgeAARender"); none seen live yet, so it isn't skipped after Luma's AA
      // The upscaler antialiased this frame or Luma's SMAA takes MarkerPatch's slot: MarkerPatch's SMAA edges and weights would only
      // soften it (they write nothing else the frame reads)
      if ((gd.smaa_slot_a || device_data.has_drawn_sr) && game_draw && markerpatch_smaa_pixel_shaders.contains(pixel_shader_hash))
         return DrawOrDispatchOverrideType::Replaced;
      return DrawOrDispatchOverrideType::None;
   }

   void PrintImGuiAbout() override
   {
      ImGui::PushTextWrapPos(0.f);
      ImGui::Text(
         "Luma for \"Dead Space 2\" is developed by DristoforColumb and is open source and free.\n"
         "It adds HDR, DLAA, FSR 3 or XeSS native anti-aliasing or Luma TAA, and SMAA (with T2x).\n"
         "It runs through dgVoodoo2 (DirectX 9 -> 11).\n"
         "Do NOT run another HDR mod (e.g. RenoDX) alongside it.\n"
         "Thanks to the Luma team and contributors.\n"
         "If you enjoy it, consider donating.");
      ImGui::PopTextWrapPos();

      ImGui::NewLine();
      ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(70, 134, 0, 255));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(70 + 9, 134 + 9, 0, 255));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(70 + 18, 134 + 18, 0, 255));
      static const std::string donation_link = std::string("Buy DristoforColumb a Coffee on ko-fi ") + std::string(ICON_FK_OK);
      if (ImGui::Button(donation_link.c_str()))
      {
         ShellExecuteA(nullptr, "open", "https://ko-fi.com/dristoforcolumb", nullptr, nullptr, SW_SHOWNORMAL);
      }
      ImGui::PopStyleColor(3);

      ImGui::NewLine();
      static const std::string social_link = std::string("Join our \"HDR Den\" Discord ") + std::string(ICON_FK_SEARCH);
      if (ImGui::Button(social_link.c_str()))
      {
         // Unique link for Luma's HDR Den (tracks the origin of people joining); do not share for other purposes.
         static const std::string discord_link = std::string("https://discord.gg/J9fM") + std::string("3EVuEZ");
         ShellExecuteA(nullptr, "open", discord_link.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
      }
      static const std::string contributing_link = std::string("Contribute on Github ") + std::string(ICON_FK_FILE_CODE);
      if (ImGui::Button(contributing_link.c_str()))
      {
         ShellExecuteA(nullptr, "open", "https://github.com/Filoppi/Luma-Framework", nullptr, nullptr, SW_SHOWNORMAL);
      }

      ImGui::NewLine();
      ImGui::Text("Build Date: %s %s", __DATE__, __TIME__);

      ImGui::NewLine();
      ImGui::Text("Credits:"
                  "\n\nMain:"
                  "\nDristoforColumb"
                  "\n\nThird Party:"
                  "\nReShade"
                  "\nImGui"
                  "\nRenoDX (HDR tonemap method)"
                  "\nMacLeod-Boynton hue emulation (RenoDX)"
                  "\nSMAA (Iryoku)"
                  "\nAMD FidelityFX (RCAS + FSR 3)"
                  "\nNVIDIA NGX (DLSS)"
                  "\nIntel XeSS"
                  "\ndgVoodoo2 by Dege (DirectX 9 -> 11 wrapper, required)");
   }
};

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
   if (ul_reason_for_call == DLL_PROCESS_ATTACH)
   {
      const char* project_name = PROJECT_NAME;
      const char* cleared_project_name = ((project_name[0] == '_') ? (project_name + 1) : project_name);

      uint32_t mod_version = 1;
      Globals::SetGlobals(cleared_project_name, "Dead Space 2 Luma mod", "", mod_version);

      // As in the other dgVoodoo mods: "force_borderless" also covers leaving fullscreen, so the window can't come back with a title bar
      prevent_fullscreen_state = true;
      force_borderless = true;

      // HDR: the Mass Effect 2007 / The Witcher scheme under the same wrapper. The game has no tonemap and every target is RGBA8, so
      // keeping what it renders above 1 (lamp cores, glows, per-channel clipping in the light buffer and scene) is the whole HDR signal.
      swapchain_format_upgrade_type = TextureFormatUpgradesType::AllowedEnabled;
      swapchain_upgrade_type = SwapchainUpgradeType::scRGB;

      // The game's brightness slider is a D3D9 SetGammaRamp, which dgVoodoo forwards to the OS device ramp and distorts HDR output
      allow_disabling_gamma_ramp = true;

      // Indirect (mirrors substituted at bind): changing dgVoodoo's own creation formats breaks its bookkeeping (TW2). By writer, not by
      // format: the G-buffer (normals, gloss), the shadow masks and the light buffer share the scene's RGBA8 desc, and a G-buffer mirror
      // lets the gloss go past 1 (0x3A1AA0D5 scales it by a material constant, the light pass raises it to ~a^3). The light buffer and the
      // scene are mirrored at their first light or material apply draw; the post chain follows them, its passes reading an upgraded view.
      texture_format_upgrades_type = TextureFormatUpgradesType::AllowedEnabled;
      enable_indirect_texture_format_upgrades = true;
      texture_format_upgrades_on_creation = false; // only the writers below (the format list and filters still gate their mirrors)
      enable_chain_indirect_texture_format_upgrades = ChainTextureFormatUpgradesType::DirectAndIndirectDependencies;
      texture_upgrade_formats = {
         reshade::api::format::r8g8b8a8_typeless, // dgVoodoo's D3D9 A8R8G8B8 surfaces: light buffer, scene and post copies, the canvas
      };
      for (const uint32_t hash : light_pixel_shaders)
      {
         auto_texture_format_upgrade_shader_hashes[hash] = {.rtv_slots = {0}};
      }
      for (const uint32_t hash : apply_pixel_shaders)
      {
         auto_texture_format_upgrade_shader_hashes[hash] = {.rtv_slots = {0}};
      }
      // "No1Px": dgVoodoo fills unused SRV slots with 1x1 placeholders, which pass the aspect filter
      texture_format_upgrades_2d_size_filters = (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainResolution | (uint32_t)TextureFormatUpgrades2DSizeFilters::SwapchainAspectRatio | (uint32_t)TextureFormatUpgrades2DSizeFilters::No1Px;

      // The upscalers' mip bias (see "OnPresent") reaches only upgraded samplers. AF16x, as in the other dgVoodoo mods;
      // "force_upgrade_linear_samplers" also gives the trilinear ones the bias (Core rewrites only anisotropic samplers otherwise, TW2).
      enable_samplers_upgrade = true; // boot-time only (cannot be changed after device creation)
      samplers_upgrade_mode = 4;
      force_upgrade_linear_samplers = true;

#if DEVELOPMENT
      // The gameplay uber post, LUT only, every frame
      forced_shader_names.emplace(Shader::Hash_StrToNum("583C9562"), "Uber Post (LUT only)");
      // The uber permutation and its post copy run only where the area's post-effect volume asks for an effect (live traces)
      forced_shader_names.emplace(Shader::Hash_StrToNum("C76B708E"), "Uber Post (DoF only)");
      forced_shader_names.emplace(Shader::Hash_StrToNum("934360AD"), "Scene Copy (uber input)");
      forced_shader_names.emplace(Shader::Hash_StrToNum("DE99AE79"), "Camera Motion Blur Downsample");
      forced_shader_names.emplace(Shader::Hash_StrToNum("87246FA1"), "Camera Motion Blur");
      forced_shader_names.emplace(Shader::Hash_StrToNum("D49CB77E"), "Video (VP6, BT.601 limited)");
      // Textured UI and overlays (RIG hologram, menus, MarkerPatch's achievements window), drawn after SMAA
      forced_shader_names.emplace(Shader::Hash_StrToNum("2A2C3104"), "UI");
      forced_shader_names.emplace(Shader::Hash_StrToNum("CBFBF98D"), "Deferred Sun Light (last light buffer pass)");
      forced_shader_names.emplace(Shader::Hash_StrToNum("5234894F"), "Deferred Material Apply");
      forced_shader_names.emplace(Shader::Hash_StrToNum("AC550A39"), "Deferred Material Apply");
      // MarkerPatch 3.3.1 (dinput8.dll, github.com/Wemino/MarkerPatch) draws its own passes and swaps some game shaders. Hashes mapped
      // offline from its embedded bytecode, the SMAA ones confirmed live. dgVoodoo specializes vertex shaders on the input layout, so
      // the VS hashes below can differ in game.
      // ImprovedAntiAliasingMode 2 (default) and 4: SMAA in place of the game's EdgeAA pass
      forced_shader_names.emplace(Shader::Hash_StrToNum("8736599C"), "MarkerPatch SMAA Edge Detection");
      forced_shader_names.emplace(Shader::Hash_StrToNum("764C97BB"), "MarkerPatch SMAA Blending Weight");
      forced_shader_names.emplace(Shader::Hash_StrToNum("BE49E10A"), "MarkerPatch SMAA Neighborhood Blending");
      // ImprovedAntiAliasingMode 1: FXAA in place of the EdgeAA pixel shaders (the game's own vertex shader)
      forced_shader_names.emplace(Shader::Hash_StrToNum("0A91885E"), "MarkerPatch FXAA");
      // ImprovedAntiAliasingMode 3 and 4: SSAA, the frame rendered at SSAAScale x the back buffer, then averaged into it
      forced_shader_names.emplace(Shader::Hash_StrToNum("113CFB69"), "MarkerPatch SSAA Downsample");
      // FixVertexNormals: replacements of game vertex shaders, so these draws lose their vanilla hashes
      forced_shader_names.emplace(Shader::Hash_StrToNum("7B1A4448"), "MarkerPatch Glass Reflection VS");
      forced_shader_names.emplace(Shader::Hash_StrToNum("202A858A"), "MarkerPatch Flashlight Overlay VS");
      forced_shader_names.emplace(Shader::Hash_StrToNum("90F04172"), "MarkerPatch Mesh Particle VS");
      forced_shader_names.emplace(Shader::Hash_StrToNum("368FBCDE"), "MarkerPatch Screen Mesh VS");
      forced_shader_names.emplace(Shader::Hash_StrToNum("90883B6F"), "MarkerPatch Motion Blur Velocity VS");
      // Live translations, matched by code to the offline ones
      forced_shader_names.emplace(Shader::Hash_StrToNum("76BC76F8"), "MarkerPatch Motion Blur Velocity VS");
      forced_shader_names.emplace(Shader::Hash_StrToNum("912E2945"), "MarkerPatch SMAA Edge Detection VS");
      forced_shader_names.emplace(Shader::Hash_StrToNum("92F966E1"), "MarkerPatch SMAA Blending Weight VS");
      forced_shader_names.emplace(Shader::Hash_StrToNum("4A9962FA"), "MarkerPatch SMAA Neighborhood Blending VS");
#endif

      game = new DeadSpace2();
   }
   else if (ul_reason_for_call == DLL_PROCESS_DETACH)
   {
      DeadSpace2::SetEvents(false);
   }

   CoreMain(hModule, ul_reason_for_call, lpReserved);

   return TRUE;
}
