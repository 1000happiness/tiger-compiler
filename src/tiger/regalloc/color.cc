#include "tiger/regalloc/color.h"

namespace COL {

enum moveType {
  workList,
  active,
};

TEMP::Map* coloring = nullptr;
TEMP::TempList* spills = nullptr;
G::Graph<TEMP::Temp> *originGraph = nullptr;
G::Graph<TEMP::Temp> *adjList = nullptr;
TEMP::Map *frameTempMap = nullptr;
LIVE::MoveList *moveList = nullptr;
TEMP::TempList *regsToColor = nullptr;
int regNumber = 0;
std::set<G::Node<TEMP::Temp> *> *initialNodes = nullptr;
std::map<G::Node<TEMP::Temp> *, int> *adjSetXIndex = nullptr;
std::map<G::Node<TEMP::Temp> *, int> *adjSetYIndex = nullptr;
std::vector<std::vector<bool> *> *adjSet = nullptr;
std::map<G::Node<TEMP::Temp> *, std::map<G::Node<TEMP::Temp> *, moveType> *> *moveNodeMap = nullptr;
std::map<G::Node<TEMP::Temp>*, G::Node<TEMP::Temp>*> *old2newNodeMap = nullptr;
std::map<G::Node<TEMP::Temp>*, G::Node<TEMP::Temp>*> *new2oldNodeMap = nullptr;
std::vector<int> *moveTypeNumber = nullptr; 
std::set<G::Node<TEMP::Temp> *> *spillWorkList = nullptr;
std::set<G::Node<TEMP::Temp> *> *moveRelatedList = nullptr;
std::set<G::Node<TEMP::Temp> *> *simplifyWorkList = nullptr;
std::stack<G::Node<TEMP::Temp> *> *selectStack = nullptr;
std::map<G::Node<TEMP::Temp> *, G::Node<TEMP::Temp> *> *coalescedChildNodes = nullptr;
std::map<G::Node<TEMP::Temp> *, std::set<G::Node<TEMP::Temp> *> *> *coalesceParentNodes = nullptr;
std::set<G::Node<TEMP::Temp> *> *spilledNodes = nullptr;
std::set<G::Node<TEMP::Temp> *> *fronzeNodes = nullptr;
std::map<G::Node<TEMP::Temp> *, std::set<G::Node<TEMP::Temp> *> *> *deleteNodesMap = nullptr;

void init();
void build();
void makeWorkList();
bool isWorkListEmpty();
void simplify();
void coalesce();
void freeze();
void selectSpill();
void assignColors();
G::Node<TEMP::Temp> * actualNode(G::Node<TEMP::Temp> * node);
void deleteNode(G::Node<TEMP::Temp> *centerNode);
bool briggs(G::Node<TEMP::Temp> *aNode, G::Node<TEMP::Temp> *bNode);
bool george(G::Node<TEMP::Temp> *aNode, G::Node<TEMP::Temp> *bNode);// not use
void coalesceNode(G::Node<TEMP::Temp> *src, G::Node<TEMP::Temp> *dst);
void freezeNode(G::Node<TEMP::Temp> *freezedNode);
void freezeMove(G::Node<TEMP::Temp> *src, G::Node<TEMP::Temp> *dst);

Result *Color(G::Graph<TEMP::Temp> *ig, TEMP::Map *initial, TEMP::TempList *regs,
             LIVE::MoveList* moves) {
  //init all var
  coloring = initial;
  spills = nullptr;
  originGraph = ig;
  frameTempMap = initial;
  regsToColor = regs;
  moveList = moves;
  init();

  build();

  makeWorkList();

  // printf("makeWorkList Sim:%d M:%d Spill:%d Move %d\n", simplifyWorkList->size(), moveRelatedList->size(), spillWorkList->size(), (*moveTypeNumber)[moveType::workList]);
  

  while(!isWorkListEmpty()){
    if(!simplifyWorkList->empty()){
      // printf("begin simplify %d\n", simplifyWorkList->size());
      
      simplify();
      // printf("end simplify %d\n", simplifyWorkList->size());
      
    }
    else if((*moveTypeNumber)[moveType::workList] != 0){
      // printf("begin coalessce worklist %d active %d relate %d\n", (*moveTypeNumber)[moveType::workList], (*moveTypeNumber)[moveType::active], moveRelatedList->size());
      
      coalesce();
      // printf("end coalesce worklist %d active %d relate %d\n", (*moveTypeNumber)[moveType::workList], (*moveTypeNumber)[moveType::active], moveRelatedList->size());
      
    }
    else if(!moveRelatedList->empty()){
      // printf("begin freeze %d\n", moveRelatedList->size());
      
      freeze();
      // printf("end freeze %d\n", moveRelatedList->size());
      
    }
    else if(!spillWorkList->empty()){
      // printf("begin spill %d\n", spillWorkList->size());
      
      selectSpill();
      // printf("end spill %d\n", spillWorkList->size());
      
    }
  }
  // printf("begin assignColors stack %d\n", selectStack->size());
  

  assignColors();
  // printf("end assignColors stack %d\n", selectStack->size());
  

  return new Result(coloring, spills);
}

void init(){
  adjList = new G::Graph<TEMP::Temp>();//deepcopy originGraph
  old2newNodeMap = new std::map<G::Node<TEMP::Temp>*, G::Node<TEMP::Temp>*>();
  new2oldNodeMap = new std::map<G::Node<TEMP::Temp>*, G::Node<TEMP::Temp>*>();
  std::map<TEMP::Temp*, G::Node<TEMP::Temp>*> *regNodeMap = new std::map<TEMP::Temp*, G::Node<TEMP::Temp>*>();
  for(auto node = originGraph->Nodes(); node; node = node->tail){
    G::Node<TEMP::Temp> *newNode = adjList->NewNode(node->head->NodeInfo());
    // printf("add node %d\n", node->head->NodeInfo()->Int());
    old2newNodeMap->insert(std::make_pair(node->head, newNode));
    new2oldNodeMap->insert(std::make_pair(newNode, node->head));
    if(frameTempMap->Look(newNode->NodeInfo()) != nullptr){
      regNodeMap->insert(std::make_pair(newNode->NodeInfo(), newNode));
    }
  }
  for(auto temp = regsToColor; temp; temp = temp->tail){
    if(regNodeMap->find(temp->head) == regNodeMap->end()){
      G::Node<TEMP::Temp> *newNode = adjList->NewNode(temp->head);
      regNodeMap->insert(std::make_pair(temp->head, newNode));
    }
  }
  G::NodeList<TEMP::Temp> *copyNode = adjList->Nodes();
  G::NodeList<TEMP::Temp> *originNode = originGraph->Nodes();
  for(; copyNode && originNode; copyNode = copyNode->tail, originNode = originNode->tail){
    std::set<TEMP::Temp *> *tempSet = new std::set<TEMP::Temp *>();
    // printf("%d:", copyNode->head->NodeInfo()->Int());
    
    for(auto pred = originNode->head->Pred(); pred; pred = pred->tail){
      if(tempSet->find(pred->head->NodeInfo()) == tempSet->end()){
        // printf("%d ",pred->head->NodeInfo()->Int());
        adjList->AddEdge((*old2newNodeMap)[pred->head], copyNode->head);
        tempSet->insert(pred->head->NodeInfo());
      }
    }
    for(auto succ = originNode->head->Succ(); succ; succ = succ->tail){
      if(tempSet->find(succ->head->NodeInfo()) == tempSet->end()){
        // printf("%d ",succ->head->NodeInfo()->Int());
        adjList->AddEdge((*old2newNodeMap)[succ->head], copyNode->head);
        tempSet->insert(succ->head->NodeInfo());
      }
    }
    if(frameTempMap->Look(copyNode->head->NodeInfo()) != nullptr){
      for(auto temp = regsToColor; temp; temp = temp->tail){
        if(temp->head->Int() != copyNode->head->NodeInfo()->Int()){
          if(tempSet->find(temp->head) == tempSet->end()){
            // printf("%d ",temp->head->Int(), frameTempMap->Look(temp->head)->c_str());
            adjList->AddEdge(copyNode->head, (*regNodeMap)[temp->head]);
            tempSet->insert(temp->head);
          }
        }
      }
    }
    // printf("\n");
  }
  // adjList->Show(stdout, adjList->Nodes(), nullptr);
  adjSet = new std::vector<std::vector<bool> *>();
  adjSetXIndex = new std::map<G::Node<TEMP::Temp> *, int>();
  adjSetYIndex = new std::map<G::Node<TEMP::Temp> *, int>();

  regNumber = 0;
  for(auto temp = regsToColor; temp ; temp = temp->tail){
    regNumber++;
  }

  moveNodeMap = new std::map<G::Node<TEMP::Temp> *, std::map<G::Node<TEMP::Temp> *, moveType> *>();
  moveTypeNumber = new std::vector<int>(5, 0);

  initialNodes = new std::set<G::Node<TEMP::Temp> *>();
  spillWorkList = new std::set<G::Node<TEMP::Temp> *>();
  moveRelatedList = new std::set<G::Node<TEMP::Temp> *>();
  simplifyWorkList = new std::set<G::Node<TEMP::Temp> *>();

  selectStack = new std::stack<G::Node<TEMP::Temp> *>();

  coalescedChildNodes = new std::map<G::Node<TEMP::Temp> *, G::Node<TEMP::Temp> *>();
  coalesceParentNodes = new std::map<G::Node<TEMP::Temp> *, std::set<G::Node<TEMP::Temp> *> *>();
  spilledNodes = new std::set<G::Node<TEMP::Temp> *>();
  deleteNodesMap = new std::map<G::Node<TEMP::Temp> *, std::set<G::Node<TEMP::Temp> *> *>();
}

void build(){
  for(auto node = adjList->Nodes(); node; node = node->tail){
    if(frameTempMap->Look(node->head->NodeInfo()) == nullptr){
      initialNodes->insert(node->head);
    }
  }

  int xIndex = 0;
  int yIndex = 0;
  
  for(auto node = adjList->Nodes(); node; node = node->tail){
    std::vector<bool> *adjSetLine = new std::vector<bool>(adjList->nodecount, false);
    adjSetXIndex->insert(std::make_pair(node->head, xIndex));
    xIndex++;
    adjSet->push_back(adjSetLine);
    adjSetYIndex->insert(std::make_pair(node->head, yIndex));
    yIndex++;
  }
  for(auto node = adjList->Nodes(); node; node = node->tail){
    xIndex = (*adjSetXIndex)[node->head];
    for(auto succ = node->head->Succ(); succ; succ = succ->tail){
      yIndex = (*adjSetYIndex)[succ->head];
      (*(*adjSet)[xIndex])[yIndex] = true; 
    }
    for(auto pred = node->head->Pred(); pred; pred = pred->tail){
      yIndex = (*adjSetYIndex)[pred->head];
      (*(*adjSet)[xIndex])[yIndex] = true; 
    }
  }
  // printf("adjSet %d:%d\n", adjSet->size(), (*adjSet)[0]->size());
  

  for(auto moveInstr = moveList; moveInstr; moveInstr = moveInstr->tail){
    G::Node<TEMP::Temp> *src = (*old2newNodeMap)[moveInstr->src];
    G::Node<TEMP::Temp> *dst = (*old2newNodeMap)[moveInstr->dst];
    if(src->NodeInfo()->Int() == dst->NodeInfo()->Int()){
      // printf("samemove %d %d\n", src->NodeInfo()->Int(), dst->NodeInfo()->Int());
      continue;
    }
    // printf("move %d %d\n", src->NodeInfo()->Int(), dst->NodeInfo()->Int());
    
    if(moveNodeMap->find(src) == moveNodeMap->end()){
      std::map<G::Node<TEMP::Temp> *, moveType> *tempMap = new std::map<G::Node<TEMP::Temp> *, moveType>();
      tempMap->insert(std::make_pair(dst, moveType::workList));
      moveNodeMap->insert(std::make_pair(src, tempMap));
      (*moveTypeNumber)[moveType::workList]++;
    }
    else{
      if((*moveNodeMap)[src]->find(dst) == (*moveNodeMap)[src]->end()){
        (*moveNodeMap)[src]->insert(std::make_pair(dst, moveType::workList));
        (*moveTypeNumber)[moveType::workList]++;
      }
    }
    if(moveNodeMap->find(dst) == moveNodeMap->end()){
      std::map<G::Node<TEMP::Temp> *, moveType> *tempMap = new std::map<G::Node<TEMP::Temp> *, moveType>();
      tempMap->insert(std::make_pair(src, moveType::workList));
      moveNodeMap->insert(std::make_pair(dst, tempMap));
      (*moveTypeNumber)[moveType::workList]++;
    }
    else{
      if((*moveNodeMap)[dst]->find(src) == (*moveNodeMap)[dst]->end()){
        (*moveNodeMap)[dst]->insert(std::make_pair(src, moveType::workList));
        (*moveTypeNumber)[moveType::workList]++;
      }
    }
  }
  // printf("workMove %d, nodeMap %d\n", (*moveTypeNumber)[moveType::workList], moveNodeMap->size());
  
  return ;
}

void makeWorkList(){
  // printf("regNumber %d\n", regNumber);
  for(auto node = initialNodes->begin(); node != initialNodes->end(); node++){
    // // printf("node degree %d\n", (*node)->Degree());
    if((*node)->Degree() >= regNumber){//callersaves + calleesaves = 14
      spillWorkList->insert(*node); 
    }
    else if(moveNodeMap->find(*node) != moveNodeMap->end()){
      moveRelatedList->insert(*node);
    }
    else{
      simplifyWorkList->insert(*node);
    }
  }
  return ;
}

bool isWorkListEmpty(){
  return spillWorkList->empty() && moveRelatedList->empty() && simplifyWorkList->empty() && (*moveTypeNumber)[moveType::workList] == 0;
}

void simplify(){
  while(!simplifyWorkList->empty()){
    std::set<G::Node<TEMP::Temp> *> *tempSimplifyWorkList = new std::set<G::Node<TEMP::Temp> *>(simplifyWorkList->begin(), simplifyWorkList->end());
    while(!tempSimplifyWorkList->empty()){
      std::set<G::Node<TEMP::Temp> *>::iterator node = tempSimplifyWorkList->begin();
      // printf("sim temp %d %d\n", (*node)->NodeInfo()->Int(), (*node)->Degree());
      selectStack->push(*node);
      deleteNode(*node);
      tempSimplifyWorkList->erase(node);
      simplifyWorkList->erase(*node);
    }
    delete tempSimplifyWorkList;
  }

  for(auto first = moveNodeMap->begin(); first != moveNodeMap->end(); first++){
    for(auto second = first->second->begin(); second != first->second->end(); second++){
      if(second->second != moveType::workList){
        second->second = moveType::workList;
        (*moveTypeNumber)[moveType::workList]++;
        (*moveTypeNumber)[moveType::active]--;
      }
    }
  }
}

void coalesce(){
  std::map<G::Node<TEMP::Temp> *, std::map<G::Node<TEMP::Temp> *, moveType> *>::iterator first = moveNodeMap->begin();
  std::map<G::Node<TEMP::Temp> *, moveType>::iterator second = first->second->begin();
  while(true){
    if(first == moveNodeMap->end()){
      break;
    }

    if(second->second == moveType::workList){
      G::Node<TEMP::Temp> *actualSrc = actualNode(first->first);
      G::Node<TEMP::Temp> *actualDst = actualNode(second->first);
      // printf("coalesce check %d=%d %d=%d\n", actualSrc->NodeInfo()->Int(), first->first->NodeInfo()->Int(), actualDst->NodeInfo()->Int(), second->first->NodeInfo()->Int());
      if(actualSrc->NodeInfo()->Int() != actualDst->NodeInfo()->Int()){
        if((*(*adjSet)[(*adjSetXIndex)[actualSrc]])[(*adjSetYIndex)[actualDst]]
        || (coloring->Look(actualSrc->NodeInfo())!= nullptr && coloring->Look(actualDst->NodeInfo()) != nullptr)){
          // printf("constrained %d %d \n", actualSrc->NodeInfo()->Int(), actualDst->NodeInfo()->Int());
          freezeMove(first->first, second->first);
          first = moveNodeMap->begin();
          if(first == moveNodeMap->end()){
            break;
          }
          second = first->second->begin();
        }
        else{
          if(briggs(actualSrc, actualDst)){
            // printf("coalesce %d %d\n", actualSrc->NodeInfo()->Int(), actualDst->NodeInfo()->Int());
            coalesceNode(actualSrc, actualDst);
            return;
          }
          else{
            // printf("active %d %d\n", actualSrc->NodeInfo()->Int(), actualDst->NodeInfo()->Int());
            second->second  = moveType::active;
            (*moveTypeNumber)[moveType::active]++;
            (*moveTypeNumber)[moveType::workList]--;
            second++;
            if(second == first->second->end()){
              first++;
              if(first == moveNodeMap->end()){
                break;
              }
              second = first->second->begin();
            }
          }
        }
      }
      else{
        (*moveTypeNumber)[moveType::workList]--;
        first->second->erase(second);
        if(first->second->empty()){
          moveNodeMap->erase(first);
        }
        first = moveNodeMap->begin();
        if(first == moveNodeMap->end()){
          break;
        }
        second = first->second->begin();
      }
    }
    else{
      second++;
      if(second == first->second->end()){
        first++;
        // if(first == moveNodeMap->end()){
        //   break;
        // }
        second = first->second->begin();
      }
    }
  }
  return ;
}

void freeze(){
  int degree = (*(moveRelatedList->begin()))->Degree();
  G::Node<TEMP::Temp> *freezedNode = *(moveRelatedList->begin());
  for(auto node = moveRelatedList->begin(); node != moveRelatedList->end(); node++){
    if((*node)->Degree() < degree){
      freezedNode = *node;
    }
  }
  // printf("freeze %d\n", freezedNode->NodeInfo()->Int());
  freezeNode(freezedNode);

  return ;
}

void selectSpill(){
  int degree = (*(spillWorkList->begin()))->Degree();
  G::Node<TEMP::Temp> *spilledNode = *(moveRelatedList->begin());
  bool spillFlag = false;
  for(auto node = spillWorkList->begin(); node != spillWorkList->end(); node++){
    if((*node)->Degree() >= degree && coloring->Look((*node)->NodeInfo()) == nullptr){
      spilledNode = *node;
    }
  }
  spillWorkList->erase(spilledNode);
  spilledNodes->insert(spilledNode);
  selectStack->push(spilledNode);
  // printf("spill temp %d\n", spilledNode->NodeInfo()->Int());
  deleteNode(spilledNode);
  
  for(auto first = moveNodeMap->begin(); first != moveNodeMap->end(); first++){
    for(auto second = first->second->begin(); second != first->second->end(); second++){
      if(second->second == moveType::workList){
        second->second = moveType::workList;
        (*moveTypeNumber)[moveType::workList]++;
        (*moveTypeNumber)[moveType::active]--;
      }
    }
  }
  return ;
}

void assignColors(){
  while(!selectStack->empty()){
    
    G::Node<TEMP::Temp> *topNode = selectStack->top();

    // printf("select %d around ", topNode->NodeInfo()->Int());
    for(auto aroundNode = (*deleteNodesMap)[topNode]->begin(); aroundNode != (*deleteNodesMap)[topNode]->end(); aroundNode++){
      adjList->AddEdge(topNode, *aroundNode);
      // printf("%d ", (*aroundNode)->NodeInfo()->Int());
    }
    // printf("\n");
    
    std::set<std::string *> *colorAround = new std::set<std::string *>();
    for(auto node = topNode->Pred(); node; node = node->tail){
      std::string *color = coloring->Look(actualNode(node->head)->NodeInfo());
      if(color != nullptr){
        if(colorAround->find(color) == colorAround->end()){
          colorAround->insert(color);
        }
      }
    }

    for(auto node = topNode->Succ(); node; node = node->tail){
      std::string *color = coloring->Look(actualNode(node->head)->NodeInfo());
      if(color != nullptr){
        if(colorAround->find(color) == colorAround->end()){
          colorAround->insert(color);
        }
      }
    }

    // if(coalesceParentNodes->find(selectStack->top()) != coalesceParentNodes->end()){
    //   std::set<G::Node<TEMP::Temp>* > *childSet = (*coalesceParentNodes)[selectStack->top()];
    //   for(auto child = childSet->begin(); child != childSet->end(); child++){
    //     for(auto neibNode = (*new2oldNodeMap)[*child]->Pred(); neibNode; neibNode = neibNode->tail){
    //       std::string *color = coloring->Look(neibNode->head->NodeInfo());
    //       if(color != nullptr){
    //         if(colorAround->find(color) == colorAround->end()){
    //           colorAround->insert(color);
    //         }
    //       }
    //     }
    //     for(auto neibNode = (*new2oldNodeMap)[*child]->Succ(); neibNode; neibNode = neibNode->tail){
    //       std::string *color = coloring->Look(neibNode->head->NodeInfo());
    //       if(color != nullptr){
    //         if(colorAround->find(color) == colorAround->end()){
    //           colorAround->insert(color);
    //         }
    //       }
    //     }
    //   }
    // }

    bool colorFlag = false;
    for(auto temp = regsToColor; temp; temp = temp->tail){
      std::string *color = coloring->Look(temp->head);
      if(colorAround->find(color) == colorAround->end()){
        // printf("enter temp %d:%s \n", topNode->NodeInfo()->Int(), coloring->Look(temp->head)->c_str());
        
        coloring->Enter(topNode->NodeInfo(), coloring->Look(temp->head));
        colorFlag = true;
        break;
      }
    }

    if(!colorFlag){
      if(spilledNodes->find(selectStack->top()) != spilledNodes->end()){
        // printf("spilled %d: ", topNode->NodeInfo()->Int());
        for(auto node = (*new2oldNodeMap)[selectStack->top()]->Pred(); node; node = node->tail){
          // printf("%d ", node->head->NodeInfo()->Int());
          
        }
        for(auto node = (*new2oldNodeMap)[selectStack->top()]->Succ(); node; node = node->tail){
          // printf("%d ", node->head->NodeInfo()->Int());
          
        }
        // printf("\n");
        spills = new TEMP::TempList(topNode->NodeInfo(), spills);
      }
      else{//never happened
        // printf("simplify error %d: ", topNode->NodeInfo()->Int());
        for(auto node = (*new2oldNodeMap)[selectStack->top()]->Pred(); node; node = node->tail){
          // printf("%d ", node->head->NodeInfo()->Int());
          
        }
        for(auto node = (*new2oldNodeMap)[selectStack->top()]->Succ(); node; node = node->tail){
          // printf("%d ", node->head->NodeInfo()->Int());
          
        }
        // printf("\n");
        
      }
    }
    
    selectStack->pop();
  }

  for(auto node = coalescedChildNodes->begin(); node != coalescedChildNodes->end(); node++){
    std::string *color = coloring->Look(actualNode(node->first)->NodeInfo());
    if(color != nullptr){
      coloring->Enter(node->first->NodeInfo(), color);
      // printf("enter coa temp %d %d:%s \n", node->first->NodeInfo()->Int(), node->second->NodeInfo()->Int(), coloring->Look(actualNode(node->first)->NodeInfo())->c_str());
    }
    else{
      coloring->Enter(node->first->NodeInfo(), coloring->Look(F::RV()));
      // printf("enter spill coa temp\n");
    }
  }
  return ;
}

G::Node<TEMP::Temp> * actualNode(G::Node<TEMP::Temp> * node){
  G::Node<TEMP::Temp> *actual = node;
  while(coalescedChildNodes->find(actual) != coalescedChildNodes->end()){
    actual = (*coalescedChildNodes)[actual];
  }
  return actual;
}

void deleteNode(G::Node<TEMP::Temp> *centerNode){

  std::vector<G::Node<TEMP::Temp> *> *deleteNodes = new std::vector<G::Node<TEMP::Temp> *>();
  std::set<G::Node<TEMP::Temp> *> * aroundNodes = new std::set<G::Node<TEMP::Temp> *>();
  for(auto node = centerNode->Pred(); node; node = node->tail){
    int degree = node->head->Degree();
    deleteNodes->push_back(node->head);
    if(degree == regNumber && coloring->Look(node->head->NodeInfo()) == nullptr){
      spillWorkList->erase(node->head);
      if(moveNodeMap->find(node->head) != moveNodeMap->end()){
        moveRelatedList->insert(node->head);
      }
      else{
        simplifyWorkList->insert(node->head);
      }
    }
  }
  for(auto node = deleteNodes->begin(); node != deleteNodes->end(); node++){
    adjList->RmEdge(*node, centerNode);
    if(aroundNodes->find(*node) == aroundNodes->end()){
      aroundNodes->insert(*node);
    }
  }
  delete deleteNodes;
  deleteNodes = new std::vector<G::Node<TEMP::Temp> *>();
  for(auto node = centerNode->Succ(); node; node = node->tail){
    int degree = node->head->Degree();
    deleteNodes->push_back(node->head);
    if(degree == regNumber && coloring->Look(node->head->NodeInfo()) == nullptr){
      spillWorkList->erase(node->head);
      if(moveNodeMap->find(node->head) != moveNodeMap->end()){
        moveRelatedList->insert(node->head);
      }
      else{
        simplifyWorkList->insert(node->head);
      }
    }
  }
  for(auto node = deleteNodes->begin(); node != deleteNodes->end(); node++){
    adjList->RmEdge(centerNode, *node);
    if(aroundNodes->find(*node) == aroundNodes->end()){
      aroundNodes->insert(*node);
    }
  }
  delete deleteNodes;
  deleteNodesMap->insert(std::make_pair(centerNode, aroundNodes));
  // printf("delete %d around ", centerNode->NodeInfo()->Int());
  for(auto aroundNode = (*deleteNodesMap)[centerNode]->begin(); aroundNode != (*deleteNodesMap)[centerNode]->end(); aroundNode++){
    // printf("%d ", (*aroundNode)->NodeInfo()->Int());
  }
  // printf("\n");
  delete (*adjSet)[(*adjSetXIndex)[centerNode]];
  (*adjSet)[(*adjSetXIndex)[centerNode]] = new std::vector<bool>(adjList->nodecount, false);

  if(moveNodeMap->find(centerNode) != moveNodeMap->end()){
    // printf("deleteNode %d move\n", centerNode->NodeInfo()->Int());
    auto centerNodeMap = (*moveNodeMap)[centerNode];
    for(auto node = centerNodeMap->begin(); node != centerNodeMap->end(); node++){
      (*moveTypeNumber)[(*(*moveNodeMap)[node->first])[centerNode]]--;
      (*moveTypeNumber)[(*(*moveNodeMap)[centerNode])[node->first]]--;
      (*moveNodeMap)[node->first]->erase(centerNode);
      if((*moveNodeMap)[node->first]->empty()){
        delete (*moveNodeMap)[node->first];
        moveNodeMap->erase(node->first);
        if(moveRelatedList->find(node->first) != moveRelatedList->end()){
          moveRelatedList->erase(node->first);
          simplifyWorkList->insert(node->first);
        }
      }
    }
    delete centerNodeMap;
    moveNodeMap->erase(centerNode);
    if(moveRelatedList->find(centerNode) != moveRelatedList->end()){
      moveRelatedList->erase(centerNode);
    }
  }

  if(spillWorkList->find(centerNode) != spillWorkList->end()){
    spillWorkList->erase(centerNode);
  }
}

bool briggs(G::Node<TEMP::Temp> *aNode, G::Node<TEMP::Temp> *bNode){
  if(aNode->NodeInfo() == F::SP() || bNode->NodeInfo() == F::SP()){
    return false;
  }

  std::set<G::Node<TEMP::Temp> *> *neibSet = new std::set<G::Node<TEMP::Temp> *>();
  bool colored = coloring->Look(aNode->NodeInfo()) != nullptr || coloring->Look(bNode->NodeInfo()) != nullptr;
  for(auto node = aNode->Pred(); node; node = node->tail){
    if(neibSet->find(node->head) == neibSet->end()){
      neibSet->insert(node->head);
    }
  }
  for(auto node = aNode->Succ(); node; node = node->tail){
    if(neibSet->find(node->head) == neibSet->end()){
      neibSet->insert(node->head);
    }
  }
  for(auto node = bNode->Pred(); node; node = node->tail){
    if(neibSet->find(node->head) == neibSet->end()){
      neibSet->insert(node->head);
    }
  }
  for(auto node = bNode->Succ(); node; node = node->tail){
    if(neibSet->find(node->head) == neibSet->end()){
      neibSet->insert(node->head);
    }
  }
  // printf("briggs %d %d %d: ", aNode->NodeInfo()->Int(), bNode->NodeInfo()->Int(), neibSet->size());
  int count = 0;
  for(auto node = neibSet->begin(); node != neibSet->end(); node++){
    
    int degree = (*node)->Degree();
    if((*(*adjSet)[(*adjSetXIndex)[*node]])[(*adjSetYIndex)[bNode]]
    && (*(*adjSet)[(*adjSetXIndex)[*node]])[(*adjSetYIndex)[aNode]]){
      degree--;
    }
    // printf("%d=%d ", (*node)->NodeInfo()->Int(), degree);
    if(colored){
      if(degree >= regNumber){
        count++;
      }
    }
    else{
      if(degree >= regNumber || coloring->Look((*node)->NodeInfo()) != nullptr){
        count++;
      }
    }
  }
  // printf("\n");
  return count < regNumber;
}

bool george(G::Node<TEMP::Temp> *aNode, G::Node<TEMP::Temp> *bNode){
  for(auto aPred = aNode->Pred(); aPred; aPred = aPred->tail){
    if(!(*(*adjSet)[(*adjSetXIndex)[aPred->head]])[(*adjSetYIndex)[bNode]]){
      // printf("george %d %d %d\n", aNode->NodeInfo()->Int(), aPred->head->NodeInfo()->Int(), aPred->head->Degree());
      if(aPred->head->Degree() >= regNumber){
        return false;
      }
    }
    else{
      if(coloring->Look(aPred->head->NodeInfo()) != nullptr){
        return false;
      }
    }
  }

  for(auto aSucc = aNode->Succ(); aSucc; aSucc = aSucc->tail){
    if(!(*(*adjSet)[(*adjSetXIndex)[aSucc->head]])[(*adjSetYIndex)[bNode]]){
      // printf("george %d %d %d\n", aNode->NodeInfo()->Int(), aSucc->head->NodeInfo()->Int(), aSucc->head->Degree());
      if(aSucc->head->Degree() >= regNumber){
        return false;
      }
    }
    else{
      if(coloring->Look(aSucc->head->NodeInfo()) != nullptr){
        return false;
      }
    }
  }
  return true;
}

void coalesceNode(G::Node<TEMP::Temp> *src, G::Node<TEMP::Temp> *dst){
  G::Node<TEMP::Temp> *deletedNode;
  G::Node<TEMP::Temp> *reservedNode;
  if(frameTempMap->Look(dst->NodeInfo()) != nullptr){
    deletedNode = src;
    reservedNode = dst;
  }
  else{
    deletedNode = dst;
    reservedNode = src;
  }

  std::map<G::Node<TEMP::Temp> *, moveType> *reservedNodeMap = (*moveNodeMap)[reservedNode];
  // printf("coaNode %d %d: ", reservedNode->NodeInfo()->Int(), deletedNode->NodeInfo()->Int());
  for(auto node = deletedNode->Succ(); node; node = node->tail){
    if(!(*(*adjSet)[(*adjSetXIndex)[node->head]])[(*adjSetYIndex)[reservedNode]]){
      (*(*adjSet)[(*adjSetXIndex)[node->head]])[(*adjSetYIndex)[reservedNode]] = true;
      (*(*adjSet)[(*adjSetXIndex)[reservedNode]])[(*adjSetYIndex)[node->head]] = true;
      // printf("%d ", node->head->NodeInfo()->Int());
      adjList->AddEdge(reservedNode, node->head);
    }

    if(moveNodeMap->find(node->head) != moveNodeMap->end()){
      if((*moveNodeMap)[node->head]->find(deletedNode) !=  (*moveNodeMap)[node->head]->end()){
        if((*moveNodeMap)[node->head]->find(reservedNode) ==  (*moveNodeMap)[node->head]->end()){
          reservedNodeMap->insert(std::make_pair(node->head, (*(*moveNodeMap)[node->head])[deletedNode]));
          (*moveNodeMap)[node->head]->insert(std::make_pair(reservedNode, (*(*moveNodeMap)[node->head])[deletedNode]));
          (*moveTypeNumber)[(*(*moveNodeMap)[node->head])[deletedNode]]++;
          (*moveTypeNumber)[(*(*moveNodeMap)[node->head])[deletedNode]]++;
        }
      }
    }
  }
  for(auto node = deletedNode->Pred(); node; node = node->tail){
    if(!(*(*adjSet)[(*adjSetXIndex)[node->head]])[(*adjSetYIndex)[reservedNode]]){
      (*(*adjSet)[(*adjSetXIndex)[node->head]])[(*adjSetYIndex)[reservedNode]] = true;
      (*(*adjSet)[(*adjSetXIndex)[reservedNode]])[(*adjSetYIndex)[node->head]] = true;
      adjList->AddEdge(reservedNode, node->head);
      // printf("%d ", node->head->NodeInfo()->Int());
    }

    if(moveNodeMap->find(node->head) != moveNodeMap->end()){
      if((*moveNodeMap)[node->head]->find(deletedNode) !=  (*moveNodeMap)[node->head]->end()){
        if((*moveNodeMap)[node->head]->find(reservedNode) ==  (*moveNodeMap)[node->head]->end()){
          reservedNodeMap->insert(std::make_pair(node->head, (*(*moveNodeMap)[node->head])[deletedNode]));
          (*moveNodeMap)[node->head]->insert(std::make_pair(reservedNode, (*(*moveNodeMap)[node->head])[deletedNode]));
          (*moveTypeNumber)[(*(*moveNodeMap)[node->head])[deletedNode]]++;
          (*moveTypeNumber)[(*(*moveNodeMap)[node->head])[deletedNode]]++;
        }
      }
    }
  }
  // printf("\n");
  
  coalescedChildNodes->insert(std::make_pair(deletedNode, reservedNode)); 
  if(coalesceParentNodes->find(reservedNode) != coalesceParentNodes->end()){
    if((*coalesceParentNodes)[reservedNode]->find(deletedNode) == (*coalesceParentNodes)[reservedNode]->end()){
      (*coalesceParentNodes)[reservedNode]->insert(deletedNode);
      // // printf("coa %d: %d\n", reservedNode->NodeInfo()->Int(), deletedNode->NodeInfo()->Int());
    }
  }
  else{
    (*coalesceParentNodes)[reservedNode] = new std::set<G::Node<TEMP::Temp> *>();
    (*coalesceParentNodes)[reservedNode]->insert(deletedNode);\
    // // printf("coa %d: %d\n", reservedNode->NodeInfo()->Int(), deletedNode->NodeInfo()->Int());s
  }

  deleteNode(deletedNode);

  if(coloring->Look(reservedNode->NodeInfo()) == nullptr){
    if(spillWorkList->find(reservedNode) != spillWorkList->end()){
      spillWorkList->erase(reservedNode);
    }
    if(moveRelatedList->find(reservedNode) != moveRelatedList->end()){
      moveRelatedList->erase(reservedNode);
    }
    if(simplifyWorkList->find(reservedNode) != simplifyWorkList->end()){
      simplifyWorkList->erase(reservedNode);
    }
    if(reservedNode->Degree() >= regNumber){
      spillWorkList->insert(reservedNode);
    }
    else if(moveNodeMap->find(reservedNode) != moveNodeMap->end()){
      moveRelatedList->insert(reservedNode);
    }
    else{
      simplifyWorkList->insert(reservedNode);
    }
  }
}

void freezeNode(G::Node<TEMP::Temp> *freezedNode){
  auto freezedNodeSet = (*moveNodeMap)[freezedNode];
  while (!freezedNodeSet->empty())
  {
    freezeMove(freezedNode, freezedNodeSet->begin()->first);
  }
}

void freezeMove(G::Node<TEMP::Temp> *src, G::Node<TEMP::Temp> *dst){
  if(moveNodeMap->find(src) != moveNodeMap->end()){
    (*moveTypeNumber)[(*(*moveNodeMap)[src])[dst]]--;
    (*moveTypeNumber)[(*(*moveNodeMap)[dst])[src]]--;

    (*moveNodeMap)[src]->erase(dst);
    (*moveNodeMap)[dst]->erase(src);
    
    if((*moveNodeMap)[src]->empty()){
      if(moveRelatedList->find(src) != moveRelatedList->end()){
        moveRelatedList->erase(src);
        if(coloring->Look(src->NodeInfo()) == nullptr){
          simplifyWorkList->insert(src);
        }
      }
      moveNodeMap->erase(src);
    }

    if((*moveNodeMap)[dst]->empty()){
      if(moveRelatedList->find(dst) != moveRelatedList->end()){
        moveRelatedList->erase(dst);
        if(coloring->Look(dst->NodeInfo()) == nullptr){
          simplifyWorkList->insert(dst);
        }
      }
      moveNodeMap->erase(dst);
    }
  }
}

}  // namespace COL