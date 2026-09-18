#include "openu5/display_names.h"
#include "openu5/magic.h"

#include <cstring>
#include <iostream>

namespace {
int failures=0;
void check(bool ok,const char *what){if(!ok){std::cerr<<"FAIL: "<<what<<'\n';++failures;}}
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
    check(std::strcmp(equipment_display_name(4),"Spiked Helm")==0,"equipment 4 reference parity");
    check(std::strcmp(equipment_display_name(16),"Mystic Armour")==0,"equipment 16 reference parity");
    check(std::strcmp(equipment_display_name(38),"Silver Sword")==0,"equipment 38 reference parity");
    check(std::strcmp(reagent_display_name(7),"Mandrake")==0,"reagent reference parity");
    check(std::strcmp(spell_display_name(47),"An Tym")==0,"spell reference parity");
    check(std::strcmp(spell_effect_summary(SpellId::AnNox),"Cures poison")==0,
          "An Nox description comes from its Cure effect");
    check(std::strcmp(spell_target_label(SpellId::VasFlam),"Direction")==0,
          "Vas Flam target label follows implemented targeting");
    check(is_generic_identifier_label("Equipment 4"),"generic equipment label detected");
    check(is_generic_identifier_label("Location 13"),"generic location label detected");
    check(!is_generic_identifier_label("Iolo's Hut"),"authoritative label accepted");
    check(equipment_display_name(99)==nullptr,"unknown equipment is unresolved, not fabricated");
    std::cout<<"display-name parity and generic-id leakage audit: "
             <<(failures?"FAIL":"PASS")<<'\n';
    return failures?1:0;
}
