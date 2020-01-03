#include "tiger/codegen/codegen.h"
//为了实现固定栈帧的结构，只有函数开头和结尾能够对%rsp的值进行操作，所有对栈帧的操作只能使用mov，
namespace CG {

using TL = TEMP::TempList;
int i = 0;

AS::InstrList* instrList = nullptr, *last = nullptr;
F::Frame *frame;
int maxFrameArgsNumber = 0;

void munchStm(T::Stm *stm);
TEMP::Temp *munchExp(T::Exp *exp);
TEMP::TempList *munchArgs(int index, T::ExpList *args);

void emit(AS::Instr *instr);

AS::InstrList* Codegen(F::Frame* f, T::StmList* stmList) {
  frame = f;
  instrList = nullptr;
  last = nullptr;
  maxFrameArgsNumber = 0;
  for(auto it = stmList; it; it = it->tail){
    munchStm(it->head);
  }
  frame->maxFrameArgsNumber += maxFrameArgsNumber;
  return F::F_procEntryExit2(instrList);
}

TL *munchArgs(int index, T::ExpList *args){
  if(args == nullptr) {
		return nullptr;
	}

  TL *argsReg = F::argsReg();
  
  bool inframe = true;
  TEMP::Temp *temp = munchExp(args->head);
  
  TL *it = argsReg;
  int i = 0;
  for(; it; it = it->tail){
    if(i == index){
      inframe = false;
      break;
    }
    i++;
  }
  
  if(inframe){
    int regNumber = 0;
    for(TL *i = argsReg; i; i= i->tail){
      regNumber++;
    }
    std::stringstream assemstream;
    assemstream << "movq `s0,"<< (index - regNumber) * F::addressSize << "(`s1)";
    emit(new AS::MoveInstr(assemstream.str(), nullptr, new TL(temp, new TL(F::SP(), nullptr))));
    if(index - regNumber + 1 > maxFrameArgsNumber){
      maxFrameArgsNumber = index - regNumber + 1;
    }
  }
  else{
    if(temp == F::FP()){
      emit(new AS::MoveInstr("movq `s0, `d0", new TL(it->head, nullptr), new TL(F::SP(), nullptr)));
      emit(new AS::OperInstr(std::string("addq $") + frame->namedFrameLength() + std::string("+, `d0"), new TL(it->head, nullptr), new TL(it->head, nullptr), nullptr));
    }
    else{
      emit(new AS::MoveInstr("movq `s0, `d0", new TL(it->head, nullptr), new TL(temp, nullptr)));
    }
    temp = it->head;
  }
  TL *next = munchArgs(index + 1, args->tail);
  return new TL(temp, next);
}

TEMP::Temp *munchExp(T::Exp *exp) {
  if(exp->kind == T::Exp::MEM){//三个节点
    T::MemExp *menExp = (T::MemExp *)exp;
    if(menExp->exp->kind == T::Exp::BINOP){
      T::BinopExp *binoExp = (T::BinopExp *)menExp->exp;
      if(binoExp->op == T::BinOp::PLUS_OP && binoExp->right->kind == T::Exp::CONST){
        TEMP::Temp *newTemp = TEMP::Temp::NewTemp();
        TEMP::Temp *temp = munchExp(binoExp->left);
        int consti = ((T::ConstExp *)(binoExp->right))->consti;
        if(temp == F::FP()){//帧指针替换
          std::stringstream assemstream;
          assemstream << "movq "<< frame->namedFrameLength() << "+" << consti << "(`s0), `d0";
          emit(new AS::MoveInstr(assemstream.str(), new TL(newTemp, nullptr), new TL(F::SP(), nullptr)));
        }
        else{
          std::stringstream assemstream;
          assemstream << "movq " << consti << "(`s0), `d0";
          emit(new AS::MoveInstr(assemstream.str(), new TL(newTemp, nullptr), new TL(temp, nullptr)));
        }
        return newTemp;
      }
      else if(binoExp->op == T::BinOp::PLUS_OP && binoExp->left->kind == T::Exp::CONST){
        TEMP::Temp *newTemp = TEMP::Temp::NewTemp();
        TEMP::Temp *temp = munchExp(binoExp->right);
        int consti = ((T::ConstExp *)(binoExp->left))->consti;
        if(temp == F::FP()){//帧指针替换
          std::stringstream assemstream;
          assemstream << "movq "<< frame->namedFrameLength() << "+" << consti << "(`s0), `d0";
          emit(new AS::MoveInstr(assemstream.str(), new TL(newTemp, nullptr), new TL(F::SP(), nullptr)));
        }
        else{
          std::stringstream assemstream;
          assemstream << "movq " << consti << "(`s0), `d0";
          emit(new AS::MoveInstr(assemstream.str(), new TL(newTemp, nullptr), new TL(temp, nullptr)));
        }
        return newTemp;
      }
      else{
        TEMP::Temp *srcTemp = munchExp(binoExp);
        TEMP::Temp *dstTemp = TEMP::Temp::NewTemp();
        emit(new AS::MoveInstr("movq (`s0), `d0", new TL(dstTemp, nullptr), new TL(srcTemp, nullptr)));
        return dstTemp;
      }
    }
    else if(menExp->kind == T::Exp::CONST){
      T::ConstExp *constExp = ((T::ConstExp *)menExp->exp);
      TEMP::Temp *newTemp = TEMP::Temp::NewTemp();
      int consti = constExp->consti;
      std::stringstream assemstream;
      assemstream << "movq (" << consti << "), `d0";
      emit(new AS::MoveInstr(assemstream.str(), new TL(newTemp, nullptr), nullptr));
      return newTemp;
    }
    else {
      TEMP::Temp *newTemp = TEMP::Temp::NewTemp();
      TEMP::Temp *temp = munchExp(menExp->exp);
      if(temp == F::FP()){//帧指针替换
        std::stringstream assemstream;
        assemstream << "movq "<< frame->namedFrameLength() << "(`s0), `d0";
        emit(new AS::MoveInstr(assemstream.str(), new TL(newTemp, nullptr), new TL(F::SP(), nullptr)));
      }
      else{
        emit(new AS::MoveInstr("movq (`s0), `d0", new TL(newTemp, nullptr), new TL(temp, nullptr)));
      }
      
      return newTemp;
    }
  }
  else if(exp->kind == T::Exp::BINOP){//两个节点 
    T::BinopExp *binopExp = (T::BinopExp *)exp;
    std::string opString;
    switch(binopExp->op) {
      case T::BinOp::PLUS_OP: 
        opString = "addq";
        break;
      case T::BinOp::MINUS_OP:
        opString = "subq";
        break;
      case T::BinOp::MUL_OP: 
        opString = "imulq";
        break;
      case T::BinOp::DIV_OP:
        opString = "idivq";
        break;
      default: {
        break;
      }
    }

    TEMP::Temp *leftTemp = nullptr;
    TEMP::Temp *rightTemp = nullptr;
    TEMP::Temp *returnTemp = nullptr;
    switch(binopExp->op) {
      case T::BinOp::PLUS_OP: 
      case T::BinOp::MINUS_OP:
      case T::BinOp::MUL_OP:
        returnTemp = TEMP::Temp::NewTemp();
        leftTemp = munchExp(binopExp->left);
        if(binopExp->right->kind == T::Exp::CONST){
          int consti = ((T::ConstExp *)binopExp->right)->consti;
          if(leftTemp == F::FP()){//帧指针替换，对帧指针的操作只有加法，对帧指针的加法运算相当于对栈指针的加法运算
            emit(new AS::MoveInstr("movq `s0, `d0", new TL(returnTemp, nullptr), new TL(F::SP(), nullptr)));
            emit(new AS::OperInstr("addq $" + frame->namedFrameLength() + "+, `d0", new TL(returnTemp, nullptr), new TL(returnTemp, nullptr), nullptr));
            std::stringstream assemstream;
            assemstream << opString<< " $" << consti << ", `d0";
            emit(new AS::OperInstr(assemstream.str(), new TL(returnTemp, nullptr), new TL(returnTemp, nullptr), nullptr));
          }
          else{
            emit(new AS::MoveInstr("movq `s0, `d0", new TL(returnTemp, nullptr), new TL(leftTemp, nullptr)));
            std::stringstream assemstream;
            assemstream << opString<< " $" << consti << ", `d0";
            emit(new AS::OperInstr(assemstream.str(), new TL(returnTemp, nullptr), new TL(returnTemp, nullptr), nullptr));
          }
        }
        else{
          rightTemp = munchExp(binopExp->right);
          if(leftTemp == F::FP()){//帧指针替换，对帧指针的操作只有加法，对帧指针的加法运算相当于对栈指针的加法运算
            emit(new AS::MoveInstr("movq `s0, `d0", new TL(returnTemp, nullptr), new TL(F::SP(), nullptr)));
            emit(new AS::OperInstr("addq $" + frame->namedFrameLength() + "+, `d0", new TL(returnTemp, nullptr), new TL(returnTemp, nullptr), nullptr));
            emit(new AS::OperInstr(opString + " `s1, `d0", new TL(F::SP(), nullptr), new TL(F::SP(), new TL(rightTemp, nullptr)), nullptr));
          }
          else{
            emit(new AS::MoveInstr("movq `s0, `d0", new TL(returnTemp, nullptr), new TL(leftTemp, nullptr)));
            emit(new AS::OperInstr(opString + " `s1, `d0", new TL(returnTemp, nullptr), new TL(returnTemp, new TL(rightTemp, nullptr)), nullptr));
          }
        }
        break;
      case T::BinOp::DIV_OP:
        leftTemp = munchExp(binopExp->left);
        rightTemp = munchExp(binopExp->right);
        emit(new AS::MoveInstr("movq `s0, `d0", new TL(F::RV(), nullptr), new TL(leftTemp, nullptr)));
        returnTemp = F::RV();
        emit(new AS::OperInstr("cqto", nullptr, nullptr, nullptr));
        emit(new AS::OperInstr(opString + " `s0", new TL(F::RV(), new TL(F::RD(), nullptr)), new TL(rightTemp, new TL(F::RV(), nullptr)), nullptr));
        break;
      default: {
        break;
      }
    }
    return returnTemp;
  }
  else if(exp->kind == T::Exp::CONST){//一个节点
    T::ConstExp *constExp = ((T::ConstExp *)exp);
    TEMP::Temp *newTemp = TEMP::Temp::NewTemp();
    int consti = constExp->consti;
    std::stringstream assemstream;
    assemstream << "movq $" << consti << ", `d0";
    emit(new AS::MoveInstr(assemstream.str(), new TL(newTemp, nullptr), nullptr));
    return newTemp;
  }
  else if(exp->kind == T::Exp::TEMP){//一个节点
    T::TempExp *tempExp = ((T::TempExp *)exp); 
    return tempExp->temp;
  }
  else if(exp->kind == T::Exp::CALL){//一个节点
    

    T::CallExp *callExp = (T::CallExp *)exp;
    T::NameExp *nameExp = (T::NameExp *)callExp->fun;
    TL *argsTL = munchArgs(0, callExp->args);
    std::stringstream assemstream;
    assemstream << "callq " << nameExp->name->Name();
    emit(new AS::OperInstr(assemstream.str(), new TL(F::RV(), F::CallerSaves()), F::argsReg(), nullptr));
    return F::RV();
  }
  else if(exp->kind == T::Exp::NAME){//一个节点
    T::NameExp *nameExp = ((T::NameExp *)exp);
    TEMP::Temp *newTemp = TEMP::Temp::NewTemp();
    TEMP::Label *label = nameExp->name;
    std::stringstream assemstream;
    assemstream << "leaq " << label->Name() << "(%rip), `d0";
    emit(new AS::MoveInstr(assemstream.str(), new TL(newTemp, nullptr), nullptr));
    return newTemp;
  }
}

void munchStm(T::Stm *stm){
  if(stm->kind == T::Stm::MOVE) {
    //prntf("movestm\n");
    //fflush(stdout);
    T::MoveStm *moveStm = (T::MoveStm *)stm;
    T::Exp *src = moveStm->src;
    T::Exp *dst = moveStm->dst;
    if(dst->kind == T::Exp::MEM) {
      T::MemExp *menExp = (T::MemExp *)dst;
      if(menExp->exp->kind == T::Exp::BINOP){
        T::BinopExp *binoExp = (T::BinopExp *)menExp->exp;
        if(binoExp->op == T::BinOp::PLUS_OP && binoExp->right->kind == T::Exp::CONST){
          TEMP::Temp *temp = munchExp(binoExp->left);
          int consti = ((T::ConstExp *)(binoExp->right))->consti;
          if(src->kind == T::Exp::CONST){
            int constsrc = ((T::ConstExp *)(src))->consti;
            if(temp == F::FP()){//帧指针替换
              std::stringstream assemstream;
              assemstream << "movq $"<< constsrc<< ", " << frame->namedFrameLength() << "+" << consti << "(`s0)";
              emit(new AS::MoveInstr(assemstream.str(), nullptr, new TL(F::SP(), nullptr)));
            }
            else{
              std::stringstream assemstream;
              assemstream << "movq $"<< constsrc<< ", " << consti << "(`s0)";
              emit(new AS::MoveInstr(assemstream.str(), nullptr, new TL(temp, nullptr)));
            }
          }
          else{
            TEMP::Temp *srcTemp = munchExp(src);
            if(temp == F::FP()){//帧指针替换
              std::stringstream assemstream;
              assemstream << "movq `s0, " << frame->namedFrameLength() << "+"<< consti << "(`s1)";
              emit(new AS::MoveInstr(assemstream.str(), nullptr, new TL(srcTemp, new TL(F::SP(), nullptr))));
            }
            else{
              std::stringstream assemstream;
              assemstream << "movq `s0, " << consti << "(`s1)";
              emit(new AS::MoveInstr(assemstream.str(), nullptr, new TL(srcTemp, new TL(temp, nullptr))));
            }
          }
        }
        else if(binoExp->op == T::BinOp::PLUS_OP && binoExp->left->kind == T::Exp::CONST){
          TEMP::Temp *temp = munchExp(binoExp->right);
          int consti = ((T::ConstExp *)(binoExp->left))->consti;
          if(src->kind == T::Exp::CONST){
            int constsrc = ((T::ConstExp *)(src))->consti;
            if(temp == F::FP()){//帧指针替换
              std::stringstream assemstream;
              assemstream << "movq $"<< constsrc<< ", " << frame->namedFrameLength() << "+" << consti << "(`s0)";
              emit(new AS::MoveInstr(assemstream.str(), nullptr, new TL(F::SP(), nullptr)));
            }
            else{
              std::stringstream assemstream;
              assemstream << "movq $"<< constsrc<< ", " << consti << "(`s0)";
              emit(new AS::MoveInstr(assemstream.str(), nullptr, new TL(temp, nullptr)));
            }
          }
          else{
            TEMP::Temp *srcTemp = munchExp(src);
            if(temp == F::FP()){//帧指针替换
              std::stringstream assemstream;
              assemstream << "movq `s0, " << frame->namedFrameLength() << "+" << consti << "(`s1)";
              emit(new AS::MoveInstr(assemstream.str(), nullptr, new TL(srcTemp, new TL(F::SP(), nullptr))));
            }
            else{
              std::stringstream assemstream;
              assemstream << "movq `s0, " << consti << "(`s1)";
              emit(new AS::MoveInstr(assemstream.str(), nullptr, new TL(srcTemp, new TL(temp, nullptr))));
            }
          }
        }
        else{
          TEMP::Temp *temp = munchExp(binoExp);
          TEMP::Temp *srcTemp = munchExp(src);
          emit(new AS::MoveInstr("movq `s0, (`s1)", nullptr, new TL(srcTemp, new TL(temp, nullptr))));
        }
      }
      else if(menExp->exp->kind == T::Exp::CONST){
        T::ConstExp *constExp = ((T::ConstExp *)menExp->exp);
        int consti = constExp->consti;
        if(src->kind == T::Exp::CONST){
          int constsrc = ((T::ConstExp *)(src))->consti;
          std::stringstream assemstream;
          assemstream << "movq $"<< constsrc <<", (" << consti << ")";
          emit(new AS::MoveInstr(assemstream.str(), nullptr, nullptr));
        }
        else{
          TEMP::Temp *srcTemp = munchExp(src);
          std::stringstream assemstream;
          assemstream << "movq `s0, (" << consti << ")";
          emit(new AS::MoveInstr(assemstream.str(), nullptr, new TL(srcTemp, nullptr)));
        }
      }
      else {
        TEMP::Temp *dstTemp = munchExp(menExp->exp);
        if(src->kind == T::Exp::CONST){
          int constsrc = ((T::ConstExp *)(src))->consti;
          if(dstTemp == F::FP()){//帧指针替换
            std::stringstream assemstream;
            assemstream << "movq $"<< constsrc <<", "<< frame->namedFrameLength() << "(`s0)";
            emit(new AS::OperInstr(assemstream.str(), nullptr, new TL(F::SP(), nullptr), nullptr));
          }
          else{
            std::stringstream assemstream;
            assemstream << "movq $"<< constsrc <<", (`s0)";
            emit(new AS::OperInstr(assemstream.str(), nullptr, new TL(dstTemp, nullptr), nullptr));
          }
        }
        else{
          TEMP::Temp *srcTemp = munchExp(src);
          if(srcTemp != dstTemp){
            if(dstTemp == F::FP()){//帧指针替换
              std::stringstream assemstream;
              assemstream << "movq `s0, "<< frame->namedFrameLength() << "(`s1)";
              emit(new AS::OperInstr("movq `s0, (`s1)", nullptr, new TL(srcTemp, new TL(F::SP(), nullptr)), nullptr));
            }
            else{
              emit(new AS::OperInstr("movq `s0, (`s1)", nullptr, new TL(srcTemp, new TL(dstTemp, nullptr)), nullptr));
            }
          }
        }
      }
    }
    else if(dst->kind == T::Exp::TEMP) {
      TEMP::Temp *dstTemp = munchExp(dst);
      if(src->kind == T::Exp::CONST){
        int constsrc = ((T::ConstExp *)(src))->consti;
        std::stringstream assemstream;
        assemstream << "movq $"<< constsrc <<", `d0";
        emit(new AS::MoveInstr(assemstream.str(), new TL(dstTemp, nullptr), nullptr));
      }
      else{
        TEMP::Temp *srcTemp = munchExp(src);
        if(srcTemp == F::FP()){
          emit(new AS::MoveInstr("movq `s0, `d0", new TL(dstTemp, nullptr), new TL(F::SP(), nullptr)));
          std::stringstream assemstream;
          assemstream<< "addq $" << frame->namedFrameLength() << "+" << ", `s0";
          emit(new AS::OperInstr(assemstream.str(), new TL(dstTemp, nullptr), new TL(dstTemp, nullptr), nullptr));
        }
        else{
          emit(new AS::MoveInstr("movq `s0, `d0", new TL(dstTemp, nullptr), new TL(srcTemp, nullptr)));
        }
      }
    }
  }
  else if(stm->kind == T::Stm::JUMP) {
    T::JumpStm *jumpStm = (T::JumpStm *)stm;
    std::stringstream assemstream;
    assemstream << "jmp " << jumpStm->exp->name->Name();
    emit(new AS::OperInstr(assemstream.str(), nullptr, nullptr, new AS::Targets(jumpStm->jumps)));
  }
  else if(stm->kind == T::Stm::CJUMP) {
    T::CjumpStm *cjumpStm = (T::CjumpStm *)stm;
    TEMP::Temp *leftTemp = munchExp(cjumpStm->left);
    TEMP::Temp *rightTemp = munchExp(cjumpStm->right);
    std::string jmpqString;
    switch(cjumpStm->op) {
      case T::RelOp::EQ_OP: 
        jmpqString = "je";
        break;
      case T::RelOp::NE_OP: 
        jmpqString = "jne";
        break;
      case T::RelOp::LT_OP: 
        jmpqString = "jl";
        break;
      case T::RelOp::GT_OP: 
        jmpqString = "jg";
        break;
      case T::RelOp::LE_OP: 
        jmpqString = "jle";
        break;
      case T::RelOp::GE_OP: 
        jmpqString = "jge";
        break;
      default: 
        break;
    }
    emit(new AS::OperInstr("cmpq `s0, `s1", nullptr, new TL(rightTemp, new TL(leftTemp, nullptr)), nullptr));
    std::stringstream assemstream;
    assemstream << jmpqString << " " << cjumpStm->true_label->Name();
    emit(new AS::OperInstr(assemstream.str(), nullptr, nullptr, 
      new AS::Targets(new TEMP::LabelList(cjumpStm->true_label, new TEMP::LabelList(cjumpStm->false_label, nullptr))))
    );
  }
  else if(stm->kind == T::Stm::EXP) {
    //prntf("expstm\n");
    //fflush(stdout);
    T::ExpStm* expStm = (T::ExpStm *)stm;
    munchExp(expStm->exp);
  }
  else if(stm->kind == T::Stm::LABEL) {
    T::LabelStm* labelStm = (T::LabelStm *)stm;
    emit(new AS::LabelInstr(labelStm->label->Name(), labelStm->label));
  }
}

void emit(AS::Instr *instr){
  if(last == nullptr){
    instrList = new AS::InstrList(instr, nullptr);
    last = instrList;
  }
  else{
    last->tail = new AS::InstrList(instr, nullptr);
    last = last->tail;
  }
}

}  // namespace CG