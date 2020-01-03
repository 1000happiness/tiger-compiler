#include "tiger/escape/escape.h"
#include "tiger/util/table.h"

namespace ESC {

void FindAExpEscape(A::Exp *exp, int depth);
void FindADecEscape(A::Dec *dec, int depth);
void FindAVarEscape(A::Var *var, int depth);

class EscapeEntry {
 public:
  int depth;
  bool* escape;
  bool changed;

  EscapeEntry(int depth, bool* escape) : depth(depth), escape(escape), changed(false) {}
};

S::Table<EscapeEntry> *escenv = nullptr;

void FindEscape(A::Exp* exp) {
  escenv = new S::Table<EscapeEntry>();
  FindAExpEscape(exp, 0);
}

void FindAExpEscape(A::Exp *exp, int depth){
  switch (exp->kind)
  {
  case A::Exp::VAR:{
    FindAVarEscape(((A::VarExp*)exp)->var, depth);
  } break;
  case A::Exp::NIL:{
    /*empty*/
  } break;
  case A::Exp::INT:{
    /*empty*/
  } break;
  case A::Exp::STRING:{
    /*empty*/
  } break;
  case A::Exp::CALL:{
    for(auto it = ((A::CallExp*)exp)->args; it; it = it->tail){
      FindAExpEscape(it->head, depth);
    }
  } break;
  case A::Exp::OP:{
    FindAExpEscape(((A::OpExp*)exp)->left, depth);
    FindAExpEscape(((A::OpExp*)exp)->right, depth);
  } break;
  case A::Exp::RECORD:{
    for(auto it = ((A::RecordExp *)exp)->fields; it; it = it->tail){
      FindAExpEscape(it->head->exp, depth);
    }
  } break;
  case A::Exp::SEQ:{
    for(auto it = ((A::SeqExp *)exp)->seq; it; it= it->tail){
      FindAExpEscape(it->head, depth);
    }
  } break;
  case A::Exp::ASSIGN:{
    FindAVarEscape(((A::AssignExp *)exp)->var, depth);
    FindAExpEscape(((A::AssignExp *)exp)->exp, depth);
  } break;
  case A::Exp::IF:{
    FindAExpEscape(((A::IfExp *)exp)->test, depth);
    FindAExpEscape(((A::IfExp *)exp)->then, depth);
    if(((A::IfExp *)exp)->elsee != nullptr){
      FindAExpEscape(((A::IfExp *)exp)->elsee, depth);
    }
  } break;
  case A::Exp::WHILE:{
    escenv->BeginScope();
    FindAExpEscape(((A::WhileExp *)exp)->test, depth);
    FindAExpEscape(((A::WhileExp *)exp)->body, depth);
    escenv->EndScope();
  } break;
  case A::Exp::FOR:{
    escenv->BeginScope();
    EscapeEntry *tempEntry = escenv->Look(((A::ForExp *)exp)->var);
    ((A::ForExp *)exp)->escape = false;
    if(tempEntry != nullptr){
      escenv->Set(((A::ForExp *)exp)->var, new EscapeEntry(depth, &(((A::ForExp *)exp)->escape)));
    }
    else{
      escenv->Enter(((A::ForExp *)exp)->var, new EscapeEntry(depth, &(((A::ForExp *)exp)->escape)));
    }
    FindAExpEscape(((A::ForExp *)exp)->lo, depth);
    FindAExpEscape(((A::ForExp *)exp)->hi, depth);
    FindAExpEscape(((A::ForExp *)exp)->body, depth);
    if(tempEntry != nullptr){
      escenv->Set(((A::ForExp *)exp)->var, tempEntry);
    }
    escenv->EndScope();
  } break;
  case A::Exp::BREAK:{
    /*empty*/
  } break;
  case A::Exp::LET:{
    escenv->BeginScope();
    for(auto it = ((A::LetExp*)exp)->decs; it; it = it->tail){
      FindADecEscape(it->head, depth);
    }
    FindAExpEscape(((A::LetExp*)exp)->body, depth);
    escenv->EndScope();
  } break;
  case A::Exp::ARRAY:{
    FindAExpEscape(((A::ArrayExp *)exp)->size, depth);
  } break;
  case A::Exp::VOID:{
    /*empty*/
  } break;
  default:
    break;
  }
}

void FindADecEscape(A::Dec *dec, int depth){
  switch (dec->kind)
  {
  case A::Dec::FUNCTION:{
    for(auto it = ((A::FunctionDec *)dec)->functions; it; it = it->tail){
      escenv->BeginScope();
      for(auto funParam = it->head->params; funParam; funParam=funParam->tail){
        funParam->head->escape = false;
        escenv->Enter(funParam->head->name, new EscapeEntry(depth + 1, &funParam->head->escape));
      }
      FindAExpEscape(it->head->body, depth + 1);
      escenv->EndScope();
    }
  } break;
  case A::Dec::VAR:{
    FindAExpEscape(((A::VarDec *)dec)->init, depth);
    ((A::VarDec *)dec)->escape = false;
    escenv->Enter(((A::VarDec *)dec)->var, new EscapeEntry(depth, &((A::VarDec *)dec)->escape));
  } break;
  case A::Dec::TYPE:{
    /*empty*/
  } break;
  default:
    break;
  }
}

void FindAVarEscape(A::Var *var, int depth){
  switch (var->kind)
  {
  case A::Var::SIMPLE:{
    if(((A::SimpleVar *)var)->sym == nullptr){
      printf("error\n");
      fflush(stdout);
    }
    EscapeEntry *escapeEntry = escenv->Look(((A::SimpleVar *)var)->sym);
    if(escapeEntry == nullptr){
      printf("null %s\n", ((A::SimpleVar *)var)->sym->Name().c_str());
    }
    if(depth != escapeEntry->depth){
      escapeEntry->changed = true;
      *(escapeEntry->escape) = true;
    }
    else{
      if(!escapeEntry->changed){
        *(escapeEntry->escape) = false;
      }
    }
  } break;
  case A::Var::FIELD:{
    FindAVarEscape(((A::FieldVar *)var)->var, depth);
  } break;
  case A::Var::SUBSCRIPT:{
    FindAVarEscape(((A::SubscriptVar *)var)->var, depth);
  } break;
  
  default:
    break;
  }
}

}  // namespace ESC