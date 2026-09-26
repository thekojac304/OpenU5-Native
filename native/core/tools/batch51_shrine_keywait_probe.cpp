// Batch 51 audit probe: what the device session does with the shrine rite's
// getkey marker (ShrineKeyWait) when no Blackthorn scene owns it.
#include "openu5/ui_session.h"
#include <array>
#include <cstdio>
#include <cstring>
using namespace openu5;
int main(){
    std::array<UiTextBlock,64> blocks{};
    UiSession ui({blocks.data(),blocks.size()});
    ui.set_base_mode(UiMode::Exploration);
    GameEvent a{};a.kind=GameEventKind::Message;a.text="The Altar speaks and a Quest is ordained! ";
    GameEvent w{};w.kind=GameEventKind::ShrineKeyWait;
    GameEvent b{};b.kind=GameEventKind::Message;b.text="'Tis now thy sacred Quest";
    ui.consume(a);ui.consume(w);ui.consume(b);
    bool both=false,first=false;
    for(size_t i=0;i<ui.transcript_size();++i){auto*t=ui.transcript_at(i);if(t&&std::strstr(t->text,"ordained"))first=true;if(t&&std::strstr(t->text,"sacred Quest"))both=first;}
    std::printf("mode_after_ShrineKeyWait=%d (Exploration=%d KeyWait=%d) text_after_wait_already_in_transcript=%d\n",
                int(ui.mode()),int(UiMode::Exploration),int(UiMode::KeyWait),int(both));
    return 0;
}
