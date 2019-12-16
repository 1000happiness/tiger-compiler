#ifndef TIGER_CODEGEN_CODEGEN_H_
#define TIGER_CODEGEN_CODEGEN_H_

#include <sstream>

#include "tiger/codegen/assem.h"
#include "tiger/frame/frame.h"
#include "tiger/translate/tree.h"

namespace CG {

AS::InstrList* Codegen(F::Frame* f, T::StmList* stmList);

void munchStm(T::Stm *stm);
TEMP::Temp *munchExp(T::Exp *exp);
TEMP::TempList *munchArgs(int index, T::ExpList *args);

void emit(AS::Instr *instr);

}
#endif