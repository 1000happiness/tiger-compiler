#include "tiger/liveness/flowgraph.h"

namespace FG {

S::Table<G::Node<AS::Instr>>* labelTable = nullptr;
TAB::Table<AS::Instr, G::Node<AS::Instr>>* jumpTable = nullptr;
TAB::Table<G::Node<AS::Instr>, std::set<TEMP::Temp *>> *useTable = nullptr;
TAB::Table<G::Node<AS::Instr>, std::set<TEMP::Temp *>> *defTable = nullptr;

std::set<TEMP::Temp *>* Def(G::Node<AS::Instr>* n) {
  std::set<TEMP::Temp *> *defTemps = defTable->Look(n);
  if(defTemps == nullptr){
    defTemps = new std::set<TEMP::Temp *>();
    AS::Instr *instr = n->NodeInfo();
    switch (instr->kind)
    {
    case AS::Instr::LABEL:{
      return defTemps;
    } break;
    case AS::Instr::MOVE:{
      for(auto it = ((AS::MoveInstr *)instr)->dst; it; it = it->tail){
        defTemps->insert(it->head);
      }
      defTable->Enter(n, defTemps);
      return defTemps;
    } break;
    case AS::Instr::OPER:{
      for(auto it = ((AS::OperInstr *)instr)->dst; it; it = it->tail){
        defTemps->insert(it->head);
      }
      defTable->Enter(n, defTemps);
      return defTemps;
    } break;
    default:
      break;
    }
  }
  else{
    return defTemps;
  }
}

std::set<TEMP::Temp *>* Use(G::Node<AS::Instr>* n) {
  std::set<TEMP::Temp *> *useTemps = useTable->Look(n);
  if(useTemps == nullptr){
    useTemps = new std::set<TEMP::Temp *>();
    AS::Instr *instr = n->NodeInfo();
    switch (instr->kind)
    {
    case AS::Instr::LABEL:{
      return useTemps;
    } break;
    case AS::Instr::MOVE:{ 
      
      for(auto it = ((AS::MoveInstr *)instr)->src; it; it = it->tail){
        useTemps->insert(it->head);
      }
      useTable->Enter(n, useTemps);
      return useTemps;
    } break;
    case AS::Instr::OPER:{
      for(auto it = ((AS::OperInstr *)instr)->src; it; it = it->tail){
        useTemps->insert(it->head);
      }
      useTable->Enter(n, useTemps);
      return useTemps;
    } break;
    default:
      break;
    }
  }
  else{
    return useTemps;
  }
}

bool IsMove(G::Node<AS::Instr>* n) {
  AS::Instr *instr = n->NodeInfo();
  switch (instr->kind)
  {
  case AS::Instr::LABEL:{
    return false;
  } break;
  case AS::Instr::MOVE:{ 
    AS::MoveInstr *moveInstr = (AS::MoveInstr *)instr;
    if(moveInstr->assem.find("(") == -1 && moveInstr->assem.find("$") == -1){// mov regB, regA
      return true;
    }
    return false;
  } break;
  case AS::Instr::OPER:{//the first isntr is not jmp or cjump
    return false;
  } break;
  default:
    break;
  }
  return false;
}

G::Graph<AS::Instr>* AssemFlowGraph(AS::InstrList* il) {
  G::Graph<AS::Instr>* flowgraph = new G::Graph<AS::Instr>();
  labelTable = new S::Table<G::Node<AS::Instr>>();
  jumpTable = new TAB::Table<AS::Instr, G::Node<AS::Instr>>();
  defTable = new TAB::Table<G::Node<AS::Instr>, std::set<TEMP::Temp *>>();
  useTable = new TAB::Table<G::Node<AS::Instr>, std::set<TEMP::Temp *>>();

  G::Node<AS::Instr> *preNode = nullptr;
  AS::Instr *instr = nullptr;

  for(auto it = il; it; it = it->tail){ //start at second instr
    if(preNode == nullptr){
      preNode = flowgraph->NewNode(it->head);//the first instr
      instr = it->head;
      switch (instr->kind)
      {
      case AS::Instr::LABEL:{
        labelTable->Enter(((AS::LabelInstr *)instr)->label, preNode);
      } break;
      case AS::Instr::MOVE:{ 
      } break;
      case AS::Instr::OPER:{//the first isntr is not jmp or cjump
      } break;
      default:
        break;
      }

      it = it->tail;
      if(it == nullptr){
        break;
      }
    }
    G::Node<AS::Instr> *nowNode = flowgraph->NewNode(it->head);
    instr= it->head;
    switch (instr->kind)
    {
    case AS::Instr::LABEL:{
      labelTable->Enter(((AS::LabelInstr *)instr)->label, nowNode);
      flowgraph->AddEdge(preNode, nowNode);
      preNode = nowNode;
    } break;
    case AS::Instr::MOVE:{ 
      flowgraph->AddEdge(preNode, nowNode);
      preNode = nowNode;
    } break;
    case AS::Instr::OPER:{ // skip jump target
      if(((AS::OperInstr *)instr)->assem.find("j") == 0){
        jumpTable->Enter(instr, nowNode);
        flowgraph->AddEdge(preNode, nowNode);
        preNode = nullptr;
      }
      else{
        flowgraph->AddEdge(preNode, nowNode);
        preNode = nowNode;
      }
    } break;
    default:
      break;
    }
  }

  //finlish cjump
  for(auto it = il->tail; it; it = it->tail){ //start at second instr
    instr= it->head;
    if(instr->kind == AS::Instr::OPER){
      if(((AS::OperInstr *)instr)->assem.find("j") == 0){
        G::Node<AS::Instr> *nowNode = jumpTable->Look(instr);
        for(auto labelPtr = ((AS::OperInstr *)instr)->jumps->labels; labelPtr; labelPtr = labelPtr->tail){
          flowgraph->AddEdge(nowNode, labelTable->Look(labelPtr->head));
        }
      }
    }
  }

  return flowgraph;
}

}  // namespace FG
