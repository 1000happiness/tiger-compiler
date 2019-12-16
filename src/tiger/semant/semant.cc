#include "tiger/semant/semant.h"
#include "tiger/errormsg/errormsg.h"
#include <vector>

extern EM::ErrorMsg errormsg;

using VEnvType = S::Table<E::EnvEntry> *;
using TEnvType = S::Table<TY::Ty> *;

namespace {
static TY::TyList *make_formal_tylist(TEnvType tenv, A::FieldList *params) {
  if (params == nullptr) {
    return nullptr;
  }

  TY::Ty *ty = tenv->Look(params->head->typ);
  if (ty == nullptr) {
    errormsg.Error(params->head->pos, "undefined type %s",
                   params->head->typ->Name().c_str());
  }

  return new TY::TyList(ty->ActualTy(), make_formal_tylist(tenv, params->tail));
}

static TY::FieldList *make_fieldlist(TEnvType tenv, A::FieldList *fields) {
  if (fields == nullptr) {
    return nullptr;
  }

  TY::Ty *ty = tenv->Look(fields->head->typ);
  return new TY::FieldList(new TY::Field(fields->head->name, ty),
                           make_fieldlist(tenv, fields->tail));
} 

}  // namespace

namespace A {

TY::Ty *SimpleVar::SemAnalyze(VEnvType venv, TEnvType tenv,
                              int labelcount) const {
  TY::Ty *ty = tenv->Look(this->sym);
  if (ty == nullptr) {
    errormsg.Error(this->pos, "undefined variable %s", this->sym->Name().c_str());
    return TY::NilTy::Instance();
  }
  return ty;
}

TY::Ty *FieldVar::SemAnalyze(VEnvType venv, TEnvType tenv,
                             int labelcount) const {
  TY::Ty *fieldty = this->var->SemAnalyze(venv, tenv, labelcount), *actualty = nullptr;
  if(fieldty->kind == TY::Ty::RECORD){
    for(auto it = ((TY::RecordTy *)fieldty)->fields; it; it = it->tail){
      if(it->head->name->Name() == this->sym->Name()){
        actualty = it->head->ty;
      }
    }

    if(actualty == nullptr){
      errormsg.Error(this->pos, "field %s doesn't exist", this->sym->Name().c_str());
      return TY::VoidTy::Instance();
    }
  }
  else{
    errormsg.Error(this->pos, "not a record type");
    return TY::VoidTy::Instance();
  }
  return actualty;
}

TY::Ty *SubscriptVar::SemAnalyze(VEnvType venv, TEnvType tenv,
                                 int labelcount) const {
  TY::Ty *arrayty = this->var->SemAnalyze(venv, tenv, labelcount);
  if(arrayty->kind != TY::Ty::ARRAY){
    errormsg.Error(this->pos, "array type required");
    return TY::VoidTy::Instance();
  } 
  return ((TY::ArrayTy *)arrayty)->ty;
}

TY::Ty *VarExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
  TY::Ty *ty = this->var->SemAnalyze(venv, tenv, labelcount);
  return ty;
}

TY::Ty *NilExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
  return TY::NilTy::Instance();
}

TY::Ty *IntExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
  return TY::IntTy::Instance();
}

TY::Ty *StringExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                              int labelcount) const {
  return TY::StringTy::Instance();
}

