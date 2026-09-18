#pragma once
#include "frontend.h"
namespace openu5 {
enum class SystemMenuIntentKind:uint8_t{None,Resume,Save,LoadLatest,LoadSlot,PersistSettings,OpenDeveloper,ReturnToTitle};
struct SystemMenuIntent{SystemMenuIntentKind kind=SystemMenuIntentKind::None;int8_t slot=-1;FrontendSettings settings{};};
class SystemMenuSession{
public:
 void open(const FrontendSettings&,const FrontendSaveSlot(&)[2]);bool active()const{return active_;}bool handle(const UiAction&);FrontendView view()const;SystemMenuIntent take_intent();void close(){active_=false;page_=Page::Root;}
 const FrontendSettings& settings()const{return settings_;}
private:
 enum class Page:uint8_t{Root,Load,Settings};bool active_=false;Page page_=Page::Root;uint8_t cursor_=0,settings_cursor_=0;FrontendSettings settings_{};FrontendSaveSlot saves_[2]{};SystemMenuIntent pending_{};
};
}
