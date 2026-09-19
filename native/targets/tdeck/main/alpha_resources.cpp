#include "alpha_resources.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "openu5/persistence.h"

namespace tdeck {
namespace {
constexpr char kTag[] = "AlphaResources";
constexpr uint8_t kMagic[8] = {'O','U','5','A','1','R','E','S'};
constexpr uint16_t kHeaderBytes = 32, kEntryBytes = 64;
constexpr uint32_t kPsram = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
uint16_t u16(const uint8_t *p) { return uint16_t(p[0]) | uint16_t(uint16_t(p[1]) << 8); }
uint32_t u32(const uint8_t *p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
int16_t i16(const uint8_t *p) { return static_cast<int16_t>(u16(p)); }
uint32_t crc_update(uint32_t crc, const uint8_t *data, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
    }
    return crc;
}
void *psram_alloc(size_t bytes) { return heap_caps_calloc(1, bytes, kPsram); }
} // namespace

void AlphaResourceOwners::release() {
    for (void *p : {static_cast<void *>(overworld), static_cast<void *>(underworld),
                    static_cast<void *>(small_tiles), static_cast<void *>(dungeons),
                    static_cast<void *>(npc_slots), static_cast<void *>(initial_gam),
                    static_cast<void *>(initial_ool),
                    static_cast<void *>(moon_phases), static_cast<void *>(search_objects),
                    static_cast<void *>(shard_spawns), static_cast<void *>(combat_maps),
                    static_cast<void *>(combat_sprites), static_cast<void *>(combat_enemies),
                    static_cast<void *>(combat_tables), static_cast<void *>(shop_records),
                    static_cast<void *>(shop_numbers), static_cast<void *>(shop_names),
                    static_cast<void *>(shop_text_offsets), static_cast<void *>(shop_text_records),
                    static_cast<void *>(misc_text_offsets), static_cast<void *>(misc_text_records),
                    static_cast<void *>(dialogue_data), static_cast<void *>(shrine_text),
                    static_cast<void *>(look_offsets), static_cast<void *>(look_text),
                    static_cast<void *>(signs), static_cast<void *>(sign_text),
                    static_cast<void *>(sign_raw), static_cast<void *>(question_text),
                    static_cast<void *>(intro_text), static_cast<void *>(intro_title),
                    static_cast<void *>(credits_panel), static_cast<void *>(creation_sprite_blob),
                    static_cast<void *>(demo_scene), static_cast<void *>(runes_font)})
        if (p) heap_caps_free(p);
    if (small_maps) heap_caps_free(small_maps);
    if (combat_map_views) heap_caps_free(combat_map_views);
    if (combat_enemy_views) heap_caps_free(combat_enemy_views);
    *this = {};
}

AlphaResourcePack::~AlphaResourcePack() { close(); }
void AlphaResourcePack::close() {
    if (file_) std::fclose(file_);
    file_ = nullptr; entry_count_ = 0;
}

const AlphaResourcePack::Entry *AlphaResourcePack::find(const char *name) const {
    for (size_t i = 0; i < entry_count_; ++i)
        if (std::strcmp(entries_[i].name, name) == 0) return entries_ + i;
    return nullptr;
}
esp_err_t AlphaResourcePack::read(const Entry &e, size_t at, void *out, size_t n) const {
    if (!file_ || !out || at > e.length || n > e.length - at ||
        std::fseek(file_, long(e.offset + at), SEEK_SET) != 0 || std::fread(out, 1, n, file_) != n)
        return ESP_FAIL;
    return ESP_OK;
}

esp_err_t AlphaResourcePack::open(const char *path, AlphaResourceReport &report) {
    close(); report = {};
    file_ = std::fopen(path, "rb");
    if (!file_) { ESP_LOGW(kTag, "Alpha resource pack not found at %s (errno=%d)", path, errno); return ESP_ERR_NOT_FOUND; }
    uint8_t header[kHeaderBytes]{};
    if (std::fread(header, 1, sizeof(header), file_) != sizeof(header) ||
        std::memcmp(header, kMagic, sizeof(kMagic)) != 0 ||
        u16(header + 8) != kAlphaResourceVersionMajor ||
        u16(header + 10) != kAlphaResourceVersionMinor || u16(header + 12) != kHeaderBytes ||
        u16(header + 14) != kEntryBytes) { close(); return ESP_ERR_INVALID_VERSION; }
    report.version_major = u16(header + 8);
    report.version_minor = u16(header + 10);
    entry_count_ = u32(header + 16); report.file_size = u32(header + 20);
    report.payload_crc32 = u32(header + 24); report.entry_count = uint32_t(entry_count_);
    if (!alpha_resource_entry_count_ok(uint32_t(entry_count_)) || std::fseek(file_, 0, SEEK_END) != 0 ||
        long(report.file_size) != std::ftell(file_)) { close(); return ESP_ERR_INVALID_SIZE; }
    uint32_t toc_crc = 0xffffffffU;
    for (size_t i = 0; i < entry_count_; ++i) {
        uint8_t raw[kEntryBytes]{};
        if (std::fseek(file_, long(kHeaderBytes + i * kEntryBytes), SEEK_SET) != 0 ||
            std::fread(raw, 1, sizeof(raw), file_) != sizeof(raw)) { close(); return ESP_FAIL; }
        toc_crc = crc_update(toc_crc, raw, sizeof(raw));
        auto &e = entries_[i]; std::memcpy(e.name, raw, 31); e.name[31] = 0;
        e.offset=u32(raw+32);e.length=u32(raw+36);e.crc32=u32(raw+40);e.records=u32(raw+44);e.stride=u32(raw+48);
        if (e.offset < kHeaderBytes + entry_count_ * kEntryBytes || e.offset > report.file_size ||
            e.length > report.file_size - e.offset) { close(); return ESP_ERR_INVALID_SIZE; }
    }
    if ((toc_crc ^ 0xffffffffU) != u32(header + 28)) { close(); return ESP_ERR_INVALID_CRC; }
    auto *scratch = static_cast<uint8_t *>(psram_alloc(4096));
    if (!scratch) { close(); return ESP_ERR_NO_MEM; }
    uint32_t payload_crc = 0xffffffffU;
    for (size_t i = 0; i < entry_count_; ++i) {
        auto &e=entries_[i]; uint32_t crc=0xffffffffU; size_t done=0;
        while(done<e.length){const size_t n=std::min<size_t>(4096,e.length-done);if(read(e,done,scratch,n)!=ESP_OK){heap_caps_free(scratch);close();return ESP_FAIL;}crc=crc_update(crc,scratch,n);payload_crc=crc_update(payload_crc,scratch,n);done+=n;}
        if((crc^0xffffffffU)!=e.crc32){heap_caps_free(scratch);close();return ESP_ERR_INVALID_CRC;}
    }
    heap_caps_free(scratch);
    if ((payload_crc ^ 0xffffffffU) != report.payload_crc32) { close(); return ESP_ERR_INVALID_CRC; }
    static const char *required[]={"init.gam","init.ool","overworld.map","underworld.map","smallmaps.bin","dungeons.bin","npcs.bin","worldtables.bin","combat.bin","shops.bin","shop-records.bin","misc-records.bin","talk.bin","shrines.bin","questions.bin","intro-text.bin","intro-title.rgb565","credits.rgb565","demo-scene.bin","look.bin","signs.bin","runes.ch","combatmaps.json","data.json","shoppe.json","talk-towne.json","talk-dwelling.json","talk-castle.json","talk-keep.json","look2.json","signs.json","endgame.json"};
    for (const char *name:required) if(!find(name)){ESP_LOGE(kTag,"Required entry missing: %s",name);close();return ESP_ERR_NOT_FOUND;}
    report.firmware_match = report.file_size == kExpectedAlphaResourceSize &&
                            report.payload_crc32 == kExpectedAlphaResourceCrc32;
    ESP_LOGI(kTag,"Alpha resource pack v%u.%u valid: %lu bytes, %lu entries, payload CRC=%08lx, expected SHA-256=%.12s, firmware_match=%d",
             unsigned(report.version_major), unsigned(report.version_minor),
             (unsigned long)report.file_size,(unsigned long)report.entry_count,
             (unsigned long)report.payload_crc32,kExpectedAlphaResourceSha256,
             report.firmware_match);
    if(!report.firmware_match)
        ESP_LOGE(kTag,"RESOURCE PACK MISMATCH expected size=%lu CRC=%08lx; stale/incompatible pack will not start",
                 (unsigned long)kExpectedAlphaResourceSize,
                 (unsigned long)kExpectedAlphaResourceCrc32);
    return ESP_OK;
}

esp_err_t AlphaResourcePack::load(AlphaResourceOwners &o, AlphaResourceReport &r) {
    if (!file_) return ESP_ERR_INVALID_STATE;
    o.release();
    const auto *over=find("overworld.map"),*under=find("underworld.map"),*small=find("smallmaps.bin"),
               *dungeon=find("dungeons.bin"),*npcs=find("npcs.bin"),*init=find("init.gam"),*init_ool=find("init.ool");
    if(!over||!under||!small||!dungeon||!npcs||!init||!init_ool||over->length!=65536||under->length!=65536||init->length<openu5::save::kGamSize||init_ool->length!=openu5::save::kOolSize)return ESP_ERR_INVALID_SIZE;
    o.overworld=static_cast<uint8_t*>(psram_alloc(over->length));o.underworld=static_cast<uint8_t*>(psram_alloc(under->length));o.initial_gam=static_cast<uint8_t*>(psram_alloc(init->length));o.initial_ool=static_cast<uint8_t*>(psram_alloc(init_ool->length));
    if(!o.overworld||!o.underworld||!o.initial_gam||!o.initial_ool){o.release();return ESP_ERR_NO_MEM;}
    if(read(*over,0,o.overworld,over->length)!=ESP_OK||read(*under,0,o.underworld,under->length)!=ESP_OK||read(*init,0,o.initial_gam,init->length)!=ESP_OK||read(*init_ool,0,o.initial_ool,init_ool->length)!=ESP_OK){o.release();return ESP_FAIL;}
    o.initial_gam_size=init->length;o.initial_ool_size=init_ool->length;
    uint8_t count_raw[4]{}; if(read(*small,0,count_raw,4)!=ESP_OK){o.release();return ESP_FAIL;}const uint32_t sc=u32(count_raw);
    if(!sc||sc>192||small->length<4+sc*12+sc*1024){o.release();return ESP_ERR_INVALID_SIZE;}
    o.small_maps=static_cast<openu5::MapData*>(heap_caps_calloc(sc,sizeof(openu5::MapData),MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
    o.small_tiles=static_cast<uint8_t*>(psram_alloc(size_t(sc)*1024));if(!o.small_maps||!o.small_tiles){o.release();return ESP_ERR_NO_MEM;}
    for(uint32_t i=0;i<sc;++i){uint8_t d[12]{};if(read(*small,4+i*12,d,12)!=ESP_OK||u32(d+8)!=1024||read(*small,u32(d+4),o.small_tiles+i*1024,1024)!=ESP_OK){o.release();return ESP_FAIL;}o.small_maps[i]={{d[0],i16(d+2)},o.small_tiles+i*1024,1024};}
    o.world={o.overworld,o.underworld,65536,65536,o.small_maps,sc};r.small_map_count=sc;
    if(read(*dungeon,0,count_raw,4)!=ESP_OK){o.release();return ESP_FAIL;}const uint32_t dc=u32(count_raw);if(!dc||dc>8||dungeon->length!=4+dc*516){o.release();return ESP_ERR_INVALID_SIZE;}
    o.dungeons=static_cast<openu5::DungeonData*>(psram_alloc(dc*sizeof(openu5::DungeonData)));if(!o.dungeons){o.release();return ESP_ERR_NO_MEM;}
    for(uint32_t i=0;i<dc;++i){uint8_t record[516]{};if(read(*dungeon,4+i*516,record,sizeof(record))!=ESP_OK){o.release();return ESP_FAIL;}o.dungeons[i].location=record[0];std::memcpy(o.dungeons[i].cells,record+4,512);}r.dungeon_count=dc;
    if(read(*npcs,0,count_raw,4)!=ESP_OK){o.release();return ESP_FAIL;}const uint32_t nc=u32(count_raw);if(npcs->length!=4+nc*24||nc>1024){o.release();return ESP_ERR_INVALID_SIZE;}
    o.npc_slots=static_cast<openu5::NpcSlot*>(psram_alloc(nc*sizeof(openu5::NpcSlot)));if(!o.npc_slots){o.release();return ESP_ERR_NO_MEM;}
    size_t starts[32]{};uint32_t counts[32]{};for(uint32_t i=0;i<nc;++i){uint8_t b[24]{};if(read(*npcs,4+i*24,b,24)!=ESP_OK||b[0]<1||b[0]>32){o.release();return ESP_FAIL;}const size_t loc=b[0]-1;if(!counts[loc])starts[loc]=i;++counts[loc];auto &s=o.npc_slots[i];s.slot=b[1];s.type=b[2];s.dialog=b[3];std::memcpy(s.ai,b+4,3);std::memcpy(s.x,b+7,3);std::memcpy(s.y,b+10,3);std::memcpy(s.z,b+13,3);std::memcpy(s.times,b+16,4);}
    for(size_t i=0;i<32;++i)
        o.npc_locations[i]={uint8_t(i+1),counts[i]?o.npc_slots+starts[i]:nullptr,counts[i],0};
    r.npc_count=nc;
    const auto *tables=find("worldtables.bin");uint8_t counts_raw[16]{};if(!tables||read(*tables,0,counts_raw,16)!=ESP_OK){o.release();return ESP_FAIL;}
    const uint32_t lc=u32(counts_raw),pc=u32(counts_raw+4),qc=u32(counts_raw+8),shc=u32(counts_raw+12);
    const size_t expected=16+size_t(lc)*2+size_t(pc)*4+size_t(qc)*24+size_t(shc)*12;if(lc>40||expected!=tables->length){o.release();return ESP_ERR_INVALID_SIZE;}
    size_t at=16;for(uint32_t i=0;i<lc;++i){uint8_t xy[2];if(read(*tables,at,xy,2)!=ESP_OK){o.release();return ESP_FAIL;}o.location_x[i]=xy[0];o.location_y[i]=xy[1];at+=2;}o.location_count=lc;
    o.moon_phases=static_cast<int32_t*>(psram_alloc(size_t(pc)*sizeof(int32_t)));o.search_objects=static_cast<openu5::SearchObject*>(psram_alloc(size_t(qc)*sizeof(openu5::SearchObject)));o.shard_spawns=static_cast<openu5::ShardSpawn*>(psram_alloc(size_t(shc)*sizeof(openu5::ShardSpawn)));
    if((pc&&!o.moon_phases)||(qc&&!o.search_objects)||(shc&&!o.shard_spawns)){o.release();return ESP_ERR_NO_MEM;}
    for(uint32_t i=0;i<pc;++i){uint8_t b[4];read(*tables,at,b,4);o.moon_phases[i]=int32_t(u32(b));at+=4;}o.moon_phase_count=pc;
    for(uint32_t i=0;i<qc;++i){int32_t*v=&o.search_objects[i].id;for(int k=0;k<6;++k){uint8_t b[4];read(*tables,at,b,4);v[k]=int32_t(u32(b));at+=4;}}o.search_count=qc;
    for(uint32_t i=0;i<shc;++i){int32_t*v=&o.shard_spawns[i].x;for(int k=0;k<3;++k){uint8_t b[4];read(*tables,at,b,4);v[k]=int32_t(u32(b));at+=4;}}o.shard_spawn_count=shc;

    const auto *combat=find("combat.bin");uint8_t combat_header[16]{};
    if(!combat||read(*combat,0,combat_header,sizeof(combat_header))!=ESP_OK){o.release();return ESP_FAIL;}
    const uint32_t cmc=u32(combat_header),cec=u32(combat_header+4),ctc=u32(combat_header+8);
    constexpr size_t map_bytes=556,enemy_bytes=88;
    const size_t combat_expected=16+size_t(cmc)*map_bytes+size_t(cec)*enemy_bytes+size_t(ctc)*4*4;
    if(!cmc||cmc>256||!cec||cec>128||!ctc||ctc>256||combat_expected!=combat->length){o.release();return ESP_ERR_INVALID_SIZE;}
    o.combat_maps=static_cast<openu5::CombatMap*>(psram_alloc(size_t(cmc)*sizeof(openu5::CombatMap)));
    o.combat_sprites=static_cast<uint8_t*>(psram_alloc(size_t(cmc)*16));
    o.combat_enemies=static_cast<openu5::CombatEnemy*>(psram_alloc(size_t(cec)*sizeof(openu5::CombatEnemy)));
    o.combat_tables=static_cast<int32_t*>(psram_alloc(size_t(ctc)*4*sizeof(int32_t)));
    o.combat_map_views=static_cast<const openu5::CombatMap**>(heap_caps_calloc(cmc,sizeof(void*),MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
    o.combat_enemy_views=static_cast<const openu5::CombatEnemy**>(heap_caps_calloc(cec,sizeof(void*),MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
    if(!o.combat_maps||!o.combat_sprites||!o.combat_enemies||!o.combat_tables||!o.combat_map_views||!o.combat_enemy_views){o.release();return ESP_ERR_NO_MEM;}
    at=16;
    for(uint32_t i=0;i<cmc;++i){uint8_t b[map_bytes]{};if(read(*combat,at,b,sizeof(b))!=ESP_OK){o.release();return ESP_FAIL;}at+=sizeof(b);size_t p=0;auto &m=o.combat_maps[i];m.index=int32_t(u32(b+p));p+=4;for(auto &v:m.tiles){v=i16(b+p);p+=2;}for(auto &direction:m.starts)for(auto &point:direction){point={i16(b+p),i16(b+p+2)};p+=4;}for(auto &point:m.units){point={i16(b+p),i16(b+p+2)};p+=4;}for(auto &v:m.start_count)v=b[p++];m.unit_count=b[p++];m.trigger_count=b[p++];for(auto &t:m.triggers){t.tile=i16(b+p);t.at={i16(b+p+2),i16(b+p+4)};t.first={i16(b+p+6),i16(b+p+8)};t.second={i16(b+p+10),i16(b+p+12)};p+=14;}for(int j=0;j<16;++j){o.combat_sprites[i*16+j]=uint8_t(i16(b+p));p+=2;}o.combat_map_views[i]=&m;}
    for(uint32_t i=0;i<cec;++i){uint8_t b[enemy_bytes]{};if(read(*combat,at,b,sizeof(b))!=ESP_OK){o.release();return ESP_FAIL;}at+=sizeof(b);auto &e=o.combat_enemies[i];const int32_t values[10]={int32_t(u32(b)),int32_t(u32(b+4)),int32_t(u32(b+8)),int32_t(u32(b+12)),int32_t(u32(b+16)),int32_t(u32(b+20)),int32_t(u32(b+24)),int32_t(u32(b+28)),int32_t(u32(b+32)),int32_t(u32(b+36))};e.index=values[0];e.strength=values[1];e.dexterity=values[2];e.intelligence=values[3];e.armor=values[4];e.damage=values[5];e.hp=values[6];e.range=values[7];e.treasure=values[8];e.max_per_map=values[9];e.abilities=u16(b+40);e.move_class=b[42];e.stationary=b[43]!=0;e.tile=i16(b+44);e.name=reinterpret_cast<const char*>(b+46);e.group_name=reinterpret_cast<const char*>(b+67);
        // Names in the stack record cannot be borrowed. They are copied into one stable PSRAM block below.
        o.combat_enemy_views[i]=&e;}
    // 42 bytes per enemy: two 21-byte, NUL-padded ASCII fields.
    char *enemy_names=static_cast<char*>(psram_alloc(size_t(cec)*42));if(!enemy_names){o.release();return ESP_ERR_NO_MEM;}
    // Re-read only the name tails so CombatEnemy string pointers remain valid.
    for(uint32_t i=0;i<cec;++i){uint8_t names[42]{};const size_t base=16+size_t(cmc)*map_bytes+size_t(i)*enemy_bytes+46;if(read(*combat,base,names,sizeof(names))!=ESP_OK){heap_caps_free(enemy_names);o.release();return ESP_FAIL;}std::memcpy(enemy_names+i*42,names,42);o.combat_enemies[i].name=enemy_names+i*42;o.combat_enemies[i].group_name=enemy_names+i*42+21;}
    // Reuse shop_names ownership as a combined stable text arena, extended after shop parsing below.
    o.shop_names=enemy_names;
    if(read(*combat,at,o.combat_tables,size_t(ctc)*4*4)!=ESP_OK){o.release();return ESP_FAIL;}o.combat_map_count=cmc;o.combat_enemy_count=cec;o.combat_table_count=ctc;

    const auto *shops=find("shops.bin");uint8_t shop_header[32]{};if(!shops||read(*shops,0,shop_header,sizeof(shop_header))!=ESP_OK){o.release();return ESP_FAIL;}
    const uint32_t src=u32(shop_header);uint32_t shop_counts[7]{};size_t sn=0;for(int i=0;i<7;++i){shop_counts[i]=u32(shop_header+4+i*4);sn+=shop_counts[i];}
    const size_t shops_expected=32+size_t(src)*76+sn*4;if(!src||src>64||shops_expected!=shops->length){o.release();return ESP_ERR_INVALID_SIZE;}
    o.shop_records=static_cast<openu5::ShopRecord*>(psram_alloc(size_t(src)*sizeof(openu5::ShopRecord)));o.shop_numbers=static_cast<int32_t*>(psram_alloc(sn*sizeof(int32_t)));
    char *all_names=static_cast<char*>(psram_alloc(size_t(cec)*42+size_t(src)*64));if(!o.shop_records||!o.shop_numbers||!all_names){if(all_names)heap_caps_free(all_names);o.release();return ESP_ERR_NO_MEM;}
    std::memcpy(all_names,o.shop_names,size_t(cec)*42);heap_caps_free(o.shop_names);o.shop_names=all_names;for(uint32_t i=0;i<cec;++i){o.combat_enemies[i].name=all_names+i*42;o.combat_enemies[i].group_name=all_names+i*42+21;}
    at=32;char *shop_text=all_names+size_t(cec)*42;
    for(uint32_t i=0;i<src;++i){uint8_t b[76]{};if(read(*shops,at,b,sizeof(b))!=ESP_OK){o.release();return ESP_FAIL;}at+=sizeof(b);auto&r0=o.shop_records[i];r0.location=int32_t(u32(b));r0.type=openu5::ShopType(b[4]);r0.index=int32_t(u32(b+8));std::memcpy(shop_text+i*64,b+12,32);std::memcpy(shop_text+i*64+32,b+44,32);r0.name=shop_text+i*64;r0.keeper=shop_text+i*64+32;}
    if(read(*shops,at,o.shop_numbers,sn*4)!=ESP_OK){o.release();return ESP_FAIL;}size_t no=0;o.shop_data.records=o.shop_records;o.shop_data.record_count=src;
#define SHOP_ARRAY(field,index) do{o.shop_data.field={o.shop_numbers+no,shop_counts[index]};no+=shop_counts[index];}while(0)
    SHOP_ARRAY(equipment_prices,0);SHOP_ARRAY(weapons,1);SHOP_ARRAY(reagent_prices,2);SHOP_ARRAY(reagent_grants,3);SHOP_ARRAY(heal_prices,4);SHOP_ARRAY(cure_prices,5);SHOP_ARRAY(resurrect_prices,6);
#undef SHOP_ARRAY
    o.shop_record_count=src;o.shop_number_count=sn;
    const auto *shop_text_records=find("shop-records.bin");uint8_t shop_text_head[4]{};
    if(!shop_text_records||read(*shop_text_records,0,shop_text_head,4)!=ESP_OK){o.release();return ESP_FAIL;}
    const uint32_t shop_text_count=u32(shop_text_head);const size_t shop_text_dir=4+size_t(shop_text_count+1)*4;
    if(!shop_text_count||shop_text_count>512||shop_text_dir>shop_text_records->length){o.release();return ESP_ERR_INVALID_SIZE;}
    o.shop_text_offsets=static_cast<uint32_t*>(psram_alloc(size_t(shop_text_count+1)*4));
    o.shop_text_records=static_cast<char*>(psram_alloc(shop_text_records->length-shop_text_dir));
    if(!o.shop_text_offsets||!o.shop_text_records){o.release();return ESP_ERR_NO_MEM;}
    if(read(*shop_text_records,4,o.shop_text_offsets,size_t(shop_text_count+1)*4)!=ESP_OK||
       read(*shop_text_records,shop_text_dir,o.shop_text_records,shop_text_records->length-shop_text_dir)!=ESP_OK||
       o.shop_text_offsets[shop_text_count]!=shop_text_records->length-shop_text_dir){o.release();return ESP_ERR_INVALID_SIZE;}
    for(uint32_t i=0;i<shop_text_count;++i)if(o.shop_text_offsets[i]>=o.shop_text_offsets[i+1]||
       o.shop_text_offsets[i+1]>shop_text_records->length-shop_text_dir||
       o.shop_text_records[o.shop_text_offsets[i+1]-1]!=0){o.release();return ESP_ERR_INVALID_SIZE;}
    o.shop_text_record_count=shop_text_count;
    const auto *misc_text_records=find("misc-records.bin");uint8_t misc_text_head[4]{};
    if(!misc_text_records||read(*misc_text_records,0,misc_text_head,4)!=ESP_OK){o.release();return ESP_FAIL;}
    const uint32_t misc_text_count=u32(misc_text_head);const size_t misc_text_dir=4+size_t(misc_text_count+1)*4;
    if(!misc_text_count||misc_text_count>512||misc_text_dir>misc_text_records->length){o.release();return ESP_ERR_INVALID_SIZE;}
    o.misc_text_offsets=static_cast<uint32_t*>(psram_alloc(size_t(misc_text_count+1)*4));
    o.misc_text_records=static_cast<char*>(psram_alloc(misc_text_records->length-misc_text_dir));
    if(!o.misc_text_offsets||!o.misc_text_records){o.release();return ESP_ERR_NO_MEM;}
    if(read(*misc_text_records,4,o.misc_text_offsets,size_t(misc_text_count+1)*4)!=ESP_OK||
       read(*misc_text_records,misc_text_dir,o.misc_text_records,misc_text_records->length-misc_text_dir)!=ESP_OK||
       !validate_misc_text_records(o.misc_text_offsets,o.misc_text_records,misc_text_records->length-misc_text_dir,misc_text_count)){o.release();return ESP_ERR_INVALID_SIZE;}
    o.misc_text_record_count=misc_text_count;
    const auto *talk=find("talk.bin");if(!talk||talk->length<4){o.release();return ESP_ERR_INVALID_SIZE;}o.dialogue_data=static_cast<uint8_t*>(psram_alloc(talk->length));if(!o.dialogue_data){o.release();return ESP_ERR_NO_MEM;}if(read(*talk,0,o.dialogue_data,talk->length)!=ESP_OK){o.release();return ESP_FAIL;}o.dialogue_data_size=talk->length;
    const auto *shrines=find("shrines.bin");uint8_t shrine_count[4]{};if(!shrines||shrines->length!=4+8*56||read(*shrines,0,shrine_count,4)!=ESP_OK||u32(shrine_count)!=8){o.release();return ESP_ERR_INVALID_SIZE;}o.shrine_text=static_cast<char16_t*>(psram_alloc(8*24*sizeof(char16_t)));if(!o.shrine_text){o.release();return ESP_ERR_NO_MEM;}for(int i=0;i<8;++i){uint8_t b[56]{};if(read(*shrines,4+i*56,b,sizeof(b))!=ESP_OK){o.release();return ESP_FAIL;}char16_t*v=o.shrine_text+i*24;size_t vn=0,mn=0;for(int j=0;j<16;++j){v[j]=char16_t(u16(b+j*2));if(v[j])vn=j+1;}for(int j=0;j<8;++j){v[16+j]=char16_t(u16(b+32+j*2));if(v[16+j])mn=j+1;}o.shrine_data.virtues[i]={v,vn};o.shrine_data.mantras[i]={v+16,mn};o.shrine_data.x[i]=int32_t(u32(b+48));o.shrine_data.y[i]=int32_t(u32(b+52));}o.shrine_data.count=8;
    const auto *questions=find("questions.bin");uint8_t question_head[4]{};if(!questions||questions->length<4+29*4||read(*questions,0,question_head,4)!=ESP_OK||u32(question_head)!=28){o.release();return ESP_ERR_INVALID_SIZE;}uint8_t question_dir[29*4]{};if(read(*questions,4,question_dir,sizeof(question_dir))!=ESP_OK){o.release();return ESP_FAIL;}const size_t question_data_at=4+sizeof(question_dir),question_bytes=questions->length-question_data_at;if(u32(question_dir+28*4)!=question_bytes){o.release();return ESP_ERR_INVALID_SIZE;}o.question_text=static_cast<char*>(psram_alloc(question_bytes));if(!o.question_text){o.release();return ESP_ERR_NO_MEM;}if(read(*questions,question_data_at,o.question_text,question_bytes)!=ESP_OK){o.release();return ESP_FAIL;}for(size_t i=0;i<28;++i){const auto begin=u32(question_dir+i*4),end=u32(question_dir+(i+1)*4);if(begin>=end||end>question_bytes||o.question_text[end-1]!=0){o.release();return ESP_ERR_INVALID_SIZE;}o.questions[i]=o.question_text+begin;}o.question_count=28;
    const auto *intro=find("intro-text.bin");uint8_t intro_head[4]{};if(!intro||intro->length<4+22*4||read(*intro,0,intro_head,4)!=ESP_OK||u32(intro_head)!=21){o.release();return ESP_ERR_INVALID_SIZE;}uint8_t intro_dir[22*4]{};if(read(*intro,4,intro_dir,sizeof(intro_dir))!=ESP_OK){o.release();return ESP_FAIL;}const size_t intro_data_at=4+sizeof(intro_dir),intro_bytes=intro->length-intro_data_at;if(u32(intro_dir+21*4)!=intro_bytes){o.release();return ESP_ERR_INVALID_SIZE;}o.intro_text=static_cast<char*>(psram_alloc(intro_bytes));if(!o.intro_text){o.release();return ESP_ERR_NO_MEM;}if(read(*intro,intro_data_at,o.intro_text,intro_bytes)!=ESP_OK){o.release();return ESP_FAIL;}for(size_t i=0;i<21;++i){const auto begin=u32(intro_dir+i*4),end=u32(intro_dir+(i+1)*4);if(begin>=end||end>intro_bytes||o.intro_text[end-1]!=0){o.release();return ESP_ERR_INVALID_SIZE;}o.intro_scenes[i]=o.intro_text+begin;}o.intro_scene_count=21;
    const auto *title=find("intro-title.rgb565");constexpr size_t title_bytes=4*320*110*2;if(!title||title->length!=title_bytes){o.release();return ESP_ERR_INVALID_SIZE;}o.intro_title=static_cast<uint16_t*>(psram_alloc(title_bytes));if(!o.intro_title){o.release();return ESP_ERR_NO_MEM;}if(read(*title,0,o.intro_title,title_bytes)!=ESP_OK){o.release();return ESP_FAIL;}
    const auto *credits=find("credits.rgb565");constexpr size_t credits_bytes=288*137*2;if(!credits||credits->length!=credits_bytes){o.release();return ESP_ERR_INVALID_SIZE;}o.credits_panel=static_cast<uint16_t*>(psram_alloc(credits_bytes));if(!o.credits_panel){o.release();return ESP_ERR_NO_MEM;}if(read(*credits,0,o.credits_panel,credits_bytes)!=ESP_OK){o.release();return ESP_FAIL;}
    const auto *creation=find("creation-sprites.bin");constexpr size_t creation_header=16+11*16;if(!creation||creation->length<creation_header){o.release();return ESP_ERR_INVALID_SIZE;}o.creation_sprite_blob=static_cast<uint8_t*>(psram_alloc(creation->length));if(!o.creation_sprite_blob){o.release();return ESP_ERR_NO_MEM;}if(read(*creation,0,o.creation_sprite_blob,creation->length)!=ESP_OK||std::memcmp(o.creation_sprite_blob,"OU5CRT1",7)!=0||u32(o.creation_sprite_blob+8)!=11||u32(o.creation_sprite_blob+12)!=creation_header){o.release();return ESP_ERR_INVALID_SIZE;}for(size_t i=0;i<11;++i){const uint8_t*r=o.creation_sprite_blob+16+i*16;const auto id=u16(r),w=u16(r+2),h=u16(r+4);const auto offset=u32(r+8),bytes=u32(r+12);if(id>=11||o.creation_sprites[id].pixels||!w||!h||bytes!=size_t(w)*h*3||offset>creation->length||bytes>creation->length-offset){o.release();return ESP_ERR_INVALID_SIZE;}o.creation_sprites[id]={w,h,o.creation_sprite_blob+offset};}for(const auto&s:o.creation_sprites)if(!s.pixels){o.release();return ESP_ERR_INVALID_SIZE;}
    const auto *demo=find("demo-scene.bin");constexpr size_t demo_bytes=16+304+655;if(!demo||demo->length!=demo_bytes){o.release();return ESP_ERR_INVALID_SIZE;}o.demo_scene=static_cast<uint8_t*>(psram_alloc(demo_bytes));if(!o.demo_scene){o.release();return ESP_ERR_NO_MEM;}if(read(*demo,0,o.demo_scene,demo_bytes)!=ESP_OK||std::memcmp(o.demo_scene,"OU5DEMO1",8)!=0||o.demo_scene[8]!=19||o.demo_scene[9]!=4||o.demo_scene[10]!=4||u16(o.demo_scene+12)!=655){o.release();return ESP_ERR_INVALID_SIZE;}o.intro_view={o.demo_scene+16,304,o.demo_scene+320,655};
    const auto *runes=find("runes.ch");if(!runes||runes->length!=1024){o.release();return ESP_ERR_INVALID_SIZE;}o.runes_font=static_cast<uint8_t*>(psram_alloc(1024));if(!o.runes_font){o.release();return ESP_ERR_NO_MEM;}if(read(*runes,0,o.runes_font,1024)!=ESP_OK){o.release();return ESP_FAIL;}
    const auto *look=find("look.bin");uint8_t look_head[4]{};if(!look||read(*look,0,look_head,4)!=ESP_OK){o.release();return ESP_FAIL;}const uint32_t look_count=u32(look_head);const size_t look_dir=4+size_t(look_count+1)*4;if(!look_count||look_count>1024||look_dir>look->length){o.release();return ESP_ERR_INVALID_SIZE;}o.look_offsets=static_cast<uint32_t*>(psram_alloc(size_t(look_count+1)*4));o.look_text=static_cast<char*>(psram_alloc(look->length-look_dir));if(!o.look_offsets||!o.look_text){o.release();return ESP_ERR_NO_MEM;}if(read(*look,4,o.look_offsets,size_t(look_count+1)*4)!=ESP_OK||read(*look,look_dir,o.look_text,look->length-look_dir)!=ESP_OK||o.look_offsets[look_count]!=look->length-look_dir){o.release();return ESP_ERR_INVALID_SIZE;}o.look_count=look_count;
    const auto *signs=find("signs.bin");uint8_t sign_head[16]{};if(!signs||read(*signs,0,sign_head,sizeof(sign_head))!=ESP_OK){o.release();return ESP_FAIL;}
    const uint32_t sign_count=u32(sign_head),sign_stride=u32(sign_head+4),sign_text_bytes=u32(sign_head+8),sign_raw_bytes=u32(sign_head+12);
    constexpr size_t sign_header=16,sign_record=24;const size_t sign_text_at=sign_header+size_t(sign_count)*sign_record,sign_raw_at=sign_text_at+sign_text_bytes;
    if(!sign_count||sign_count>256||sign_stride!=sign_record||sign_raw_at>signs->length||size_t(sign_raw_bytes)!=signs->length-sign_raw_at){o.release();return ESP_ERR_INVALID_SIZE;}
    o.signs=static_cast<openu5::LookSignRecord*>(psram_alloc(size_t(sign_count)*sizeof(openu5::LookSignRecord)));o.sign_text=static_cast<char*>(psram_alloc(sign_text_bytes));o.sign_raw=static_cast<uint8_t*>(psram_alloc(sign_raw_bytes));
    if(!o.signs||!o.sign_text||(sign_raw_bytes&&!o.sign_raw)){o.release();return ESP_ERR_NO_MEM;}
    if(read(*signs,sign_text_at,o.sign_text,sign_text_bytes)!=ESP_OK||(sign_raw_bytes&&read(*signs,sign_raw_at,o.sign_raw,sign_raw_bytes)!=ESP_OK)){o.release();return ESP_FAIL;}
    for(uint32_t i=0;i<sign_count;++i){uint8_t b[sign_record]{};if(read(*signs,sign_header+size_t(i)*sign_record,b,sizeof(b))!=ESP_OK){o.release();return ESP_FAIL;}const uint32_t to=u32(b+8),tl=u32(b+12),ro=u32(b+16),rl=u32(b+20);if(to>=sign_text_bytes||tl>=sign_text_bytes-to||o.sign_text[to+tl]!=0||ro>sign_raw_bytes||rl>sign_raw_bytes-ro){o.release();return ESP_ERR_INVALID_SIZE;}auto &record=o.signs[i];record.map={b[0],i16(b+2)};record.x=b[4];record.y=b[5];record.value={o.sign_text+to,rl?o.sign_raw+ro:nullptr,rl};}
    o.sign_count=sign_count;
    o.psram_bytes=over->length+under->length+init->length+init_ool->length+size_t(sc)*1024+dc*sizeof(openu5::DungeonData)+nc*sizeof(openu5::NpcSlot)+size_t(pc)*4+size_t(qc)*sizeof(openu5::SearchObject)+size_t(shc)*sizeof(openu5::ShardSpawn)+size_t(cmc)*(sizeof(openu5::CombatMap)+16)+size_t(cec)*(sizeof(openu5::CombatEnemy)+42)+size_t(ctc)*16+size_t(src)*(sizeof(openu5::ShopRecord)+64)+sn*4+talk->length+8*24*sizeof(char16_t)+question_bytes+intro_bytes+title_bytes+credits_bytes+creation->length+demo_bytes+1024+size_t(shop_text_count+1)*4+shop_text_records->length-shop_text_dir+size_t(misc_text_count+1)*4+misc_text_records->length-misc_text_dir+size_t(look_count+1)*4+look->length-look_dir+size_t(sign_count)*sizeof(openu5::LookSignRecord)+sign_text_bytes+sign_raw_bytes;
    ESP_LOGI(kTag,"Loaded owners: %lu small floors, %lu dungeons, %lu NPC records; PSRAM=%zu",(unsigned long)sc,(unsigned long)dc,(unsigned long)nc,o.psram_bytes);
    return ESP_OK;
}
} // namespace tdeck