TY::Ty *CallExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                            int labelcount) const {
  TY::Ty *ty = tenv->Look(this->func);
  E::EnvEntry *vy = venv->Look(this->func);
  errormsg.Error(this->pos, "call function %s", this->func->Name().c_str());
  if(ty == nullptr){
    errormsg.Error(this->pos, "undefined function %s", this->func->Name().c_str());
    return TY::VoidTy::Instance();
  }

  if(vy->kind != E::EnvEntry::FUN){
    errormsg.Error(this->pos, "not function %s", this->func->Name().c_str());
    return TY::VoidTy::Instance();
  }
  
  auto parait = ((E::FunEntry *)vy)->formals;
  auto argsit = this->args;
  
  for(; parait && argsit; parait = parait->tail, argsit = argsit->tail){
  }

  if(parait != nullptr && argsit == nullptr){
    errormsg.Error(this->pos, "para type mismatch");
    return ty;
  }
  else if(parait == nullptr && argsit != nullptr){
    errormsg.Error(this->pos, "too many params in function %s", this->func->Name().c_str());
    return ty;
  }
  
  parait = ((E::FunEntry *)vy)->formals;
  argsit = this->args;

  for(; parait && argsit; parait = parait->tail, argsit = argsit->tail){
    if(!parait->head->IsSameType(argsit->head->SemAnalyze(venv, tenv, labelcount))){
      errormsg.Error(argsit->head->pos, "para type mismatch");
    }
  }
  return ty;
}

TY::Ty *OpExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
  TY::Ty *leftty = this->left->SemAnalyze(venv, tenv, labelcount);
  TY::Ty *rightty = this->right->SemAnalyze(venv, tenv, labelcount);
  switch (this->oper)
  {
  case PLUS_OP:
  case MINUS_OP:
  case TIMES_OP:
  case DIVIDE_OP:
    if(!leftty->IsSameType(TY::IntTy::Instance())){
      errormsg.Error(this->left->pos, "integer required");
      return TY::IntTy::Instance();
    }
    if(!rightty->IsSameType(TY::IntTy::Instance())){
      errormsg.Error(this->right->pos, "integer required");
      return TY::IntTy::Instance();
    }
    return TY::IntTy::Instance();
  case EQ_OP:
  case NEQ_OP:
  case LT_OP:
  case LE_OP:
  case GT_OP:
  case GE_OP:
    if(!leftty->IsSameType(rightty)){
      errormsg.Error(this->right->pos, "same type required");
      return TY::IntTy::Instance();
    }
    //errormsg.Error(this->right->pos, "OP L: %d, R: %d", rightty->kind, leftty->kind);
    return TY::IntTy::Instance();
  default:
    errormsg.Error(this->right->pos, "undefined operation");
    return TY::VoidTy::Instance();
  }
}

TY::Ty *RecordExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                              int labelcount) const {
  TY::Ty *ty = tenv->Look(this->typ);
  //errormsg.Error(this->pos, "record type %s : %d", this->typ->Name().c_str(), int(ty->ActualTy()->kind));
  if(ty == nullptr){
    errormsg.Error(this->pos, "undefined type %s", this->typ->Name().c_str());
    return new TY::RecordTy(nullptr);
  }
  
  if(ty->kind != TY::Ty::RECORD){
    errormsg.Error(this->pos, "not record type %s", this->typ->Name().c_str());
  }
  auto paramit = ((TY::RecordTy *)ty)->fields;
  for(auto argsit = this->fields; argsit && paramit; argsit = argsit->tail, paramit = paramit->tail){
    if(argsit->head->name->Name() != paramit->head->name->Name()){
      errormsg.Error(this->pos, "field %s doesn't exist", argsit->head->name->Name().c_str());
    }
    if(argsit->head->exp->SemAnalyze(venv, tenv, labelcount)->kind != paramit->head->ty->kind){
      errormsg.Error(this->pos, "field %s type error", argsit->head->name->Name().c_str());
    }
  }
  return ty;
}

TY::Ty *SeqExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
  TY::Ty *ty = nullptr;
  for(auto it = this->seq; it; it = it->tail) {
    ty = it->head->SemAnalyze(venv, tenv, labelcount);
    errormsg.Error(it->head->pos, "exp type %d", ty->kind);
  }
  return ty;
}

TY::Ty *AssignExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                              int labelcount) const {
  TY::Ty *varty = this->var->SemAnalyze(venv, tenv, labelcount);
  TY::Ty *expty = this->exp->SemAnalyze(venv, tenv, labelcount);
  if(!varty->IsSameType(expty)){
    errormsg.Error(this->pos, "unmatched assign exp %d : %d", varty->kind, expty->kind);
  }
  if(varty->kind == TY::Ty::RECORD && expty->kind == TY::Ty::RECORD){
    SimpleVar *simple = (SimpleVar *)this->var;
    RecordExp *record = (RecordExp *)this->exp;
    if(simple->sym->Name() != record->typ->Name()){
      errormsg.Error(this->pos, "type mismatch");
    }
  }
  
  return TY::VoidTy::Instance();
}

