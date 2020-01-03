#ifndef TIGER_LIVENESS_FLOWGRAPH_H_
#define TIGER_LIVENESS_FLOWGRAPH_H_

#include <set>

#include "tiger/codegen/assem.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"
#include "tiger/util/graph.h"
#include "tiger/util/table.h"

namespace FG {

std::set<TEMP::Temp *>* Def(G::Node<AS::Instr>* n);
std::set<TEMP::Temp *>* Use(G::Node<AS::Instr>* n);

bool IsMove(G::Node<AS::Instr>* n);

G::Graph<AS::Instr>* AssemFlowGraph(AS::InstrList* il);

}  // namespace FG

#endif