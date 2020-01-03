#include "tiger/liveness/liveness.h"

namespace LIVE {

void DFSANode(int &index, G::Node<AS::Instr>* node, TAB::Table<G::Node<AS::Instr>, bool> *nodeMark, std::vector<G::Node<AS::Instr> *> *nodeSorted);
void AnalyseLiveTable(TAB::Table<G::Node<AS::Instr>, std::set<TEMP::Temp *>> *liveTable, std::vector<G::Node<AS::Instr> *> *nodeSorted);
void AnalyseLiveGraph(G::Graph<AS::Instr>* flowgraph, TAB::Table<G::Node<AS::Instr>, std::set<TEMP::Temp *>> *liveTable, G::Graph<TEMP::Temp>* graph, MoveList* &moves);
void setUnion(std::set<TEMP::Temp *> * aSet, std::set<TEMP::Temp *> *bSet);
std::set<TEMP::Temp *> * setDifference(std::set<TEMP::Temp *> * aSet, std::set<TEMP::Temp *> *bSet);
bool setSame(std::set<TEMP::Temp *> * aSet, std::set<TEMP::Temp *> *bSet);

LiveGraph *Liveness(G::Graph<AS::Instr>* flowgraph) {
  TAB::Table<G::Node<AS::Instr>, std::set<TEMP::Temp *>> * liveTable = new TAB::Table<G::Node<AS::Instr>, std::set<TEMP::Temp *>>();
  
  //use DFS to parse, get liveTable
  TAB::Table<G::Node<AS::Instr>, bool> *nodeMark = new TAB::Table<G::Node<AS::Instr>, bool>();
  for(auto it = flowgraph->Nodes(); it; it = it->tail){
    nodeMark->Enter(it->head, new bool(false));
  }
  std::vector<G::Node<AS::Instr> *> *nodeSorted = new std::vector<G::Node<AS::Instr> *>(flowgraph->nodecount, nullptr);
  int index = flowgraph->nodecount - 1;//使用引用，保证所有的点的DFS是对同一个index操作
  DFSANode(index, flowgraph->Nodes()->head, nodeMark, nodeSorted);
  AnalyseLiveTable(liveTable, nodeSorted);
  
  //get liveGraph and moveList
  G::Graph<TEMP::Temp>* graph = new G::Graph<TEMP::Temp>();
  MoveList* moves = nullptr;
  AnalyseLiveGraph(flowgraph, liveTable, graph, moves);
  return new LiveGraph(graph, moves);
}

void DFSANode(int &index, G::Node<AS::Instr>* node, TAB::Table<G::Node<AS::Instr>, bool> *nodeMark, std::vector<G::Node<AS::Instr> *> *nodeSorted){
  if(!(*nodeMark->Look(node))){
    nodeMark->Set(node, new bool(true));
    (*nodeSorted)[index] = node;
    index--;//课本上此处的DFS写的有问题，需要先赋值再遍历后续节点
    for(auto it = node->Succ(); it; it = it->tail){
      DFSANode(index, it->head, nodeMark, nodeSorted);
    }
  }
}

void AnalyseLiveTable(TAB::Table<G::Node<AS::Instr>, std::set<TEMP::Temp *>> *outliveTable, std::vector<G::Node<AS::Instr> *> *nodeSorted){
  bool changeFlag = true;
  TAB::Table<G::Node<AS::Instr>, std::set<TEMP::Temp *>> *inliveTable = new TAB::Table<G::Node<AS::Instr>, std::set<TEMP::Temp *>>();
  while (changeFlag)
  {
    changeFlag = false;
    for(auto it = nodeSorted->begin(); it != nodeSorted->end(); it++){
      std::set<TEMP::Temp *> *lastOut = outliveTable->Look(*it);
      std::set<TEMP::Temp *> *in = nullptr;
      std::set<TEMP::Temp *> *def = FG::Def(*it);
      std::set<TEMP::Temp *> *use = FG::Use(*it);
      if(lastOut == nullptr){
        in = new std::set<TEMP::Temp *>();
      }
      else{
        in = setDifference(lastOut, def);//in = lastout - def
      }
      setUnion(in, use);// in = in | use
      std::set<TEMP::Temp *> *lastIn = inliveTable->Look(*it);
      if(lastIn == nullptr){
        changeFlag = true;
        // printf("change Flag in null change\n");
        inliveTable->Enter(*it, in);
      }
      else{
        if(!setSame(in, lastIn)){
          changeFlag = true;
          // printf("change Flag in change\n");
          inliveTable->Set(*it, in);
        }
      }

      std::set<TEMP::Temp *> *out = new std::set<TEMP::Temp *>();
      for(auto succNode = (*it)->Succ(); succNode; succNode = succNode->tail){
        std::set<TEMP::Temp *> *succIn = inliveTable->Look(succNode->head);
        if(succIn != nullptr){
          setUnion(out, succIn);
        }
      }
      if(lastOut == nullptr){
        changeFlag = true;
        // printf("change Flag out  null change\n");
        outliveTable->Enter(*it, out);
      }
      else{
        if(!setSame(out, lastOut)){
          // printf("%d, %d\n", out->size(), lastOut->size());
          // printf("change Flag out change\n");
          changeFlag = true;
          outliveTable->Set(*it, out);
        }
      }
    }
  }
}

void AnalyseLiveGraph(G::Graph<AS::Instr>* flowgraph, TAB::Table<G::Node<AS::Instr>, std::set<TEMP::Temp *>> *liveTable, G::Graph<TEMP::Temp>* graph, MoveList* &moves){
  TAB::Table<TEMP::Temp, G::Node<TEMP::Temp>> * tempInGraphTable = new TAB::Table<TEMP::Temp, G::Node<TEMP::Temp>>();
  //add temp node
  for(auto instrNode = flowgraph->Nodes(); instrNode; instrNode = instrNode->tail){
    std::set<TEMP::Temp *> *def = FG::Def(instrNode->head);
    std::set<TEMP::Temp *> *use = FG::Use(instrNode->head);
    for(auto temp = def->begin(); temp != def->end(); temp++){
      G::Node<TEMP::Temp> *tempNode = tempInGraphTable->Look(*temp);
      if(tempNode == nullptr){
        tempNode = graph->NewNode(*temp);
        tempInGraphTable->Enter(*temp, tempNode);
      }
    }
    for(auto temp = use->begin(); temp != use->end(); temp++){
      G::Node<TEMP::Temp> *tempNode = tempInGraphTable->Look(*temp);
      if(tempNode == nullptr){
        tempNode = graph->NewNode(*temp);
        tempInGraphTable->Enter(*temp, tempNode);
      }
    }
  }


  //add edge
  TAB::Table<TEMP::Temp, std::set<TEMP::Temp *>> *edges = new TAB::Table<TEMP::Temp, std::set<TEMP::Temp *>>();
  for(auto instrNode = flowgraph->Nodes(); instrNode; instrNode = instrNode->tail){
    std::set<TEMP::Temp *> *def = FG::Def(instrNode->head);
    std::set<TEMP::Temp *> *out = liveTable->Look(instrNode->head);
    
    for(auto defTemp = def->begin(); defTemp != def->end(); defTemp++){
      for(auto outTemp = out->begin(); outTemp != out->end(); outTemp++){
        std::set<TEMP::Temp *> *tempSet = edges->Look(*defTemp);
        if(tempSet == nullptr || tempSet->find(*outTemp) == tempSet->end()){
          if(FG::IsMove(instrNode->head)){
            std::set<TEMP::Temp *> *use = FG::Use(instrNode->head);
            if((*outTemp)->Int() != (*defTemp)->Int() && use->find(*outTemp) == use->end()){
              graph->AddEdge(tempInGraphTable->Look(*defTemp), tempInGraphTable->Look(*outTemp));
              if(tempSet == nullptr){
                tempSet = new std::set<TEMP::Temp *>();
                tempSet->insert(*outTemp);
                edges->Set(*defTemp, tempSet);
              }
              else{
                tempSet->insert(*outTemp);
              }
            }
          }
          else{
            if((*outTemp)->Int() != (*defTemp)->Int()){
              graph->AddEdge(tempInGraphTable->Look(*defTemp), tempInGraphTable->Look(*outTemp));
              if(tempSet == nullptr){
                tempSet = new std::set<TEMP::Temp *>();
                tempSet->insert(*outTemp);
                edges->Set(*defTemp, tempSet);
              }
              else{
                tempSet->insert(*outTemp);
              }
            }
          }
        }
      }
    }
  }

  //make move list
  for(auto instrNode = flowgraph->Nodes(); instrNode; instrNode = instrNode->tail){
    if(FG::IsMove(instrNode->head)){
      AS::MoveInstr *moveInstr = (AS::MoveInstr *)instrNode->head->NodeInfo();
      if(moveInstr->assem.find("(") == -1 && moveInstr->assem.find("$") == -1){
        G::Node<TEMP::Temp> *srcTempNode = tempInGraphTable->Look(moveInstr->src->head);
        G::Node<TEMP::Temp> *dstTempNode = tempInGraphTable->Look(moveInstr->dst->head);
        moves = new MoveList(srcTempNode, dstTempNode, moves);
      }
    }
  }
}

void setUnion(std::set<TEMP::Temp *> * aSet, std::set<TEMP::Temp *> *bSet){
  for(auto it = bSet->begin(); it != bSet->end(); it++){
    if(aSet->find(*it) == aSet->end()){
      aSet->insert(*it);
    }
  }
}

std::set<TEMP::Temp *> * setDifference(std::set<TEMP::Temp *> * aSet, std::set<TEMP::Temp *> *bSet){
  std::set<TEMP::Temp *> * returnSet = new std::set<TEMP::Temp *>(aSet->begin(), aSet->end());
  if(bSet->size() != 0){
    for(auto it = returnSet->begin(); it != returnSet->end(); ){
      if(bSet->find(*it) != bSet->end()){
        it = returnSet->erase(it);
      }
      else{
        it++;
      }
    }
  }
  return returnSet;
}

bool setSame(std::set<TEMP::Temp *> * aSet, std::set<TEMP::Temp *> *bSet){
  if(aSet->size() != bSet->size()){
    return false;
  }
  else{
    for(auto it = bSet->begin(); it != bSet->end(); it++){
      if(aSet->find(*it) == aSet->end()){
        return false;
      }
    }
    return true;
  }
}

}  // namespace LIVE