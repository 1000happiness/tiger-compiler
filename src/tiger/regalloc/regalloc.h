#ifndef TIGER_REGALLOC_REGALLOC_H_
#define TIGER_REGALLOC_REGALLOC_H_

#include <map>
#include <vector>
#include <stack>

#include "tiger/codegen/assem.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"
#include "tiger/liveness/liveness.h"
#include "tiger/liveness/flowgraph.h"
#include "tiger/regalloc/color.h"

namespace RA {

class Result {
 public:
  TEMP::Map* coloring;
  AS::InstrList* il;

  Result(TEMP::Map* coloring, AS::InstrList* il) : coloring(coloring), il(il) {}
};

Result RegAlloc(F::Frame* f, AS::InstrList* il);

}  // namespace RA

#endif