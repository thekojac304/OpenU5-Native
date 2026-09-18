#pragma once
#include "openu5/combat.h"
#include "openu5/persistence.h"
#include <vector>
#include <string>
// Borrowed, authoritative JSON assets supplied by the TypeScript harness.
struct QuestCombatFixture {
    std::vector<openu5::CombatMap> maps;
    std::vector<const openu5::CombatMap *> map_ptrs;
    std::vector<openu5::CombatEnemy> enemies;
    std::vector<const openu5::CombatEnemy *> enemy_ptrs;
    std::vector<std::string> names,groups;
    std::vector<int32_t> attack,range,defense,strength_range;
    openu5::CombatResources resources;
    static std::string text(const openu5::save::Json &j){std::string s;for(auto c:j.string)s+=char(c);return s;}
    void load(const openu5::save::Json &q){
        using namespace openu5;
        for(const auto &m:q["combatMaps"].values){
            CombatMap map;map.index=int32_t(m["index"].integer());int cell=0;
            for(const auto &row:m["tiles"].values)for(const auto &v:row.values)map.tiles[cell++]=int16_t(v.integer());
            const char *dirs[]={"east","west","south","north"};
            for(int d=0;d<4;++d){auto &starts=m["playerStarts"][dirs[d]];map.start_count[d]=uint8_t(starts.values.size());for(size_t i=0;i<starts.values.size();++i)map.starts[d][i]={int16_t(starts.at(i)["x"].integer()),int16_t(starts.at(i)["y"].integer())};}
            map.unit_count=uint8_t(m["units"].values.size());for(size_t i=0;i<m["units"].values.size();++i)map.units[i]={int16_t(m["units"].at(i)["x"].integer()),int16_t(m["units"].at(i)["y"].integer())};
            map.trigger_count=uint8_t(m["triggers"].values.size());for(size_t i=0;i<m["triggers"].values.size();++i){auto &t=m["triggers"].at(i);auto point=[&](const char *k){return CombatPoint{int16_t(t[k]["x"].integer()),int16_t(t[k]["y"].integer())};};map.triggers[i]={int16_t(t["sprite"].integer()),point("at"),point("pos1"),point("pos2")};}
            maps.push_back(map);
        }
        map_ptrs.resize(128);for(auto &m:maps)map_ptrs[m.index]=&m;
        for(const auto &e:q["enemyDefs"].values){
            CombatEnemy d;auto n=[&](const char *k){return int32_t(e[k].integer());};
            d.index=n("index");d.strength=n("str");d.dexterity=n("dex");d.intelligence=n("int");d.armor=n("armour");d.damage=n("damage");d.hp=n("hp");d.max_per_map=n("maxPerMap");d.treasure=n("treasure");d.range=n("attackRange");d.abilities=uint16_t(n("nativeMask"));d.move_class=uint8_t(n("moveClass"));d.stationary=e["doesNotMove"].truth();d.tile=int16_t(n("tile"));
            enemies.push_back(d);names.push_back(text(e["name"]));groups.push_back(text(e["groupName"]));
        }
        for(size_t i=0;i<enemies.size();++i){enemies[i].name=names[i].c_str();enemies[i].group_name=groups[i].c_str();enemy_ptrs.push_back(&enemies[i]);}
        auto table=[](const save::Json &j,std::vector<int32_t> &v){for(auto &x:j.values)v.push_back(int32_t(x.integer()));};
        table(q["attackValues"],attack);table(q["attackRangeValues"],range);table(q["defenseValues"],defense);table(q["spellAttackRange"],strength_range);
        resources.maps=map_ptrs.data();resources.map_count=map_ptrs.size();resources.enemies=enemy_ptrs.data();resources.enemy_count=enemy_ptrs.size();resources.tables={attack.data(),range.data(),defense.data(),strength_range.data(),attack.size()};
    }
};
