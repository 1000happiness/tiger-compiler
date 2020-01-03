#include "tiger/regalloc/regalloc.h"

namespace RA {
AS::InstrList *newIl, *newIlCur;
TEMP::Map *inframeRegsMap, *coloring;
TEMP::Map *frameTempMap = nullptr;
F::Frame *frame = nullptr;
TEMP::TempList *regs = nullptr;

void initialRegAlloc(F::Frame* f, AS::InstrList* il);
void main();
void rewriteProgram(TEMP::TempList *spill);
void moveInstrInframe(F::Frame* f, AS::MoveInstr *moveInstr, std::set<TEMP::Temp *> *spillSet);
void labelInstrInframe(F::Frame* f, AS::LabelInstr *labelInstr, std::set<TEMP::Temp *> *spillSet);
void operInstrInframe(F::Frame* f, AS::OperInstr *operInstr, std::set<TEMP::Temp *> *spillSet);
void emit(AS::Instr *instr);
void removeSameReg();
bool IsSpilledTempExisted(TEMP::TempList *src, TEMP::TempList *dst, std::set<TEMP::Temp *> *spillSet);
void replaceSpilledReg(std::string assem, TEMP::TempList* dst, TEMP::TempList* src, AS::Targets* jumps);
TEMP::Temp* nth_temp(TEMP::TempList* list, int i);
TEMP::Label* nth_label(TEMP::LabelList* list, int i);
void changeNthTemp(TEMP::TempList* list, int i, TEMP::Temp *newTemp);

Result RegAlloc(F::Frame* f, AS::InstrList* rawIl) {
  initialRegAlloc(f, rawIl);

  main();

  Result allocResult = Result(coloring ,newIl);
  return allocResult;
}

void initialRegAlloc(F::Frame* f, AS::InstrList* rawIl){
  newIl = nullptr;
  newIlCur = nullptr;
  inframeRegsMap = TEMP::Map::Empty();
  TEMP::TempList *regsCur = nullptr;// the calleeSave will be used after argsReg and CallerSaves
  regs = F::Regs();

  frame = f;
  for(auto it = rawIl; it; it = it->tail){
    emit(it->head);
  }
}

void main(){
  LIVE::LiveGraph *liveGraph = LIVE::Liveness(FG::AssemFlowGraph(newIl));
  // liveGraph->graph->Show(stdout, liveGraph->graph->Nodes(), nullptr);
  TEMP::Map *frameTempMap = F::FrameTempMap();
  TEMP::Map *initial = TEMP::Map::Empty();//deepcopy frame temp map
  for(auto temp = regs; temp; temp = temp->tail){
    initial->Enter(temp->head, frameTempMap->Look(temp->head));
  }
  initial->Enter(F::SP(), new std::string("%rsp"));

  COL::Result *colResult = COL::Color(liveGraph->graph, initial, regs, liveGraph->moves);
  coloring = colResult->coloring;
  
  rewriteProgram(colResult->spills);
  // newIl->Print(stdout, TEMP::Map::Name());
  if(colResult->spills != nullptr){ 
    main();
  }
  else{
    // newIl->Print(stdout, TEMP::Map::Name());
    removeSameReg();
    // newIl->Print(stdout, TEMP::Map::Name());
    // newIl->Print(stdout, coloring);
  }
}

void rewriteProgram(TEMP::TempList *spills){
  std::set<TEMP::Temp *> *spillSet = new std::set<TEMP::Temp *>();
  for(auto temp = spills; temp; temp = temp->tail){
    spillSet->insert(temp->head);
    // printf("rewrite spill %d\n", temp->head->Int());
  }

  AS::InstrList *tempIl = newIl;
  newIl = nullptr;

  for(AS::InstrList *it = tempIl; it; it=it->tail){
    // it->head->Print(stdout, TEMP::Map::Name());
    if(it->head->kind == AS::Instr::MOVE){
      moveInstrInframe(frame, (AS::MoveInstr *)it->head, spillSet);
    }
    else if(it->head->kind == AS::Instr::OPER){
      operInstrInframe(frame, (AS::OperInstr *)it->head, spillSet);
    }
    else if(it->head->kind == AS::Instr::LABEL){
      labelInstrInframe(frame, (AS::LabelInstr *)it->head, spillSet);
    }
    // newIlCur->Print(stdout, TEMP::Map::Name());
  }

  delete spillSet;
}

void moveInstrInframe(F::Frame* f, AS::MoveInstr *moveInstr, std::set<TEMP::Temp *> *spillSet){
  if(IsSpilledTempExisted(moveInstr->src, moveInstr->dst, spillSet)){
    if(moveInstr->assem.find("(") == -1 && moveInstr->assem.find("$") == -1){// mov regB, regA
      if(coloring->Look(moveInstr->src->head) != nullptr){// src colored
        if(coloring->Look(moveInstr->dst->head) != nullptr){
          emit(moveInstr);
        }
        else{
          if(inframeRegsMap->Look(moveInstr->dst->head) == nullptr){
            f->length += F::addressSize;
            std::stringstream assemStream;
            assemStream << f->namedFrameLength() << "+" << -f->length << "(%rsp)";
            emit(new AS::MoveInstr("movq `s0, " + assemStream.str(), nullptr, new TEMP::TempList(moveInstr->src->head, new TEMP::TempList(F::SP(), nullptr))));
            inframeRegsMap->Enter(moveInstr->dst->head, new std::string(assemStream.str()));
          }
          else{
            emit(new AS::MoveInstr("movq `s0, " + *inframeRegsMap->Look(moveInstr->dst->head), nullptr, new TEMP::TempList(moveInstr->src->head, new TEMP::TempList(F::SP(), nullptr))));
          }
        }
        return ;
      }
      else if(coloring->Look(moveInstr->dst->head) != nullptr){// dst colored
        emit(new AS::MoveInstr("movq " + *inframeRegsMap->Look(moveInstr->src->head) + ", `d0", new TEMP::TempList(moveInstr->dst->head, nullptr),new TEMP::TempList(F::SP(), nullptr)));
      }
      else{// src not colored, dst not colored
        TEMP::Temp *newTemp = TEMP::Temp::NewTemp();
        emit(new AS::MoveInstr("movq " + *inframeRegsMap->Look(moveInstr->src->head) + ", `d0", new TEMP::TempList(newTemp, nullptr), new TEMP::TempList(F::SP(), nullptr)));
        if(inframeRegsMap->Look(moveInstr->dst->head) == nullptr){
          f->length += F::addressSize;
          std::stringstream assemStream;
          assemStream << f->namedFrameLength() << "+" << -f->length << "(%rsp)";
          emit(new AS::MoveInstr("movq `s0, " + assemStream.str(), nullptr, new TEMP::TempList(newTemp, new TEMP::TempList(F::SP(), nullptr))));
          inframeRegsMap->Enter(moveInstr->dst->head, new std::string(assemStream.str()));
        }
        else{
          emit(new AS::MoveInstr("movq `s0, " + *inframeRegsMap->Look(moveInstr->dst->head), nullptr, new TEMP::TempList(newTemp, new TEMP::TempList(F::SP(), nullptr))));
        }
      }
    }
    else if(moveInstr->assem.find("(") == -1 && moveInstr->assem.find("$") != -1){//movq $1, regA 
      if(coloring->Look(moveInstr->dst->head) != nullptr){ // dst colored
        emit(moveInstr);
      }
      else{// dst not colored
        TEMP::Temp *newTemp = TEMP::Temp::NewTemp();
        if(inframeRegsMap->Look(moveInstr->dst->head) == nullptr){
          f->length += F::addressSize;
          std::stringstream assemStream;
          assemStream << f->namedFrameLength() << "+" << -f->length << "(%rsp)";
          emit(new AS::MoveInstr(moveInstr->assem, new TEMP::TempList(newTemp, nullptr), nullptr));
          emit(new AS::MoveInstr("movq `s0, " + assemStream.str(), nullptr, new TEMP::TempList(newTemp, nullptr)));
          inframeRegsMap->Enter(moveInstr->dst->head, new std::string(assemStream.str()));
        }
        else{
          emit(new AS::MoveInstr(moveInstr->assem, new TEMP::TempList(newTemp, nullptr), nullptr));
          emit(new AS::MoveInstr("movq `s0, " + *inframeRegsMap->Look(moveInstr->dst->head), nullptr, new TEMP::TempList(newTemp, new TEMP::TempList(F::SP(), nullptr))));
        }
      }
    }
    else if(moveInstr->assem.find("(") != -1 && moveInstr->assem.find("(") < moveInstr->assem.find(",") && moveInstr->assem.find("%rip") == -1){//movq 1(regB), regA
      if(coloring->Look(moveInstr->dst->head) != nullptr){//dst colored
        if(coloring->Look(moveInstr->src->head) != nullptr){// src colored
          emit(moveInstr);
        }
        else{// src not colored
          TEMP::Temp *newTemp = TEMP::Temp::NewTemp();
          emit(new AS::MoveInstr("movq " + *inframeRegsMap->Look(moveInstr->src->head) + ", `d0", new TEMP::TempList(newTemp, nullptr), new TEMP::TempList(F::SP(), nullptr)));
          emit(new AS::MoveInstr(moveInstr->assem, moveInstr->dst, new TEMP::TempList(newTemp, nullptr)));
        }
      }
      else{//dst not colored
        if(coloring->Look(moveInstr->src->head) != nullptr){// src colored
          TEMP::Temp *newTemp = TEMP::Temp::NewTemp();
          emit(new AS::MoveInstr(moveInstr->assem, new TEMP::TempList(newTemp, nullptr), moveInstr->src));
          if(inframeRegsMap->Look(moveInstr->dst->head) == nullptr){
            f->length += F::addressSize;
            std::stringstream assemStream;
            assemStream << f->namedFrameLength() << "+" << -f->length << "(%rsp)";
            emit(new AS::MoveInstr("movq `s0, " + assemStream.str(), nullptr, new TEMP::TempList(newTemp, new TEMP::TempList(F::SP(), nullptr))));
            inframeRegsMap->Enter(moveInstr->dst->head, new std::string(assemStream.str()));
          }
          else{
            emit(new AS::MoveInstr("movq `s0, " + *inframeRegsMap->Look(moveInstr->dst->head), nullptr, new TEMP::TempList(newTemp, new TEMP::TempList(F::SP(), nullptr))));
          }
        }
        else{// src not colored
          TEMP::Temp *newTemp = TEMP::Temp::NewTemp();
          emit(new AS::MoveInstr("movq " + *inframeRegsMap->Look(moveInstr->src->head) + ", `d0", new TEMP::TempList(newTemp, nullptr), nullptr));
          if(inframeRegsMap->Look(moveInstr->dst->head) == nullptr){
            f->length += F::addressSize;
            std::stringstream assemStream;
            assemStream << f->namedFrameLength() << "+" << -f->length << "(%rsp)";
            emit(new AS::MoveInstr("movq `s0, " + assemStream.str(), nullptr, new TEMP::TempList(newTemp, nullptr)));
            inframeRegsMap->Enter(moveInstr->dst->head, new std::string(assemStream.str()));
          }
          else{
            emit(new AS::MoveInstr("movq `s0, " + *inframeRegsMap->Look(moveInstr->dst->head), nullptr, new TEMP::TempList(newTemp, nullptr)));
          }
        }
      }
    }
    else if(moveInstr->assem.find("%rip") != -1){//leaq L1(%rip), regA
      if(coloring->Look(moveInstr->dst->head) != nullptr){
        emit(moveInstr);
      }
      else{
        TEMP::Temp *newTemp = TEMP::Temp::NewTemp();
        if(coloring->Look(moveInstr->dst->head) == nullptr){
          f->length += F::addressSize;
          std::stringstream assemStream;
          assemStream << f->namedFrameLength() << "+" << -f->length << "(%rsp)";
          emit(new AS::MoveInstr(moveInstr->assem, new TEMP::TempList(newTemp, nullptr), nullptr));
          emit(new AS::MoveInstr("movq `s0, " + assemStream.str(), nullptr, new TEMP::TempList(newTemp, nullptr)));
          inframeRegsMap->Enter(moveInstr->dst->head, new std::string(assemStream.str()));
        }
        else{
          emit(new AS::MoveInstr(moveInstr->assem, new TEMP::TempList(newTemp, nullptr), nullptr));
          emit(new AS::MoveInstr("movq `s0, " + *inframeRegsMap->Look(moveInstr->dst->head), nullptr, new TEMP::TempList(newTemp, nullptr)));
        }
      }
    }
    else{// mov regB, 1(regA) | mov $4, 1(regA)
      replaceSpilledReg(moveInstr->assem, moveInstr->dst, moveInstr->src, nullptr);
    }
  }
  else{
    emit(moveInstr);
  }
}

void labelInstrInframe(F::Frame* f, AS::LabelInstr *labelInstr, std::set<TEMP::Temp *> *spillSet){
  emit(labelInstr);
  return ;
}

void operInstrInframe(F::Frame* f, AS::OperInstr *operInstr, std::set<TEMP::Temp *> *spillSet){
  if(IsSpilledTempExisted(operInstr->src, operInstr->dst, spillSet)){
    replaceSpilledReg(operInstr->assem, operInstr->dst, operInstr->src, nullptr);
  }
  else{
    // if(operInstr->assem.find("call") != -1){
    //   callEmit(f, operInstr);
    // }
    // else{
    //   emit(operInstr);
    // }
    emit(operInstr);
  }
  return ;
}

void emit(AS::Instr *instr){
  if(newIl == nullptr){
    newIl = new AS::InstrList(instr, nullptr);
    newIlCur = newIl;
  }
  else{
    newIlCur->tail = new AS::InstrList(instr, nullptr);
    newIlCur = newIlCur->tail;
  }
}

bool IsSpilledTempExisted(TEMP::TempList *src, TEMP::TempList *dst, std::set<TEMP::Temp *> *spillSet){
  for(auto temp = src; temp; temp = temp->tail){
    if(spillSet->find(temp->head) != spillSet->end()){
      return true;
    }
  }
  for(auto temp = dst; temp; temp = temp->tail){
    if(spillSet->find(temp->head) != spillSet->end()){
      return true;
    }
  }
  return false;
}

void removeSameReg(){
  AS::InstrList *tempIl = newIl;
  newIl = nullptr;
  for(AS::InstrList *it = tempIl; it; it=it->tail){
    if(it->head->kind == AS::Instr::MOVE){
      AS::MoveInstr *moveInstr = (AS::MoveInstr *)it->head;
      if(moveInstr->assem.find("(") == -1 && moveInstr->assem.find("$") == -1){
        if(coloring->Look(moveInstr->src->head) != coloring->Look(moveInstr->dst->head)){
          emit(moveInstr);
        }
      }
      else{
        emit(it->head);
      }
    }
    else{
      emit(it->head);
    }
  }
}

void replaceSpilledReg(std::string assem, TEMP::TempList* dst, TEMP::TempList* src, AS::Targets* jumps){
  
  TEMP::Temp *dstTemp = nullptr;
  TEMP::Temp *aternativeTemp = nullptr; 
  for (int i = 0; i < assem.size(); i++) {
    char ch = assem.at(i);
    if (ch == '`') {
      i++;
      switch (assem.at(i)) {
        case 's': {
          i++;
          int n = assem.at(i) - '0';
          TEMP::Temp *temp = nth_temp(src, n);
          if(coloring->Look(temp) == nullptr){
            TEMP::Temp *newTemp = TEMP::Temp::NewTemp();
            emit(new AS::MoveInstr("movq " + *inframeRegsMap->Look(temp) + ", `d0", new TEMP::TempList(newTemp, nullptr), nullptr));
            changeNthTemp(src, n, newTemp);
          }
        } break;
        case 'd': {
          i++;
          int n = assem.at(i) - '0';
          dstTemp = nth_temp(dst, n);
          if(coloring->Look(dstTemp) == nullptr){
            TEMP::Temp *newTemp = TEMP::Temp::NewTemp();
            emit(new AS::MoveInstr("movq " + *inframeRegsMap->Look(dstTemp) + ", `d0", new TEMP::TempList(newTemp, nullptr),nullptr));
            changeNthTemp(dst, n, newTemp);
            aternativeTemp = newTemp;
          }
        } break;
        case 'j':
          i++;
          break; 
        case '`': 
          break;
        default:
          assert(0);
      }
    } 
  }
  emit(new AS::OperInstr(assem, dst, src, jumps));
  if(aternativeTemp != nullptr){
    emit(new AS::MoveInstr("movq `s0, " + *inframeRegsMap->Look(dstTemp), nullptr, new TEMP::TempList(aternativeTemp, nullptr)));
  }
}

TEMP::Temp* nth_temp(TEMP::TempList* list, int i) {
  if (i == 0)
    return list->head;
  else
    return nth_temp(list->tail, i - 1);
}

TEMP::Label* nth_label(TEMP::LabelList* list, int i) {
  if (i == 0)
    return list->head;
  else
    return nth_label(list->tail, i - 1);
}

void changeNthTemp(TEMP::TempList* list, int i, TEMP::Temp *newTemp){
  if (i == 0)
    list->head = newTemp;
  else
    changeNthTemp(list->tail, i - 1, newTemp);
}

}  // namespace RA