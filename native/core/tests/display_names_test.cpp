#include "openu5/display_names.h"
#include "openu5/magic.h"
#include "openu5/rng.h"

#include <cstring>
#include <iostream>

namespace {
int failures=0;
void check(bool ok,const char *what){if(!ok){std::cerr<<"FAIL: "<<what<<'\n';++failures;}}
bool has(const char *hay,const char *needle){return hay&&std::strstr(hay,needle)!=nullptr;}
}

int main(){
    using namespace openu5;
    for(int i=0;i<48;++i){
        const char *name=equipment_display_name(i);
        check(name&&*name,"all authoritative equipment ids resolve");
        check(!is_generic_identifier_label(name),"equipment names never expose generic ids");
        const char *spell=spell_display_name(i);
        check(spell&&*spell,"all player spell ids resolve");
        check(!is_generic_identifier_label(spell),"spell names never expose generic ids");
        check(spell_effect_summary(SpellId(i))&&*spell_effect_summary(SpellId(i)),
              "every selectable spell has concise effect metadata");
        check(spell_target_label(SpellId(i))&&*spell_target_label(SpellId(i)),
              "every selectable spell has a player-facing target label");
    }
    for(int i=0;i<8;++i){
        check(reagent_display_name(i)!=nullptr,"reagent id resolves");
        check(potion_display_name(i)!=nullptr,"potion id resolves");
        check(scroll_display_name(i)!=nullptr,"scroll id resolves");
    }
    // R-22 / Batch 14.  These three anchors previously encoded the phantom
    // "BareHands" slot of InventoryDetails.json, which pushed every equipment
    // name one id high.  They now name the ids the binary itself pins:
    // 4 is a shield (equip_type_of == 0x20 and id < 16 -> EquipSlot::Shield),
    // 16 is the Dagger of COMSUBS 0x097c's thrown-weapon set {16, 21, 22},
    // and 38 is the Magic Axe (39 is the Glass Sword whose pack slot
    // SJOG 0x0587 gates the overworld search on).  See src/display_names.cpp.
    check(std::strcmp(equipment_display_name(4),"Small Shield")==0,"equipment 4 reference parity");
    check(std::strcmp(equipment_display_name(16),"Dagger")==0,"equipment 16 reference parity");
    check(std::strcmp(equipment_display_name(38),"Magic Axe")==0,"equipment 38 reference parity");
    check(std::strcmp(equipment_display_name(39),"Glass Sword")==0,"equipment 39 reference parity");
    check(std::strcmp(equipment_display_name(47),"Ankh")==0,"equipment 47 reference parity");
    check(std::strcmp(reagent_display_name(7),"Mandrake")==0,"reagent reference parity");
    check(std::strcmp(spell_display_name(47),"An Tym")==0,"spell reference parity");
    check(std::strcmp(spell_effect_summary(SpellId::AnNox),"Cures poison")==0,
          "An Nox description comes from its Cure effect");
    check(std::strcmp(spell_target_label(SpellId::VasFlam),"Direction")==0,
          "Vas Flam target label follows implemented targeting");

    // Audit R-16 (Batch 8): kSummaries entries confirmed to contradict
    // MagicDefinitions.json::SimpleDescription (and, for In Zu, the RE-derived
    // line-spell mode-1 "dormir"/sleep effect). Semantic substring checks, not
    // literal-string duplication of the table: each pins the corrected meaning
    // in and the old, wrong meaning out.
    check(has(spell_effect_summary(SpellId::AnYlem),"vanish")&&!has(spell_effect_summary(SpellId::AnYlem),"Teleport"),
          "An Ylem summary matches its Poof effect (vanish), not a teleport");
    check(has(spell_effect_summary(SpellId::InWis),"location")&&!has(spell_effect_summary(SpellId::InWis),"gem"),
          "In Wis summary reveals the caster's location, not a gem map");
    check(has(spell_effect_summary(SpellId::InZu),"sleep")&&!has(spell_effect_summary(SpellId::InZu),"invisible"),
          "In Zu summary matches its sleep line-effect, not invisibility");
    check(has(spell_effect_summary(SpellId::QuasAnWis),"harm")&&!has(spell_effect_summary(SpellId::QuasAnWis),"Negat"),
          "Quas An Wis summary charms multiple enemies, not negate magic");
    check(has(spell_effect_summary(SpellId::InAn),"Negat")&&!has(spell_effect_summary(SpellId::InAn),"death"),
          "In An summary negates magic, not death vision");
    check(has(spell_effect_summary(SpellId::SanctLor),"nvisib")&&!has(spell_effect_summary(SpellId::SanctLor),"Protect"),
          "Sanct Lor summary grants invisibility, not a generic protection");
    check(has(spell_effect_summary(SpellId::InNoxHur),"poison")&&!has(spell_effect_summary(SpellId::InNoxHur),"Fear"),
          "In Nox Hur summary blasts foes with poison, not fear");
    check(has(spell_effect_summary(SpellId::InQuasCorp),"fear")&&!has(spell_effect_summary(SpellId::InQuasCorp),"sleep"),
          "In Quas Corp summary causes fear, not sleep");
    check(has(spell_effect_summary(SpellId::AnTym),"time")&&!has(spell_effect_summary(SpellId::AnTym),"Slows"),
          "An Tym summary stops the passage of time, not a slow effect");

    // Audit R-16: the sole confirmed kTargets defect -- An Tym is a noSelection
    // global time-stop, like its World-labelled siblings Quas An Wis/In An, not
    // a direction-consuming spell.
    check(std::strcmp(spell_target_label(SpellId::AnTym),"World")==0,
          "An Tym target label is World, matching its noSelection siblings");

    // Audit R-16: kEffects[26]/[28] adjudicated against RE evidence, not
    // "fixed" to match the audit's guess -- lock the adjudicated values in so a
    // future well-meaning edit doesn't flip them back based on the raw
    // (unreliable) target_type/SimpleDescription text alone.
    {
        GameState g{}; TurnState t{}; auto &p = g.party.characters[0];
        p.current_mp = 30; p.level = 8;
        g.spell_quantities[unsigned(SpellId::InExPor)] = 1;
        OriginalRng orng; orng.seed(1);
        Rand rng{&orng, [](void *ctx, int32_t lo, int32_t hi) -> int32_t {
                     return static_cast<OriginalRng *>(ctx)->next(lo, hi).value; }};
        auto r = cast_spell(g, t, p, SpellId::InExPor, {1, false, -1, 0}, rng);
        check(r.ok && r.effect.kind == MagicEffect::Unlock,
              "In Ex Por is classified Unlock (RE-confirmed door-unlock), not Animation");
    }
    {
        GameState g{}; TurnState t{}; auto &p = g.party.characters[0];
        p.current_mp = 30; p.level = 8;
        g.spell_quantities[unsigned(SpellId::InZu)] = 1;
        OriginalRng orng; orng.seed(1);
        Rand rng{&orng, [](void *ctx, int32_t lo, int32_t hi) -> int32_t {
                     return static_cast<OriginalRng *>(ctx)->next(lo, hi).value; }};
        auto r = cast_spell(g, t, p, SpellId::InZu, {0, true, -1, 0}, rng);
        check(r.ok && r.effect.kind == MagicEffect::Line,
              "In Zu stays classified Line (RE-confirmed lineAoe delivery, sleep is mode 1), not re-typed to a sleep-only kind");
    }

    check(is_generic_identifier_label("Equipment 4"),"generic equipment label detected");
    check(is_generic_identifier_label("Location 13"),"generic location label detected");
    check(!is_generic_identifier_label("Iolo's Hut"),"authoritative label accepted");
    check(equipment_display_name(99)==nullptr,"unknown equipment is unresolved, not fabricated");
    std::cout<<"display-name parity and generic-id leakage audit: "
             <<(failures?"FAIL":"PASS")<<'\n';
    return failures?1:0;
}
