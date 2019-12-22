#include "tiger/regalloc/regalloc.h"

namespace RA {
AS::InstrList *newIl, *newIlCur;
TEMP::Map *inframe, *coloring;
TEMP::Map *frameTM = F::FrameTempMap();

void moveInstregAlloc(F::Frame* f, AS::MoveInstr *moveInstr);
void operInstregAlloc(F::Frame* f, AS::OperInstr *operInstr);
void labelInstregAlloc(F::Frame* f, AS::LabelInstr *labelInstr);

void emit(AS::Instr *instr);
bool IsCalleeSaves(TEMP::Temp *temp);
bool IsCallerSaves(TEMP::Temp *temp);
void frameInstr(std::string assem, TEMP::TempList* dst, TEMP::TempList* src, AS::Targets* jumps);
TEMP::Temp* nth_temp(TEMP::TempList* list, int i);
TEMP::Label* nth_label(TEMP::LabelList* list, int i);
void changeNthTemp(TEMP::TempList* list, int i, TEMP::Temp *newTemp);

Result RegAlloc(F::Frame* f, AS::InstrList* il) {
  newIl = nullptr;
  newIlCur = nullptr;
  inframe = TEMP::Map::Empty();
  
  for(AS::InstrList *it = il; it; it=it->tail){
    // it->head->Print(stdout, TEMP::Map::LayerMap(F::FrameTempMap(), TEMP::Map::Name()));
    // fflush(stdout);
    if(it->head->kind == AS::Instr::MOVE){
      moveInstregAlloc(f, (AS::MoveInstr *)it->head);
    }
    else if(it->head->kind == AS::Instr::OPER){
      operInstregAlloc(f, (AS::OperInstr *)it->head);
    }
    else if(it->head->kind == AS::Instr::LABEL){
      labelInstregAlloc(f, (AS::LabelInstr *)it->head);
    }
    
    // newIlCur->Print(stdout, TEMP::Map::LayerMap(F::FrameTempMap(), TEMP::Map::Name()));
    // fflush(stdout);
  }
  Result allocResult = Result(coloring ,newIl);
  return allocResult;
}

void moveInstregAlloc(F::Frame* f, AS::MoveInstr *moveInstr){
  if(moveInstr->assem.find("(") == -1 && moveInstr->assem.find("$") == -1){//movq regA, regB 只用于lab5
    // moveInstr->assem = "111111" + moveInstr->assem;
    // emit(moveInstr);
    if(moveInstr->dst->head == moveInstr->src->head){
      return ;
    }
    else if(IsCalleeSaves(moveInstr->dst->head) || IsCalleeSaves(moveInstr->src->head)){
      return ;
    }
    else if(frameTM->Look(moveInstr->src->head) != nullptr){
      if(frameTM->Look(moveInstr->dst->head) != nullptr){
        emit(moveInstr);
      }
      else{
        if(inframe->Look(moveInstr->dst->head) == nullptr){
          f->length += F::addressSize;
          std::stringstream assemStream;
          assemStream << f->namedFrameLength() << "+" << -f->length << "(%rsp)";
          emit(new AS::MoveInstr("movq `s0, " + assemStream.str(), nullptr, new TEMP::TempList(moveInstr->src->head, nullptr)));
          inframe->Enter(moveInstr->dst->head, new std::string(assemStream.str()));
        }
        else{
          emit(new AS::MoveInstr("movq `s0, " + *inframe->Look(moveInstr->dst->head), nullptr, new TEMP::TempList(moveInstr->src->head, nullptr)));
        }
      }
      return ;
    }
    else if(frameTM->Look(moveInstr->dst->head) != nullptr){
      emit(new AS::MoveInstr("movq " + *inframe->Look(moveInstr->src->head) + ", `d0", new TEMP::TempList(moveInstr->dst->head, nullptr),nullptr));
    }
    else{
      emit(new AS::MoveInstr("movq " + *inframe->Look(moveInstr->src->head) + ", `d0", new TEMP::TempList(F::R10(), nullptr),nullptr));
      if(inframe->Look(moveInstr->dst->head) == nullptr){
        f->length += F::addressSize;
        std::stringstream assemStream;
        assemStream << f->namedFrameLength() << "+" << -f->length << "(%rsp)";
        emit(new AS::MoveInstr("movq `s0, " + assemStream.str(), nullptr, new TEMP::TempList(F::R10(), nullptr)));
        inframe->Enter(moveInstr->dst->head, new std::string(assemStream.str()));
      }
      else{
        emit(new AS::MoveInstr("movq `s0, " + *inframe->Look(moveInstr->dst->head), nullptr, new TEMP::TempList(F::R10(), nullptr)));
      }
    }
  }
  else if(moveInstr->assem.find("(") == -1 && moveInstr->assem.find("$") != -1){//movq $1, regA 只用于lab5
    if(frameTM->Look(moveInstr->dst->head) != nullptr){
      emit(moveInstr);
    }
    else{
      if(inframe->Look(moveInstr->dst->head) == nullptr){
        f->length += F::addressSize;
        std::stringstream assemStream;
        assemStream << f->namedFrameLength() << "+" << -f->length << "(%rsp)";
        emit(new AS::MoveInstr(moveInstr->assem, new TEMP::TempList(F::R10(), nullptr), nullptr));
        emit(new AS::MoveInstr("movq `s0, " + assemStream.str(), nullptr, new TEMP::TempList(F::R10(), nullptr)));
        inframe->Enter(moveInstr->dst->head, new std::string(assemStream.str()));
      }
      else{
        emit(new AS::MoveInstr(moveInstr->assem, new TEMP::TempList(F::R10(), nullptr), nullptr));
        emit(new AS::MoveInstr("movq `s0, " + *inframe->Look(moveInstr->dst->head), nullptr, new TEMP::TempList(F::R10(), nullptr)));
      }
    }
  }
  else if(moveInstr->assem.find("(") != -1 && moveInstr->assem.find("(") < moveInstr->assem.find(",") && moveInstr->assem.find("%rip") == -1){//movq 1(regB), regA
    // moveInstr->assem = "333333" + moveInstr->assem;
    // emit(moveInstr);
    if(IsCallerSaves(moveInstr->dst->head)){//只用于lab5
      if(frameTM->Look(moveInstr->src->head) != nullptr){
        emit(moveInstr);
      }
      else{
        emit(new AS::MoveInstr("movq " + *inframe->Look(moveInstr->src->head) + ", `d0", new TEMP::TempList(F::R10(), nullptr), nullptr));
        emit(new AS::MoveInstr(moveInstr->assem, moveInstr->dst, new TEMP::TempList(F::R10(), nullptr)));
      }
    }
    else{
      if(frameTM->Look(moveInstr->src->head) != nullptr){
        emit(new AS::MoveInstr(moveInstr->assem, new TEMP::TempList(F::R10(), nullptr), moveInstr->src));
        if(inframe->Look(moveInstr->dst->head) == nullptr){
          f->length += F::addressSize;
          std::stringstream assemStream;
          assemStream << f->namedFrameLength() << "+" << -f->length << "(%rsp)";
          emit(new AS::MoveInstr("movq `s0, " + assemStream.str(), nullptr, new TEMP::TempList(F::R10(), nullptr)));
          inframe->Enter(moveInstr->dst->head, new std::string(assemStream.str()));
        }
        else{
          emit(new AS::MoveInstr("movq `s0, " + *inframe->Look(moveInstr->dst->head), nullptr, new TEMP::TempList(F::R10(), nullptr)));
        }
      }
      else{
        emit(new AS::MoveInstr("movq " + *inframe->Look(moveInstr->src->head) + ", `d0", new TEMP::TempList(F::R10(), nullptr), nullptr));
        if(inframe->Look(moveInstr->dst->head) == nullptr){
          f->length += F::addressSize;
          std::stringstream assemStream;
          assemStream << f->namedFrameLength() << "+" << -f->length << "(%rsp)";
          emit(new AS::MoveInstr(moveInstr->assem, new TEMP::TempList(F::R11(), nullptr), new TEMP::TempList(F::R10(), nullptr)));
          emit(new AS::MoveInstr("movq `s0, " + assemStream.str(), nullptr, new TEMP::TempList(F::R11(), nullptr)));
          inframe->Enter(moveInstr->dst->head, new std::string(assemStream.str()));
        }
        else{
          emit(new AS::MoveInstr(moveInstr->assem, new TEMP::TempList(F::R11(), nullptr), new TEMP::TempList(F::R10(), nullptr)));
          emit(new AS::MoveInstr("movq `s0, " + *inframe->Look(moveInstr->dst->head), nullptr, new TEMP::TempList(F::R11(), nullptr)));
        }
      }
    }
  }
  else if(moveInstr->assem.find("%rip") != -1){
    if(frameTM->Look(moveInstr->dst->head) != nullptr){
      emit(moveInstr);
    }
    else{
      if(inframe->Look(moveInstr->dst->head) == nullptr){
        f->length += F::addressSize;
        std::stringstream assemStream;
        assemStream << f->namedFrameLength() << "+" << -f->length << "(%rsp)";
        emit(new AS::MoveInstr(moveInstr->assem, new TEMP::TempList(F::R10(), nullptr), nullptr));
        emit(new AS::MoveInstr("movq `s0, " + assemStream.str(), nullptr, new TEMP::TempList(F::R10(), nullptr)));
        inframe->Enter(moveInstr->dst->head, new std::string(assemStream.str()));
      }
      else{
        emit(new AS::MoveInstr(moveInstr->assem, new TEMP::TempList(F::R10(), nullptr), nullptr));
        emit(new AS::MoveInstr("movq `s0, " + *inframe->Look(moveInstr->dst->head), nullptr, new TEMP::TempList(F::R10(), nullptr)));
      }
    }
  }
  else{
    //TEMP::TempList *callersaves = F::CallerSaves();
    frameInstr(moveInstr->assem, moveInstr->dst, moveInstr->src, nullptr);
    //emit(moveInstr);
  }
}

void labelInstregAlloc(F::Frame* f, AS::LabelInstr *labelInstr){
  emit(labelInstr);
  return ;
}

void operInstregAlloc(F::Frame* f, AS::OperInstr *operInstr){
  frameInstr(operInstr->assem, operInstr->dst, operInstr->src, nullptr);
  //emit(operInstr);
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

bool IsCalleeSaves(TEMP::Temp *temp){
  static TEMP::Map *calleesavesMap = nullptr;
  if(calleesavesMap == nullptr){
    calleesavesMap = TEMP::Map::Empty();
    for(auto it = F::CalleeSaves(); it; it= it->tail){
      calleesavesMap->Enter(it->head, new std::string(""));
    }
  }

  if(calleesavesMap->Look(temp) == nullptr){
    return false;
  }
  else{
    return true;
  }
}

bool IsCallerSaves(TEMP::Temp *temp){
  static TEMP::Map *callersavesMap = nullptr;
  if(callersavesMap == nullptr){
    callersavesMap = TEMP::Map::Empty();
    for(auto it = F::CallerSaves(); it; it= it->tail){
      callersavesMap->Enter(it->head, new std::string(""));
    }
  }

  if(callersavesMap->Look(temp) == nullptr){
    return false;
  }
  else{
    return true;
  }
}

void frameInstr(std::string assem, TEMP::TempList* dst, TEMP::TempList* src, AS::Targets* jumps){
  TEMP::TempList *callersaves = F::CallerSaves();
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
          if(frameTM->Look(temp) == nullptr){
            emit(new AS::MoveInstr("movq " + *inframe->Look(temp) + ", `d0", new TEMP::TempList(callersaves->head, nullptr), nullptr));
            changeNthTemp(src, n, callersaves->head);
            callersaves = callersaves->tail;
          }
        } break;
        case 'd': {
          i++;
          int n = assem.at(i) - '0';
          dstTemp = nth_temp(dst, n);
          if(frameTM->Look(dstTemp) == nullptr){
            emit(new AS::MoveInstr("movq " + *inframe->Look(dstTemp) + ", `d0", new TEMP::TempList(callersaves->head, nullptr),nullptr));
            changeNthTemp(dst, n, callersaves->head);
            aternativeTemp = callersaves->head;
            callersaves = callersaves->tail;
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
    emit(new AS::MoveInstr("movq `s0, " + *inframe->Look(dstTemp), nullptr, new TEMP::TempList(aternativeTemp, nullptr)));
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