TY::Ty *IfExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
  //check test
  TY::Ty *testty = this->test->SemAnalyze(venv, tenv, labelcount);
  if(!testty->IsSameType(TY::IntTy::Instance())){
    errormsg.Error(this->pos, "If test exp not int type");
  }
  //check then
  TY::Ty *thenty = this->then->SemAnalyze(venv, tenv, labelcount);
  if(!thenty->IsSameType(TY::VoidTy::Instance())){
    errormsg.Error(this->pos, "if-then exp's body must produce no value");
  }
  //check else
  TY::Ty *elsety = nullptr;
  if(this->elsee != nullptr){
    elsety = this->elsee->SemAnalyze(venv, tenv, labelcount);
    if(!elsety->IsSameType(TY::VoidTy::Instance())){
      errormsg.Error(this->pos, "if-then exp's body must produce no value");
    }
  }
  if(elsety != nullptr){
    if(!thenty->IsSameType(elsety)){
      errormsg.Error(this->pos, "then exp and else exp type mismatch");
    }
  }
  return TY::VoidTy::Instance();
}

TY::Ty *WhileExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                             int labelcount) const {
  //check test
  TY::Ty *ty = this->test->SemAnalyze(venv, tenv, labelcount);
  if (!ty->IsSameType(TY::IntTy::Instance())) {
    errormsg.Error(this->pos, "test error type");
  }
  //check body
  ty = this->body->SemAnalyze(venv, tenv, labelcount);
  if (!ty->IsSameType(TY::VoidTy::Instance())) {
    errormsg.Error(this->pos, "while body must produce no value");
  }
  return TY::VoidTy::Instance();
}

TY::Ty *ForExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
  venv->BeginScope();
  tenv->BeginScope();
  //check range
  TY::Ty *lowty = this->lo->SemAnalyze(venv, tenv, labelcount);
  TY::Ty *highty = this->hi->SemAnalyze(venv, tenv, labelcount);
  if (!lowty->IsSameType(TY::IntTy::Instance())) {
    errormsg.Error(this->pos, "for exp's range type is not integer");
  }
  if (!highty->IsSameType(TY::IntTy::Instance())) {
    errormsg.Error(this->pos, "for exp's range type is not integer");
  }
  tenv->Enter(this->var, TY::IntTy::Instance());

  //check body
  TY::Ty *ty = this->body->SemAnalyze(venv, tenv, labelcount);
  if (!ty->IsSameType(TY::VoidTy::Instance())) {
    errormsg.Error(this->pos, "for body must produce no value");
  }
  //check assign exp
  if(this->body->kind == Exp::Kind::ASSIGN) {
    if(((SimpleVar *)(((AssignExp *)this->body)->var))->sym->Name() == this->var->Name()){
      errormsg.Error(this->pos, "loop variable can't be assigned");
    }
  }
  else if(this->body->kind == Exp::Kind::SEQ) {
    SeqExp * seqbody = (SeqExp *)this->body;
    for(auto it = seqbody->seq; it; it = it->tail){
      if(it->head->kind == Exp::Kind::ASSIGN) {
        if(((SimpleVar *)(((AssignExp *)it->head)->var))->sym->Name() == this->var->Name()){
          errormsg.Error(it->head->pos, "loop variable can't be assigned");
        }
      }
    }
  }
  //end
  venv->EndScope();
  tenv->EndScope();
  return TY::VoidTy::Instance();
}

TY::Ty *BreakExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                             int labelcount) const {
  // TODO: Put your codes here (lab4).
  return TY::VoidTy::Instance();
}

