#include "openu5/dungeon.h"
#include "openu5/world_commands.h"
#include "openu5/magic.h"
#include "openu5/display_names.h"
#include "openu5/outdoor.h"
#include <algorithm>
#include <cstdio>
namespace openu5 {
WorldCommandResult world_magic(CommandContext &c,Command cmd,const ActiveMap &map,EventSink sink,Rand rand){
    auto &g=c.game;auto &t=c.turn;const int location=c.dungeon&&c.dungeon_context?c.dungeon_context->state.pos.dungeon:g.position.map.location;
    auto emit=[&](GameEventKind k,const char *s=nullptr){GameEvent e;e.kind=k;e.text=s;if(k==GameEventKind::MapReveal)e.note=20;if(sink.emit)sink.emit(sink.context,e);};
    auto say=[&](const char *s){if(s&&*s)emit(GameEventKind::Message,s);};
    auto ceremony=[&](int index,const char *audio){emit(GameEventKind::Sfx,audio);GameEvent e;e.kind=GameEventKind::MagicCeremony;e.note=index;if(sink.emit)sink.emit(sink.context,e);};
    auto target=[&]()->CharacterState *{return cmd.member>=0&&cmd.member<g.party.character_count?&g.party.characters[cmd.member]:nullptr;};
    if(cmd.kind==CommandKind::UseItem){
        if(cmd.item>=16){
            if(cmd.item==16||cmd.item==32){say(cmd.item==16?"Carpet":"Spyglass");say("Not here!");return {};}
            if(cmd.item==17){if(!c.combat||g.skull_keys>0)--g.skull_keys;say("Skull Key");if(!c.combat)say("Not here!");return {};}
            if(cmd.item>=21&&cmd.item<=28){if(c.combat){say("Moonstone");say("cannot be buried here!");return {};}return {use_moonstone(c,cmd.item-21,sink)};}
            if(cmd.item==34){say("Sextant");if(c.combat||g.position.map.location||g.position.map.floor>127)say("Only outdoors!");else if(g.time.hour>5&&g.time.hour<19)say("Only at night!");else{char text[48];std::snprintf(text,sizeof(text),"Position: %u, %u",unsigned(g.position.xy.x),unsigned(g.position.xy.y));say(text);}return {};}
            if(cmd.item==35){say("Watch");char text[64];std::snprintf(text,sizeof(text),"The pocket watch reads %d:%02d %s.",int(g.time.hour%12?g.time.hour%12:12),int(g.time.minute),g.time.hour<=11?"AM":"PM");say(text);return {};}
            if(cmd.item==37){say("Box");say("How?");return {};}
            return {use_quest_item(c,cmd.item,sink)};
        }
        if(cmd.item>=8&&cmd.item<16){consume_potion(g,cmd.item-8);say("Potion");if(auto *p=target()){ceremony(cmd.item-8,"potion-used");auto effect=apply_potion_effect(*p,reroll_potion_color(cmd.item-8,rand),rand,location);say(effect.result.message);if(effect.reveal)emit(GameEventKind::MapReveal);}return {};}
        if(cmd.item<0||cmd.item>7)return {CommandStatus::Unsupported};
        if(g.scroll_quantities[cmd.item]>0){--g.scroll_quantities[cmd.item];}say("Scroll");
        switch(cmd.item){
        case 0:t.light_spell_minutes=240;say("Light!");break;
        case 1:say("Wind change!");if(cmd.has_direction&&location<33){t.wind=int(cmd.direction)+1;t.wind_drift_counter=0;}break;
        case 2:t.time_spell='P';t.spell_turns=100;say("Protection!");break;
        case 3:t.time_spell='N';t.spell_turns=20;say("Negate magic!");break;
        case 4:say("View!");if(location>127)say("Not here!");else emit(GameEventKind::MapReveal);break;
        case 5:say("Summon Daemon!");say("Not here!");break;
        case 6:say("Resurrection!");if(auto *p=target())apply_target_spell(*p,MagicEffect::Resurrect,g.karma,rand);break;
        case 7:if(location==29||location==40)say("No effect!");else{t.time_spell='T';t.spell_turns=20;say("Negate time!");}break;
        }static constexpr int8_t indices[8]={0,-1,2,3,4,-1,-1,7};if(indices[cmd.item]>=0&&!(cmd.item==4&&location>127)&&!(cmd.item==7&&(location==29||location==40)))ceremony(indices[cmd.item],"scroll-used");return {};
    }
    if(cmd.item<0||cmd.item>48||cmd.caster<0||cmd.caster>=g.party.character_count)return {CommandStatus::InvalidContext};
    auto *q=c.quest_world;
    // These effects require mutable terrain; reject absent owners before spending resources.
    if((cmd.item==6||cmd.item==25||cmd.item==26)&&cmd.has_direction&&(!q||!q->volatile_tile))return {CommandStatus::InvalidContext};
    if(cmd.item==17&&cmd.has_direction&&!c.outdoor)return {CommandStatus::InvalidContext};
    if(cmd.item==46&&(!q||(q->moonstone_count&&!q->moonstones)))return {CommandStatus::InvalidContext};
    auto cast=cast_spell(g,t,g.party.characters[cmd.caster],SpellId(cmd.item),{g.position.map.location,false,-1,0},rand);
    say(cast.message);if(!cast.ok){if(cast.consumed)say("Failed!");emit(GameEventKind::Sfx,"invalid-magic");return {};}
    static constexpr int no_ceremony[]={1,13,37,28,40,44,45};bool ceremonial=cmd.item!=46;for(int id:no_ceremony)ceremonial&=cmd.item!=id;if(ceremonial)ceremony(spell_definition(SpellId(cmd.item))->circle,"spell-cast");
    auto fx=cast.effect.kind;
    if(fx==MagicEffect::Gate){
        // Y-33. CAST.OVL 0x0d2d pushes the literal ceremony index 8 into
        // CAST2:0x0000 only AFTER the phase-gate ('1'-'8') has already
        // passed; the ship check (0x0cf6) gates even earlier, before the
        // phase is ever read, so it is checked here too rather than trusted
        // to the UI alone (a caller that supplies cmd.hours directly, like
        // the parity fixture, must get the same answer). Any abort path
        // (ship, bad key, Cancel) leaves cmd.hours at the -1 sentinel the UI
        // arms it with. The gate flag stays unconditional either way, so
        // commands.cpp's existing phase<0/ship/bounds check still owns
        // "Failed!" and the actual moonstone_teleport call.
        const bool aboard_ship=(c.turn.transport_tile&240)==32;
        if(cmd.hours>=0&&cmd.hours<=7&&!aboard_ship){static constexpr int kVasRelPorPhaseCeremonyIndex=8;ceremony(kVasRelPorPhaseCeremonyIndex,"spell-cast");}
        return {CommandStatus::Success,false,false,false,true};
    }
    if(fx==MagicEffect::Mani||fx==MagicEffect::FullHeal||fx==MagicEffect::Cure||fx==MagicEffect::Awaken||fx==MagicEffect::Resurrect){if(auto *p=target())say(apply_target_spell(*p,fx,g.karma,rand)?"Success!":"Failed!");return {};}
    if(fx==MagicEffect::DeathVision){emit(GameEventKind::MapReveal);return {};}
    if(fx==MagicEffect::Peer){char text[32];std::snprintf(text,sizeof(text),"\n%c'%c\", %c'%c\"\n",'A'+(g.position.xy.y>>4),'A'+(g.position.xy.y&15),'A'+(g.position.xy.x>>4),'A'+(g.position.xy.x&15));say(text);return {};}
    if(fx==MagicEffect::Seal||fx==MagicEffect::Disarm){
        if(!cmd.has_direction){say("Cancelled.");return {};}
        auto d=direction_delta(cmd.direction);int x=g.position.xy.x+d.dx,y=g.position.xy.y+d.dy;if(map.geometry.wraps){x&=255;y&=255;}
        int tile=q->tile_at?q->tile_at(q->context,x,y):map.tile_at(x,y),next=-1;
        if(fx==MagicEffect::Seal)next=tile==184||tile==185?151:tile==186||tile==187?152:-1;
        else next=tile==185||tile==187?tile-1:-1;
        if(next>=0){q->volatile_tile(q->context,x,y,next);say(fx==MagicEffect::Seal?"Locked!":"Success!");emit(GameEventKind::MapChanged);return {};}
        if(fx==MagicEffect::Disarm&&q->read&&q->count&&q->write){
            std::vector<PoolEntity> enemies,objects; if(c.outdoor)for(auto &e:c.outdoor->enemies)enemies.push_back({e.slot,e.tile,e.x,e.y,g.position.map.floor,0});
            for(size_t i=0;i<q->count(q->context);++i){auto o=q->read(q->context,i);objects.push_back({o.slot,o.tile,o.x,o.y,o.floor,o.location});}
            auto pool=compose_world_pool(g.position.map.location,g.position.map.floor,enemies.data(),enemies.size(),objects.data(),objects.size());
            if(c.outdoor)for(size_t i=0;i<enemies.size();++i)c.outdoor->enemies[i].slot=enemies[i].slot;
            for(size_t i=0;i<objects.size();++i){auto o=q->read(q->context,i);o.slot=objects[i].slot;q->write(q->context,i,o);}
            for(auto &s:pool)if(s.tile==1&&s.x==x&&s.y==y&&s.floor==g.position.map.floor&&s.kind==PoolOwnerKind::Object){auto o=q->read(q->context,s.owner_index);o.contents&=127;o.trapped=false;q->write(q->context,s.owner_index,o);say("Success!");return {};}
        }say(fx==MagicEffect::Seal?"No effect!":"Failed!");return {};
    }
    if(fx==MagicEffect::Unlock){
        if(!cmd.has_direction){say("Cancelled.");return {};}
        auto d=direction_delta(cmd.direction);int x=g.position.xy.x+d.dx,y=g.position.xy.y+d.dy;if(map.geometry.wraps){x&=255;y&=255;}
        int tile=q->tile_at?q->tile_at(q->context,x,y):map.tile_at(x,y);
        int next=tile==151?184:tile==152?186:-1;
        if(next>=0){q->volatile_tile(q->context,x,y,next);say("Success!");emit(GameEventKind::MapChanged);return {};}
        say("No effect!");return {};
    }
    if(fx==MagicEffect::Blink&&cmd.has_direction){auto &o=*c.outdoor;auto origin=[](int p){return (((p&240)-((p&15)<8?16:0))&255);};int px=g.position.xy.x,py=g.position.xy.y;bool fresh=o.has_chunk_origin&&((px-o.chunk_x)&255)<32&&((py-o.chunk_y)&255)<32;int ox=fresh?o.chunk_x:origin(px),oy=fresh?o.chunk_y:origin(py);auto d=direction_delta(cmd.direction);int dx=-1,dy=-1;
        for(int x=px+d.dx,y=py+d.dy;x>=ox&&x<std::min(ox+32,256)&&y>=oy&&y<std::min(oy+32,256);x+=d.dx,y+=d.dy)if(map.tile_at(x,y)==5){dx=x;dy=y;}
        if(dx>=0){g.position.xy={uint8_t(dx),uint8_t(dy)};o.has_chunk_origin=true;o.chunk_x=origin(dx);o.chunk_y=origin(dy);emit(GameEventKind::MapChanged);return {CommandStatus::Success,true};}
    }
    return {};
}
}
