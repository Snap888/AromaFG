#include "vulkan/vulkan_backend.hpp"
#include <cassert>
#include <cmath>
using namespace aromafg::vk;
int main() {
 const auto levels=make_pyramid(1919,1079,4,32);
 assert(levels.size()==4);
 assert(levels[0].width==1919 && levels[0].height==1079);
 assert(levels[1].width==960 && levels[1].height==540);
 assert(levels[2].width==480 && levels[2].height==270);
 assert(levels[3].width==240 && levels[3].height==135);
 assert(make_pyramid(0,100,4).empty());
 assert(make_pyramid(100,100,0).empty());
 for(uint32_t m=2;m<=4;++m) {
   const auto graph=make_flow_schedule(3,m);
   assert(!graph.empty());
   uint32_t synth=0,forward=0,backward=0,up=0;
   float expected=0.0f;
   for(const auto& pass:graph) {
     if(pass.kind==FlowPassKind::Synthesize) {
       ++synth; expected+=1.0f/static_cast<float>(m);
       assert(std::fabs(pass.phase-expected)<1.0e-5f);
     }
     if(pass.kind==FlowPassKind::SearchForward)++forward;
     if(pass.kind==FlowPassKind::SearchBackward)++backward;
     if(pass.kind==FlowPassKind::UpsampleFlow)++up;
   }
   assert(synth==m-1 && forward==3 && backward==3 && up==4);
 }
 assert(make_flow_schedule(0,2).empty());
 assert(make_flow_schedule(3,1).empty());
 assert(make_flow_schedule(3,5).empty());
 return 0;
}