TY::Ty *LetExp::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
  venv->BeginScope();
  tenv->BeginScope();
  for(auto it = this->decs; it; it = it->tail){
    it->head->SemAnalyze(venv, tenv, labelcount);
  }
  TY::Ty *ty = this->body->SemAnalyze(venv, tenv, labelcount);
  venv->EndScope();
  tenv->EndScope();
  return TY::VoidTy::Instance();
}

TY::Ty *ArrayExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                             int labelcount) const {
  TY::Ty *ty = tenv->Look(this->typ);
  if(ty->kind != TY::Ty::ARRAY){
    if(ty->kind == TY::Ty::NAME){
      if(((TY::NameTy *)ty)->ty->kind != TY::Ty::ARRAY){
        errormsg.Error(this->pos, "not array");
        return TY::VoidTy::Instance();
      }
      else{
        ty = ((TY::NameTy *)ty)->ty;
      }
    }
    else{
      errormsg.Error(this->pos, "not array");
      return TY::VoidTy::Instance();
    }
  }
  TY::Ty *expty = this->init->SemAnalyze(venv, tenv, labelcount);
  TY::Ty *arrayty = ((TY::ArrayTy *)ty)->ty;
  if(!arrayty->IsSameType(expty)){
    errormsg.Error(this->pos, "type mismatch");
  }
  return ty;
}

TY::Ty *VoidExp::SemAnalyze(VEnvType venv, TEnvType tenv,
                            int labelcount) const {
  return TY::VoidTy::Instance();
}

void FunctionDec::SemAnalyze(VEnvType venv, TEnvType tenv,
                             int labelcount) const {
  std::vector<S::Symbol *> redec;
  for(auto it = this->functions; it; it = it->tail){
    errormsg.Error(this->pos, "function %s", it->head->name->Name().c_str());
    TY::Ty *ty = nullptr, *vy = nullptr;
    if(it->head->result == nullptr){
      ty = TY::NilTy::Instance();
    }
    else{
      ty = tenv->Look(it->head->result);
    }
    
    if(ty == nullptr){
      errormsg.Error(this->pos, "undefined type %s", it->head->result->Name().c_str());
    }

    if(tenv->Look(it->head->name) != nullptr){
      tenv->Set(it->head->name, ty);
      bool flag = false;
      for(auto i = redec.begin(); i != redec.end(); i++){
        if((*i)->Name() == it->head->name->Name()){
          flag = true;
        }
        //errormsg.Error(this->pos, "functions %s", (*i)->Name().c_str());
      }
      if(flag){
        errormsg.Error(this->pos, "two functions have the same name");
      }
      //errormsg.Error(this->pos, "function set %s : %d", it->head->name->Name().c_str(), int(ty->kind));
    }
    else{
      tenv->Enter(it->head->name, ty);
      //errormsg.Error(this->pos, "function enter %s : %d", it->head->name->Name().c_str(), int(ty->kind));
    }

    if(venv->Look(it->head->name) != nullptr){
      venv->Set(it->head->name, new E::FunEntry(make_formal_tylist(tenv ,it->head->params), ty));
    }
    else{
      venv->Enter(it->head->name, new E::FunEntry(make_formal_tylist(tenv ,it->head->params), ty));
    }
    redec.push_back(it->head->name);
  }

  for(auto it = this->functions; it; it = it->tail){
    venv->BeginScope();
    tenv->BeginScope();
    for(auto i = it->head->params; i; i = i ->tail){
      TY::Ty *paramty = tenv->Look(i->head->typ);
      if(paramty == nullptr){
        errormsg.Error(this->pos, "undefined type %s", i->head->typ->Name().c_str());            
        continue;
      }
      else{
        tenv->Enter(i->head->name, paramty);
      }
    }

    TY::Ty *expty = it->head->body->SemAnalyze(venv, tenv, labelcount);
    if(it->head->result != nullptr){
      if(expty->kind != tenv->Look(it->head->result)->kind){
        errormsg.Error(this->pos, "function %s body type error", it->head->name->Name().c_str(), int(expty->kind));
      }
    }
    else{
      errormsg.Error(this->pos, "procedure returns value");
    }

    venv->EndScope();
    tenv->EndScope();
  }
}

