#include "openu5/look.h"
#include "openu5/ui_session.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

using namespace openu5;

namespace {
int checks = 0;
void check(bool value) {
    ++checks;
    if (!value) {
        std::cerr << "look sign check " << checks << " failed\n";
        std::exit(1);
    }
}

struct Probe {
    UiSession *ui = nullptr;
    std::vector<std::string> messages;
    std::vector<bool> signs;
    static void emit(void *context, const GameEvent &event) {
        auto &probe = *static_cast<Probe *>(context);
        if (event.kind == GameEventKind::Message) {
            probe.messages.emplace_back(event.text ? event.text : "");
            probe.signs.push_back(event.sign);
        }
        probe.ui->consume(event);
    }
};
}

int main() {
    static const uint8_t iolo_raw[] = {56,108,108,108,57};
    const LookSignRecord records[] = {
        {{0,0},55,66,{"\n           \n BEWARE THE \n DEEP FOREST \n           \n",iolo_raw,sizeof(iolo_raw)}},
        {{0,0},95,148,{" NORTH BRITAIN \n   EAST PAWS    \n SOUTH TRINSIC ",nullptr,0}},
        {{0,255},54,143,{"HEREUPON BEGAN\n  THE QUEST OF  ",nullptr,0}},
        {{1,0},14,20,{" BLACKTHORN'S \nLAW OF HONESTY",nullptr,0}},
        {{17,255},6,13,{" YE ROYAL   \n SHIPWRIGHT ",nullptr,0}},
    };

    auto resolved = resolve_look_sign(records,5,{0,0},55,66);
    check(resolved.text && std::strcmp(resolved.text,records[0].value.text)==0);
    check(resolved.raw==iolo_raw && resolved.raw_size==sizeof(iolo_raw));
    check(std::strcmp(resolve_look_sign(records,5,{0,0},95,148).text,records[1].value.text)==0);
    check(std::strcmp(resolve_look_sign(records,5,{0,255},54,143).text,records[2].value.text)==0);
    check(std::strcmp(resolve_look_sign(records,5,{1,0},14,20).text,records[3].value.text)==0);
    check(std::strcmp(resolve_look_sign(records,5,{17,-1},6,13).text,records[4].value.text)==0);
    check(!resolve_look_sign(records,5,{0,0},54,66).text);

    std::vector<uint8_t> tiles(256U*256U,68);
    tiles[66U*256U+55U]=164; // Actual sign face at (55,66), near Iolo's Hut.
    ActiveMap map{{0,0},MapKind::Overworld,{256,256,true},tiles.data(),-1};
    GameState game{};game.position={{54,66},{0,0}};
    TurnState turn{};TravelState travel{};CommandState commands{};WorldData world{};
    CommandContext context{game,turn,travel,commands,world};
    LookServices services{};services.context=const_cast<LookSignRecord*>(records);
    services.describe=[](void *,int32_t){return "*";};
    services.sign=[](void *owner,MapId id,int32_t x,int32_t y){return resolve_look_sign(static_cast<const LookSignRecord*>(owner),5,id,x,y);};
    context.look=&services;

    UiTextBlock storage[8]{};UiSession ui{{storage,8},{},{21,8,12}};
    Probe probe{&ui};Command command{};command.kind=CommandKind::Look;command.direction=Direction::East;
    check(world_look(context,command,map,{&probe,Probe::emit},rng_source(game.rng))==CommandStatus::Success);

    // TypeScript reference semantics: prefix event, then decoded sign body.
    check(probe.messages.size()==2);
    check(probe.messages[0]=="Thou dost see");
    check(probe.messages[1]==" BEWARE THE \n DEEP FOREST ");
    check(!probe.signs[0] && probe.signs[1]);

    // UiSession and the physical renderer receive normal supported glyphs,
    // rather than LOOK2's sentinel placeholder.
    check(ui.transcript_size()==2);
    check(std::strcmp(ui.transcript_at(0)->text,"Thou dost see")==0);
    check(std::strcmp(ui.transcript_at(1)->text," BEWARE THE \n DEEP FOREST ")==0);
    UiRenderedLine lines[4]{};const auto count=ui.visible_lines(lines,4,21);
    check(count==3);
    check(std::strcmp(lines[0].text,"Thou dost see")==0);
    check(std::strcmp(lines[1].text,"BEWARE THE")==0);
    check(std::strcmp(lines[2].text,"DEEP FOREST")==0);
    for(size_t i=0;i<count;++i)check(std::strchr(lines[i].text,'*')==nullptr);

    std::cout << checks << " Look/sign resource, semantic, and transcript checks passed\n";
}
