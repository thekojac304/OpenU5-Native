#include "openu5/dungeon_encounters.h"
#include "openu5/dungeon.h"
namespace openu5 {
CombatResult dungeon_encounter(CommandContext &c,int32_t room_map,bool attack){
    if(!c.dungeon_context||!c.dungeon_context->encounters||c.combat)return CombatResult::Invalid;
    auto &d=c.dungeon_context->state;auto &s=*c.dungeon_context->encounters;if(!s.combat||!d.active||&s.combat->game!=&c.game||&s.combat->turn!=&c.turn)return CombatResult::Invalid;
    const CombatMap *map=nullptr;const uint8_t *sprites=nullptr;
    if(room_map>=0){
        if(s.count&&!s.arenas)return CombatResult::Invalid;
        for(size_t i=0;i<s.count;++i)if(s.arenas[i].map&&s.arenas[i].map->index==room_map){map=s.arenas[i].map;sprites=s.arenas[i].sprites;break;}
        if(!map||(map->unit_count&&!sprites))return CombatResult::MissingMap;
    }else{
        auto &b=*s.combat;int type=d.wanderer.type;if(!b.enemy_defs||size_t(type)>=b.enemy_def_count||!b.enemy_defs[type])return CombatResult::Invalid;
        build_corridor_map(c.game,d,b.enemy_defs[type]->max_per_map,s.corridor,s.sprites);map=&s.corridor;sprites=s.sprites;
    }
    FixedCombatSetup fixed{sprites,d.pos.floor,s.fields,16};
    return start_fixed_combat(c,*s.combat,*map,fixed,dungeon_room_entry(*map,uint8_t(d.pos.facing)),room_map>=0,room_map>=0?-1:attack?1:0);
}
}