void VarDec::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
  TY::Ty *varty = nullptr, *expty = this->init->SemAnalyze(venv, tenv, labelcount);
  if(this->typ != nullptr){
    varty = tenv->Look(this->typ);
    if(varty->kind == TY::Ty::NAME){
      varty = ((TY::NameTy *)varty)->ty;
    }

    if(!varty->IsSameType(expty)){
      if(expty->kind != TY::Ty::NIL){
        errormsg.Error(this->pos, "type mismatch");
      }
    }
    if(varty->kind == TY::Ty::RECORD && expty->kind == TY::Ty::RECORD){
      if(varty != expty){
        //errormsg.Error(this->pos, "type mismatch");
      }
    }
    if(varty->kind == TY::Ty::ARRAY && expty->kind == TY::Ty::ARRAY){
      if(varty != expty){
        //errormsg.Error(this->pos, "type mismatch");
      }
    }
    if(tenv->Look(this->var) != nullptr){
      tenv->Set(this->var, varty);
    }
    else{
      tenv->Enter(this->var, varty);
    }
  }
  else{
    if(tenv->Look(this->var) != nullptr){
      tenv->Set(this->var, expty);
      if(expty->kind == TY::Ty::NIL){
        errormsg.Error(this->pos, "init should not be nil without type specified");
      }
    }
    else{
      tenv->Enter(this->var, expty);
      if(expty->kind == TY::Ty::NIL){
        errormsg.Error(this->pos, "init should not be nil without type specified");
      }
    }
  }
}

void TypeDec::SemAnalyze(VEnvType venv, TEnvType tenv, int labelcount) const {
  for(auto it = this->types; it; it = it->tail){
    errormsg.Error(this->pos, "type %s : %d", it->head->name->Name().c_str(), int(it->head->ty->kind));
    TY::Ty *ty = it->head->ty->SemAnalyze(tenv);
    TY::Ty *temp = nullptr;
    if((temp = tenv->Look(it->head->name)) != nullptr){
      
      tenv->Set(it->head->name, ty);
      errormsg.Error(this->pos, "type set %s : %d", it->head->name->Name().c_str(), int(ty->kind));
    }
    else{
      tenv->Enter(it->head->name, ty);
      errormsg.Error(this->pos, "type enter %s : %d", it->head->name->Name().c_str(), int(ty->kind));
    }
  }
  
  TY::Ty *headty, *decty;
  bool it_flag = true;
  for(auto it = this->types; it && it_flag; it = it->tail){
    errormsg.Error(this->pos, "change: %s", it->head->name->Name().c_str());
    headty = tenv->Look(it->head->name);
    decty = it->head->ty->SemAnalyze(tenv);
    
    if(headty == nullptr){
      errormsg.Error(this->pos, "undefined name: %s", it->head->name->Name().c_str());
      continue;
    }
    else if(headty->kind == TY::Ty::NAME){
      TY::Ty *actualty = nullptr;
      if(((TY::NameTy *)headty)->ty == nullptr){
        errormsg.Error(this->pos, "change namety: %s", it->head->name->Name().c_str());
        actualty = tenv->Look(((TY::NameTy *)headty)->sym);
        while (true)
        {
          if(actualty == nullptr){
            errormsg.Error(this->pos, "undefined ed namety: %s", it->head->name->Name().c_str());
            break;
          }
          else if(actualty->kind != TY::Ty::NAME){
            ((TY::NameTy *)headty)->ty = actualty;
            break;
          }
          else if(actualty->kind == TY::Ty::NAME){
            if(((TY::NameTy *)actualty)->sym->Name() == ((TY::NameTy *)headty)->sym->Name()){
              errormsg.Error(this->pos, "illegal type cycle");
              it_flag = false;
              break;
            }
            // errormsg.Error(this->pos, "switch namety: %s", ((TY::NameTy *)actualty)->sym->Name().c_str());
            actualty = tenv->Look(((TY::NameTy *)actualty)->sym);
            continue;
          }
        }
      }
      else if(((TY::NameTy *)headty)->ty->kind == TY::Ty::NAME){
        actualty = ((TY::NameTy *)headty)->ty;
        while (true)
        {
          if(actualty == nullptr){
            errormsg.Error(this->pos, "undefined ed ed namety: %s", it->head->name->Name().c_str());
            break;
          }
          else if(actualty->kind != TY::Ty::NAME){
            ((TY::NameTy *)headty)->ty = actualty;
            break;
          }
          else if(actualty->kind == TY::Ty::NAME){
            if(((TY::NameTy *)actualty)->sym->Name() == ((TY::NameTy *)headty)->sym->Name()){
              errormsg.Error(this->pos, "illegal type cycle");
              it_flag = false;
              break;
            }
            actualty = tenv->Look(((TY::NameTy *)actualty)->sym);
            continue;
          }
        }
      }
      else {
        if(decty->kind == TY::Ty::NAME){
          if(((TY::NameTy *)decty)->ty != nullptr && ((TY::NameTy *)headty)->ty->kind != ((TY::NameTy *)decty)->ty->kind){
            errormsg.Error(this->pos, "two types have the same name");
          }
        }
        break;
      }
    }  
    else if(headty->kind == TY::Ty::RECORD){
      // errormsg.Error(this->pos, "record type");        
      for(auto i = ((TY::RecordTy *)headty)->fields; i; i= i->tail){
        if(i->head->ty == nullptr){
          errormsg.Error(this->pos, "undefined type %s", i->head->name->Name().c_str());            
          continue;
        }
        else if(i->head->ty->kind == TY::Ty::NAME){
          TY::Ty *ty = tenv->Look(((TY::NameTy *)i->head->ty)->sym);
          if(ty == i->head->ty){
            errormsg.Error(this->pos, "undefined type %s", ((TY::NameTy *)i->head->ty)->sym->Name().c_str());            
          }
          else{
            ((TY::NameTy *)i->head->ty)->ty = ty;
          }
        }
      }
    }
  }
}

