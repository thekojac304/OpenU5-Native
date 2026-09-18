#include "openu5/frontend_settings.h"
#include "openu5/persistence.h"
#include <cmath>

namespace openu5 {
save::Json settings_document(const FrontendSettings&s){using J=save::Json;J j=J::object();j["version"]=J(int(s.version));j["brightness"]=J(int(s.brightness));j["movementMode"]=J(s.movement_mode);j["trackballResponsiveness"]=J(int(s.trackball_responsiveness));j["uiSize"]=J(int(s.ui_size));j["developerToolsVisible"]=J(s.developer_tools_visible);j["soundVolume"]=J(int(s.sound_volume));j["musicVolume"]=J(int(s.music_volume));j["touchControls"]=J(s.touch_controls);return j;}
bool settings_from_document(const save::Json&j,FrontendSettings&s){using J=save::Json;auto integer=[&](const char*k,int lo,int hi,int&out){const auto&v=j[k];if(v.kind!=J::Number||!std::isfinite(v.number)||std::floor(v.number)!=v.number||v.number<lo||v.number>hi)return false;out=int(v.number);return true;};int version=0,b=0,t=0,u=0,sv=0,mv=0;if(j.kind!=J::Object||!integer("version",1,1,version)||!integer("brightness",10,100,b)||!integer("trackballResponsiveness",0,300,t)||!integer("uiSize",0,2,u)||!integer("soundVolume",0,100,sv)||!integer("musicVolume",0,100,mv))return false;for(auto k:{"movementMode","developerToolsVisible","touchControls"})if(j[k].kind!=J::Bool)return false;
// Migrate the three Alpha preset values without invalidating an existing card.
// New documents use exact percentages in 25% increments.
if(t<=2)t=t==0?50:t==1?100:200;else if(t<25||t%25!=0)return false;
s.version=uint8_t(version);s.brightness=uint8_t(b);s.trackball_responsiveness=uint16_t(t);s.ui_size=uint8_t(u);s.sound_volume=uint8_t(sv);s.music_volume=uint8_t(mv);s.movement_mode=j["movementMode"].truth();s.developer_tools_visible=j["developerToolsVisible"].truth();s.touch_controls=j["touchControls"].truth();return true;}
bool encode_settings(const FrontendSettings&s,std::string&out){return save::encode_json(settings_document(s),out)==save::JsonError::None;}
bool decode_settings(const std::string&text,FrontendSettings&s){save::Json j;if(save::parse_json(text,j)!=save::JsonError::None)return false;FrontendSettings parsed{};if(!settings_from_document(j,parsed))return false;s=parsed;return true;}
}
