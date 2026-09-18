#include "openu5/ui_session.h"
#if defined(OPENU5_ENABLE_DEVELOPER_TOOLS)
#include "openu5/ui_debug_menu.h"
#endif
#include <iostream>
using namespace openu5;
int main(){
 std::cout<<"UiSession "<<sizeof(UiSession)<<"\nUiTextBlock "<<sizeof(UiTextBlock)<<"\nUiRenderedLine "<<sizeof(UiRenderedLine)<<"\n";
#if defined(OPENU5_ENABLE_DEVELOPER_TOOLS)
 std::cout<<"UiDebugMenu "<<sizeof(UiDebugMenu)<<"\n";
#endif
 for(size_t n:{size_t(8),size_t(16),size_t(32),size_t(64)})std::cout<<"history["<<n<<"] "<<n*sizeof(UiTextBlock)<<"\n";
}