TY::Ty *NameTy::SemAnalyze(TEnvType tenv) const {
  TY::Ty *temp, *ty;
  temp = tenv->Look(this->name);
  if(temp == nullptr){
    ty = new TY::NameTy(this->name, nullptr);
  }
  else{
    if(temp->kind == TY::Ty::NAME){
      ty = new TY::NameTy(this->name, nullptr);
    }
    else{
      ty = new TY::NameTy(this->name, temp);
      //errormsg.Error(this->pos, "NameTy not nullptr %d", (int)temp->kind);
    }
  }
  //errormsg.Error(this->pos, "NameTy %s", this->name->Name().c_str());
  return ty;
}

TY::Ty *RecordTy::SemAnalyze(TEnvType tenv) const {
  TY::Ty *ty;
  if(this->record == nullptr){
    ty = new TY::RecordTy(nullptr);
  }
  else {
    for(auto it = this->record; it; it = it->tail){
      ty = tenv->Look(it->head->typ);
      if(ty == nullptr){
        // errormsg.Error(it->head->pos, "recordTY typ %s", it->head->typ->Name().c_str());
        tenv->Enter(it->head->typ, new TY::NameTy(it->head->typ, nullptr));
      }
    }
    ty = new TY::RecordTy(make_fieldlist(tenv, this->record));
  }
  return ty;
}

TY::Ty *ArrayTy::SemAnalyze(TEnvType tenv) const {
  TY::Ty *arrayty = tenv->Look(this->array);
  if(arrayty == nullptr){
    errormsg.Error(this->pos, "array type undefined");
  }
  return new TY::ArrayTy(arrayty);
}

}  // namespace A

namespace SEM {
void SemAnalyze(A::Exp *root) {
  if (root) root->SemAnalyze(E::BaseVEnv(), E::BaseTEnv(), 0);
}

}  // namespace SEM
