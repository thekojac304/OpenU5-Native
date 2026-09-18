#include "openu5/quest.h"
#include <algorithm>
namespace openu5 {
const char *quest_flag_name(QuestFlag f) {
    static constexpr const char *names[] = {"shadowlord-dead:falsehood", "shadowlord-dead:hatred",
        "shadowlord-dead:cowardice", "in-doom", "game-won", "word-spoken:33", "word-spoken:34",
        "word-spoken:35", "word-spoken:36", "word-spoken:37", "word-spoken:38", "word-spoken:39", "word-spoken:40"};
    return unsigned(f) < unsigned(QuestFlag::Count) ? names[unsigned(f)] : "";
}
namespace {
std::u16string upper(TalkText input) {
    struct Mapping { uint32_t code, out[3]; uint8_t count; };
    static constexpr Mapping mappings[] = {
#include "quest_case.inc"
    };
    std::u16string result;
    auto append=[&](uint32_t code) {
        if (code<=65535) result+=char16_t(code);
        else { code-=65536; result+=char16_t(0xd800+(code>>10)); result+=char16_t(0xdc00+(code&1023)); }
    };
    for (size_t i=0;i<input.size();++i) {
        uint32_t code=input[i];
        if (code>=0xd800 && code<=0xdbff && i+1<input.size() && input[i+1]>=0xdc00 && input[i+1]<=0xdfff)
            code=0x10000+((code-0xd800)<<10)+(input[++i]-0xdc00);
        const auto *m=std::lower_bound(std::begin(mappings),std::end(mappings),code,
            [](const Mapping &a,uint32_t b){return a.code<b;});
        if (m!=std::end(mappings) && m->code==code) for (uint8_t n=0;n<m->count;++n) append(m->out[n]);
        else append(code);
    }
    return result;
}
bool contains(TalkText said, TalkText word) {
    return !word.empty() && upper(said).find(upper(word)) != std::u16string::npos;
}
constexpr int32_t flame_y[] = {9,3,16}, flame_floor[] = {2,1,-1};
}
PlotItem plot_item_for_npc_type(int32_t type) {
    switch (type) { case 0xb5: return PlotItem::Crown; case 0xb6: return PlotItem::Sceptre;
    case 14: return PlotItem::WoodenBox; case 27: return PlotItem::Carpet; default: return PlotItem::None; }
}
const char *grant_plot_item(GameState &g, PlotItem item) {
    static constexpr const char *messages[] = {"The Amulet of Lord British!\n", "The Crown of Lord British!\n",
        "The Sceptre of Lord British!\n", "A sandalwood box!\n", "A magic carpet!\n",
        "The Shard of\nFalsehood!\n", "The Shard of\nHatred!\n", "The Shard of\nCowardice!\n"};
    unsigned i = unsigned(item);
    if (i >= 8) return "";
    if (i < 3) g.quest.artifacts[i] = true;
    else if (item == PlotItem::WoodenBox) g.wooden_box = true;
    else if (item == PlotItem::Carpet) g.magic_carpets = int32_t(std::min(int64_t(99), int64_t(g.magic_carpets)+1));
    else g.quest.shards[i-5] = true;
    return messages[i];
}
bool can_reach_doom(const GameState &g) { return (g.quest.flags & 7) == 7; }
DestroyShadowlordResult destroy_shadowlord(GameState &g, int32_t i) {
    if (i<0 || i>2) return {};
    constexpr const char16_t *names[]={u"Faulinei",u"Astaroth",u"Nosfentor"};
    constexpr const char16_t *domains[]={u"Falsehood",u"Hatred",u"Cowardice"};
    constexpr const char16_t *flames[]={u"the Flame of Truth",u"the Flame of Love",u"the Flame of Courage"};
    if (quest_flag(g.quest,QuestFlag(i))) return {true,std::u16string(names[i])+u" is already no more."};
    if (!g.quest.shards[i]) return {false,std::u16string(u"Without the Shard of ")+domains[i]+u", "+names[i]+u" cannot be undone."};
    set_quest_flag(g.quest,QuestFlag(i));
    return {true,std::u16string(u"Thou dost cast the Shard of ")+domains[i]+u" into "+flames[i]+u"! "+names[i]+u" shrieks and is consumed utterly \u2014 one Shadowlord falls."};
}
bool endgame_ready(const GameState &g) {
    return can_reach_doom(g) && g.quest.artifacts[0] && g.quest.artifacts[1] && g.quest.artifacts[2];
}
RescueEnding rescue_lord_british(GameState &g, bool absorption) {
    if (!absorption && (!endgame_ready(g) || !quest_flag(g.quest,QuestFlag::InDoom))) return RescueEnding::Incomplete;
    set_quest_flag(g.quest,QuestFlag::GameWon);
    return g.wooden_box ? RescueEnding::Victory : RescueEnding::Stranded;
}
QuestPlaytime endgame_playtime(const GameState &g) {
    QuestPlaytime r{g.time.year-139,g.time.month-4,g.time.day-5};
    if (r.days < 0) { r.days += 28; --r.months; }
    if (r.months < 0) { r.months += 13; --r.years; }
    return r;
}
int32_t match_yell_name(TalkText word) {
    constexpr TalkText names[] = {u"FAULINEI",u"ASTAROTH",u"NOSFENTOR"};
    for (int i=0;i<3;++i) if (contains(word,names[i])) return i;
    return -1;
}
bool quest_text_equal(TalkText a,TalkText b){return upper(a)==upper(b);}
bool quest_text_contains(TalkText input,TalkText word){return contains(input,word);}
int32_t summon_shadowlord(TalkText word, int32_t y, const bool (&alive)[3], bool present) {
    const auto i=match_yell_name(word);
    return i < 0 || y < 2 || !alive[i] || present ? -1 : i;
}
RitualResult cast_shard_into_flame(int32_t i, int32_t x, int32_t y, int32_t location,
                                 int32_t floor, int32_t above, int32_t summoned) {
    RitualResult r;
    if (i<0 || i>2) return r;
    constexpr const char *headers[] = {
        "Gem Shard\n\nThou dost hold above thee the evil Shard of Falsehood...",
        "Gem Shard\n\nThou dost hold above thee the evil Shard of Hatred...",
        "Gem Shard\n\nThou dost hold above thee the evil Shard of Cowardice..."};
    constexpr const char *flames[] = {"\n\n...and cast it into the Flame of Truth!\n",
        "\n\n...and cast it into the Flame of Love!\n", "\n\n...and cast it into the Flame of Courage!\n"};
    constexpr const char *dooms[] = {"\nThe doom of the Shadowlord Faulinei is wrought!\n",
        "\nThe doom of the Shadowlord Astaroth is wrought!\n", "\nThe doom of the Shadowlord Nosfentor is wrought!\n"};
    r.text.lines[r.text.count++]=headers[i];
    if (x!=15 || y!=flame_y[i] || location!=30+i || floor!=flame_floor[i]) {
        r.text.lines[r.text.count++]="\n\nNo effect!\n"; return r;
    }
    r.text.lines[r.text.count++]=flames[i];
    if (above!=0xfc || summoned!=i) return r;
    r.text.lines[r.text.count++]=dooms[i]; r.destroyed=true; r.doom_bit=uint8_t(2u<<unsigned(i)); return r;
}
void apply_shard_destruction(GameState &g, TurnState &t, int32_t i) {
    if (i<0 || i>2) return;
    set_quest_flag(g.quest,QuestFlag(i));
    if (t.has_shadowlords) t.shadowlord_locations[size_t(i)]=255;
    g.quest.shards[i]=false;
    g.quest.doom_bits |= 2<<i; g.quest.optional_present |= 4;
    g.npc_dead[28] |= uint32_t(1) << unsigned(6-i);
    g.quest.summoned=-1; g.quest.optional_present &= uint8_t(~8u);
}
YellWordResult yell_word_of_power(TalkView<TalkText> words, TalkText said, TalkView<int32_t> adjacent) {
    YellWordResult r;
    said=talk_trim(said);
    if (!said.empty()) for (size_t i=0;i<words.size;++i) if (contains(said,words[i])) {
        r.uttered=true; const auto loc=int32_t(33+i);
        if (loc<=40 && std::find(adjacent.begin(),adjacent.end(),loc)!=adjacent.end()) { r.opened=true; r.location=loc; }
        break;
    }
    if (r.uttered) r.text.lines[r.text.count++]="\nA word of power is uttered\n";
    if (!r.opened) r.text.lines[r.text.count++]="\nNo effect!\n";
    return r;
}
ShrineMode shrine_mode(const GameState &g,uint8_t v) {
    if (v>=8 || !(g.quest.shrine_visited & (1<<v))) return ShrineMode::ShowMantra;
    return (g.quest.shrine_quest & (1<<v)) ? ShrineMode::QuestComplete : ShrineMode::Donation;
}
void shrine_show_mantra(GameState &g,uint8_t v) {
    if (v<8) { g.quest.shrine_quest |= 1<<v; g.quest.optional_present |= 1; }
}
bool shrine_visit_check(uint8_t v,TalkText virtue,TalkView<TalkText> mantras,const ShrineData &d) {
    if (v>=d.count || !contains(virtue,d.virtues[v]) || mantras.size!=3) return false;
    for (auto m:mantras) if (!contains(m,d.mantras[v])) return false;
    return true;
}
CodexLesson shrine_codex_lesson(GameState &g) {
    for (int i=0;i<8;++i) if (g.quest.shrine_quest & (1<<i)) {
        g.quest.shrine_visited |= 1<<i; g.quest.optional_present |= 2;
        return {i,(g.quest.shrine_visited & 255)==255};
    }
    return {};
}
DonationResult shrine_donate(GameState &g,int32_t n) {
    if (n<=0) return {};
    const int64_t cost=int64_t(n)*100;
    if (cost>g.gold) return {false,cost};
    g.gold=uint16_t(g.gold-cost); g.karma=uint8_t(std::min(int64_t(99),int64_t(g.karma)+n));
    return {true,cost};
}
uint8_t shrine_complete_quest(GameState &g,uint8_t v,CharacterState &a) {
    if (v>=8) return 0;
    constexpr uint8_t attrs[]={4,2,1,6,3,5,7,0};
    g.quest.shrine_quest &= ~(1<<v); g.quest.optional_present |= 1;
    g.karma=uint8_t(std::min(99,int(g.karma)+3));
    if (attrs[v]&1) a.strength=uint8_t(std::min(30,int(a.strength)+1));
    if (attrs[v]&2) a.dexterity=uint8_t(std::min(30,int(a.dexterity)+1));
    if (attrs[v]&4) a.intelligence=uint8_t(std::min(30,int(a.intelligence)+1));
    if (v==7) g.karma=uint8_t(std::min(99,int(g.karma)+3));
    return attrs[v];
}
bool shrine_restore(GameState &g,uint8_t v,TalkText virtue,TalkView<TalkText> mantras,int32_t x,int32_t y,const ShrineData &d) {
    if (v>=d.count || virtue!=d.virtues[v] || mantras.size!=3 || d.x[v]!=x || d.y[v]!=y) return false;
    for (auto m:mantras) if (m!=d.mantras[v]) return false;
    g.quest.shrine_destroyed[v] &= 127; g.quest.destroyed_count=std::max(g.quest.destroyed_count,uint8_t(v+1));
    return true;
}
int32_t shrine_index_at(const ShrineData &d,int32_t x,int32_t y) {
    for (uint8_t i=0;i<d.count && i<8;++i) if (d.x[i]==x && d.y[i]==y) return i;
    return -1;
}
MelodyStep advance_melody(int32_t progress,int32_t digit) {
    constexpr int32_t melody[]={6,7,8,9,8,7,8,7,6,7,6,5,3};
    if (progress>=0 && progress<13 && digit==melody[progress]) return progress==12 ? MelodyStep{0,true} : MelodyStep{progress+1,false};
    if (progress==10 && digit==8) return {3,false};
    if (progress==11 && digit==7) return {2,false};
    return {digit==6 ? 1 : 0,false};
}
TheftResult apply_faulinei_theft(GameState &g,int32_t here,Rand rand) {
    if (here!=0) return {};
    if ((g.keys | g.gems | g.torches)!=0) {
        int32_t *items[]={&g.keys,&g.gems,&g.torches};
        for (;;) { const auto i=rand(0,2); if (*items[i]!=0) {
            *items[i]=*items[i]<=1 ? 0 : *items[i]-1; return {TheftKind(i+1),-1,1};
        } }
    }
    int32_t *arrays[]={g.equipment_quantities,g.potion_quantities,g.scroll_quantities};
    for (int k=0;k<3;++k) for (int i=k==0 ? std::min(47,int(g.equipment_count)-1) : 7;i>=0;--i) if (arrays[k][i]!=0) {
        arrays[k][i]=arrays[k][i]<=1 ? 0 : arrays[k][i]-1;
        return {TheftKind(k+4),i,1};
    }
    const auto roll=rand(1,15); const auto before=g.gold;
    g.gold=uint16_t(g.gold<=roll ? 0 : g.gold-roll);
    return {TheftKind::Gold,-1,before-g.gold};
}
} // namespace openu5
