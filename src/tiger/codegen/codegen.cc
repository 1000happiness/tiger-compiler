#include "tiger/codegen/codegen.h"

namespace CG {

using TL = TEMP::TempList;

AS::InstrList* instrList = nullptr;

void emit(AS::Instr *instr){
  instrList = new AS::InstrList(instr, instrList);
}

AS::InstrList* Codegen(F::Frame* f, T::StmList* stmList) {
  for(auto it = stmList; it; it = it->tail){
    //prntf("stm\n");
    munchStm(it->head);
  }
  return F::F_procEntryExit2(instrList);
}

TL *munchArgs(int index, T::ExpList *args){
  if(args == NULL) {
		return NULL;
	}
	munchArgs(index + 1, args->tail);
	TEMP::Temp *temp = munchExp(args->head);
  TL *next = munchArgs(index++, args->tail);
  TL *argsReg = F::argsReg();
  
  int i = 0;
  bool inframe = true;
  TL *it = argsReg;
  for(; it; it = it->tail){
    if(i == index){
      inframe = false;
      break;
    }
    i++;
  }

  if(inframe){
    emit(new AS::OperInstr("pushq `s0", new TL(F::SP(), nullptr), new TL(temp, new TL(F::SP(), nullptr)), nullptr));
    return next;
  }
  else{
    if(temp != it->head){
      emit(new AS::MoveInstr("movq `s0 `d0", new TL(it->head, nullptr), new TL(temp, nullptr)));
      temp = it->head;
    }
    return new TL(temp, next);
  }
}

TEMP::Temp *munchExp(T::Exp *exp) {
  if(exp->kind == T::Exp::MEM){//三个节点
    T::MemExp *menExp = (T::MemExp *)exp;
    if(menExp->kind == T::Exp::BINOP){
      T::BinopExp *binoExp = (T::BinopExp *)menExp->exp;
      if(binoExp->op == T::BinOp::PLUS_OP && binoExp->right->kind == T::Exp::CONST){
        TEMP::Temp *newTemp = TEMP::Temp::NewTemp();
        TEMP::Temp *temp = munchExp(binoExp->left);
        int consti = ((T::ConstExp *)(binoExp->right))->consti;
        std::stringstream assemstream;
        assemstream << "movq " << consti << "(`s0), `d0";
        emit(new AS::OperInstr(assemstream.str(), new TL(newTemp, nullptr), new TL(temp, nullptr), nullptr));
        return newTemp;
      }
      else if(binoExp->op == T::BinOp::PLUS_OP && binoExp->left->kind == T::Exp::CONST){
        TEMP::Temp *newTemp = TEMP::Temp::NewTemp();
        TEMP::Temp *temp = munchExp(binoExp->right);
        int consti = ((T::ConstExp *)(binoExp->left))->consti;
        std::stringstream assemstream;
        assemstream << "movq " << consti << "(`s0), `d0";
        emit(new AS::OperInstr(assemstream.str(), new TL(newTemp, nullptr), new TL(temp, nullptr), nullptr));
        return newTemp;
      }
    }
    else if(menExp->kind == T::Exp::CONST){
      T::ConstExp *constExp = ((T::ConstExp *)menExp->exp);
      TEMP::Temp *newTemp = TEMP::Temp::NewTemp();
      int consti = constExp->consti;
      std::stringstream assemstream;
      assemstream << "movq " << consti << "(0), `d0";
      emit(new AS::OperInstr(assemstream.str(), new TL(newTemp, nullptr), nullptr, nullptr));
      return newTemp;
    }
    else {
      TEMP::Temp *newTemp = TEMP::Temp::NewTemp();
      TEMP::Temp *temp = munchExp(menExp->exp);
      emit(new AS::MoveInstr("movq `s0, `d0", new TL(newTemp, nullptr), new TL(temp, nullptr)));
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
    switch(binopExp->op) {
      case T::BinOp::PLUS_OP: 
      case T::BinOp::MINUS_OP:
      case T::BinOp::MUL_OP:
        leftTemp = munchExp(binopExp->left);
        if(binopExp->right->kind == T::Exp::CONST){
          emit(new AS::OperInstr(opString + " `s0, `d0", new TL(leftTemp, nullptr), new TL(leftTemp, nullptr), nullptr));
        }
        else{
          rightTemp = munchExp(binopExp->right);
          emit(new AS::OperInstr(opString + " `s0, `d0", new TL(leftTemp, nullptr), new TL(leftTemp, new TL(rightTemp, nullptr)), nullptr));
        }
        break;
      case T::BinOp::DIV_OP:
        leftTemp = munchExp(binopExp->left);
        rightTemp = munchExp(binopExp->right);
        if(leftTemp != F::RV()){
          emit(new AS::MoveInstr("movq `s0 `d0", new TL(F::RV(), nullptr), new TL(leftTemp, nullptr)));
          leftTemp = F::RV();
        }
        emit(new AS::OperInstr(opString + " `s0", new TL(F::RV(), new TL(F::RD(), nullptr)), new TL(rightTemp, new TL(F::RV(), nullptr)), nullptr));
        break;
      default: {
        break;
      }
    }
    return leftTemp;
  }
  else if(exp->kind == T::Exp::CONST){//一个节点
    T::ConstExp *constExp = ((T::ConstExp *)exp);
    TEMP::Temp *newTemp = TEMP::Temp::NewTemp();
    int consti = constExp->consti;
    std::stringstream assemstream;
    assemstream << "movq $" << consti << ", `d0";
    emit(new AS::OperInstr(assemstream.str(), new TL(newTemp, nullptr), nullptr, nullptr));
    return newTemp;
  }
  else if(exp->kind == T::Exp::TEMP){//一个节点
    T::TempExp *tempExp = ((T::TempExp *)exp);
    return tempExp->temp;
  }
  else if(exp->kind == T::Exp::CALL){//一个节点
    //prntf("callexp\n");
    //fflush(stdout);
    T::CallExp *callExp = (T::CallExp *)exp;
    T::NameExp *nameExp = (T::NameExp *)callExp->fun;
    TL *argsTL = munchArgs(0, callExp->args);
    std::stringstream assemstream;
    assemstream << "call " << nameExp->name->Name();
    emit(new AS::OperInstr(assemstream.str(), new TL(F::RV(), F::CallerSaved()), argsTL, nullptr));
    return F::RV();
  }
  else if(exp->kind == T::Exp::NAME){//一个节点
    T::NameExp *nameExp = ((T::NameExp *)exp);
    TEMP::Temp *newTemp = TEMP::Temp::NewTemp();
    TEMP::Label *label = nameExp->name;
    std::stringstream assemstream;
    assemstream << "movq $." << label->Name() << "`d0";
    emit(new AS::OperInstr(assemstream.str(), new TL(newTemp, nullptr), nullptr, nullptr));
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
        TEMP::Temp *srcTemp = munchExp(src);
        T::BinopExp *binoExp = (T::BinopExp *)menExp->exp;
        if(binoExp->op == T::BinOp::PLUS_OP && binoExp->right->kind == T::Exp::CONST){
          TEMP::Temp *temp = munchExp(binoExp->left);
          int consti = ((T::ConstExp *)(binoExp->right))->consti;
          if(src->kind == T::Exp::CONST){
            int constsrc = ((T::ConstExp *)(src))->consti;
            std::stringstream assemstream;
            assemstream << "movq $"<< constsrc<< ", " << consti << "(`s1)";
            emit(new AS::OperInstr(assemstream.str(), nullptr, new TL(temp, nullptr), nullptr));
          }
          else{
            TEMP::Temp *srcTemp = munchExp(src);
            std::stringstream assemstream;
            assemstream << "movq `s0," << consti << "(`s1)";
            emit(new AS::OperInstr(assemstream.str(), nullptr, new TL(srcTemp, new TL(temp, nullptr)), nullptr));
          }
        }
        else if(binoExp->op == T::BinOp::PLUS_OP && binoExp->left->kind == T::Exp::CONST){
          TEMP::Temp *temp = munchExp(binoExp->right);
          int consti = ((T::ConstExp *)(binoExp->left))->consti;
          if(src->kind == T::Exp::CONST){
            int constsrc = ((T::ConstExp *)(src))->consti;
            std::stringstream assemstream;
            assemstream << "movq $"<< constsrc<< ", " << consti << "(`s1)";
            emit(new AS::OperInstr(assemstream.str(), nullptr, new TL(temp, nullptr), nullptr));
          }
          else{
            TEMP::Temp *srcTemp = munchExp(src);
            std::stringstream assemstream;
            assemstream << "movq `s0," << consti << "(`s1)";
            emit(new AS::OperInstr(assemstream.str(), nullptr, new TL(srcTemp, new TL(temp, nullptr)), nullptr));
          }
        }
      }
      else if(menExp->exp->kind == T::Exp::CONST){
        T::ConstExp *constExp = ((T::ConstExp *)menExp->exp);
        int consti = constExp->consti;
        if(src->kind == T::Exp::CONST){
          int constsrc = ((T::ConstExp *)(src))->consti;
          std::stringstream assemstream;
          assemstream << "movq $"<< constsrc <<", (" << consti << ")";
          emit(new AS::OperInstr(assemstream.str(), nullptr, nullptr, nullptr));
        }
        else{
          TEMP::Temp *srcTemp = munchExp(src);
          std::stringstream assemstream;
          assemstream << "movq `s0, (" << consti << ")";
          emit(new AS::OperInstr(assemstream.str(), nullptr, new TL(srcTemp, nullptr), nullptr));
        }
      }
      else {
        TEMP::Temp *srcTemp = munchExp(src);
        TEMP::Temp *dstTemp = munchExp(menExp);
        emit(new AS::MoveInstr("movq `s0, `s1", new TL(srcTemp, nullptr), new TL(dstTemp, nullptr)));
      }
    }
    else if(dst->kind == T::Exp::TEMP) {
      TEMP::Temp *srcTemp = munchExp(src);
      TEMP::Temp *dstTemp = munchExp(dst);
      emit(new AS::MoveInstr("movq `s0, `s1", new TL(srcTemp, nullptr), new TL(dstTemp, nullptr)));
    }
  }
  else if(stm->kind == T::Stm::JUMP) {
    T::JumpStm *jumpStm = (T::JumpStm *)stm;
    std::stringstream assemstream;
    assemstream << "jmp " << jumpStm->exp->name->Name();
    emit(new AS::OperInstr(assemstream.str(), nullptr, nullptr, nullptr));
  }
  else if(stm->kind == T::Stm::CJUMP) {
    T::CjumpStm *cjumpStm = (T::CjumpStm *)stm;
    TEMP::Temp *leftTemp = munchExp(cjumpStm->left);
    TEMP::Temp *rightTemp = munchExp(cjumpStm->right);
    std::string jmpString;
    switch(cjumpStm->op) {
      case T::RelOp::EQ_OP: 
        jmpString = "je";
        break;
      case T::RelOp::NE_OP: 
        jmpString = "jne";
        break;
      case T::RelOp::LT_OP: 
        jmpString = "jl";
        break;
      case T::RelOp::GT_OP: 
        jmpString = "jg";
        break;
      case T::RelOp::LE_OP: 
        jmpString = "jle";
        break;
      case T::RelOp::GE_OP: 
        jmpString = "jge";
        break;
      default: 
        break;
    }
    emit(new AS::OperInstr("cmpq `s0, `s1", nullptr, new TL(leftTemp, new TL(rightTemp, nullptr)), nullptr));
    std::stringstream assemstream;
    assemstream << jmpString << cjumpStm->true_label->Name();
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

}  // namespace CG