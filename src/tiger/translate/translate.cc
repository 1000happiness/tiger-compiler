#include "tiger/translate/translate.h"

#include <cstdio>
#include <set>
#include <string>
#include <vector>

#include "tiger/errormsg/errormsg.h"
#include "tiger/frame/temp.h"
#include "tiger/semant/semant.h"
#include "tiger/semant/types.h"
#include "tiger/util/util.h"

extern EM::ErrorMsg errormsg;

namespace TR {

F::FragList *fraglist = nullptr;
F::FragList *fraglistCur = nullptr;

/*-------------------------------------------------------------------------------*/

class Access {
 public:
  Level *level;
  F::Access *access;

  Access(Level *level, F::Access *access) : level(level), access(access) {}
  static Access *AllocLocal(Level *level, bool escape);
};

class AccessList {
 public:
  Access *head;
  AccessList *tail;

  AccessList(Access *head, AccessList *tail) : head(head), tail(tail) {}
};



/*-------------------------------------------------------------------------------*/

class Level {
 public:
  F::Frame *frame;
  Level *parent;

  Level(F::Frame *frame, Level *parent) : frame(frame), parent(parent) {}
  AccessList *Formals() {
    F::AccessList *f_accesslist = frame->getFormals();
    TR::AccessList *tr_accesslist = nullptr, *cur = nullptr;
    for(auto it = f_accesslist; it; it=it->tail){
      if(tr_accesslist == nullptr){
        tr_accesslist = new TR::AccessList(new TR::Access(this, it->head), nullptr);
        cur = tr_accesslist;
      }
      else{
        cur->tail = new TR::AccessList(new TR::Access(this, it->head), nullptr);
        cur = cur->tail;
      }
    }
    return tr_accesslist;
  }

  static Level *NewLevel(
    Level *parent, 
    TEMP::Label *name,
    U::BoolList *formals
  ){
    F::Frame *frame = F::newFrame(name, formals);
    Level *level = new Level(frame, parent);
    return level;
  }
};

Level *Outermost() {
  static Level *lv = nullptr;
  if (lv != nullptr) return lv;

  lv = new Level(nullptr, nullptr);
  return lv;
}

/*-------------------------------------------------------------------------------*/

class PatchList {
 public:
  TEMP::Label **head;
  PatchList *tail;

  PatchList(TEMP::Label **head, PatchList *tail) : head(head), tail(tail) {}
};

void do_patch(PatchList *tList, TEMP::Label *label) {
  for (; tList; tList = tList->tail) *(tList->head) = label;
}

PatchList *join_patch(PatchList *first, PatchList *second) {
  if (!first) return second;
  for (; first->tail; first = first->tail)
    ;
  first->tail = second;
  return first;
}

/*-------------------------------------------------------------------------------*/

class Cx {
 public:
  PatchList *trues;
  PatchList *falses;
  T::Stm *stm;

  Cx(PatchList *trues, PatchList *falses, T::Stm *stm)
      : trues(trues), falses(falses), stm(stm) {}
};

class Exp {
 public:
  enum Kind { EX, NX, CX };

  Kind kind;

  Exp(Kind kind) : kind(kind) {}

  virtual T::Exp *UnEx() const = 0;
  virtual T::Stm *UnNx() const = 0;
  virtual Cx UnCx() const = 0;
};

class ExpAndTy {
 public:
  TR::Exp *exp;
  TY::Ty *ty;

  ExpAndTy(TR::Exp *exp, TY::Ty *ty) : exp(exp), ty(ty) {}
};

class ExExp : public Exp {
 public:
  T::Exp *exp;

  ExExp(T::Exp *exp) : Exp(EX), exp(exp) {}

  T::Exp *UnEx() const override {
    return this->exp;
  }
  T::Stm *UnNx() const override {
    return new T::ExpStm(this->exp);
  }
  Cx UnCx() const override {
    Cx cx(
      nullptr,
      nullptr,
      new T::ExpStm(this->exp)
    );
    return cx;
  }
};

class NxExp : public Exp {
 public:
  T::Stm *stm;

  NxExp(T::Stm *stm) : Exp(NX), stm(stm) {}

  T::Exp *UnEx() const override {
    return new T::EseqExp(this->stm, new T::ConstExp(0));
  }
  T::Stm *UnNx() const override {
    return this->stm;
  }
  Cx UnCx() const override {
    Cx cx(
      nullptr,
      nullptr,
      this->stm
    );
    return cx;
  }
};

class CxExp : public Exp {
 public:
  Cx cx;

  CxExp(struct Cx cx) : Exp(CX), cx(cx) {}
  CxExp(PatchList *trues, PatchList *falses, T::Stm *stm)
      : Exp(CX), cx(trues, falses, stm) {}

  T::Exp *UnEx() const override {
    TEMP::Temp *temp = TEMP::Temp::NewTemp();
    TEMP::Label *trueLabel = TEMP::NewLabel();
    TEMP::Label *falseLabel = TEMP::NewLabel();
    TEMP::Label *endLabel = TEMP::NewLabel();
    TR::Cx cx = this->cx;
    TR::do_patch(cx.trues, trueLabel);
    TR::do_patch(cx.falses, falseLabel);
    T::Exp *exp= new T::EseqExp(
      new T::SeqStm(
        new T::CjumpStm(
          ((T::CjumpStm *)cx.stm)->op,
          ((T::CjumpStm *)cx.stm)->left,
          ((T::CjumpStm *)cx.stm)->right,
          trueLabel,
          falseLabel
        ),
        new T::SeqStm(
          new T::LabelStm(trueLabel),
          new T::SeqStm(
            new T::MoveStm(
              new T::TempExp(temp),
              new T::ConstExp(1)
            ),
            new T::SeqStm(
              new T::LabelStm(endLabel),
              new T::SeqStm(
                new T::LabelStm(falseLabel),
                new T::SeqStm(
                  new T::MoveStm(
                    new T::TempExp(temp),
                    new T::ConstExp(0)
                  ),
                  new T::LabelStm(endLabel)
                )
              )
            )
          )
        )
      ),
      new T::TempExp(temp)
    );
    return exp;
  }

  T::Stm *UnNx() const override {
    return this->cx.stm;
  }

  Cx UnCx() const override {
    return this->cx;
  }
};

/*-------------------------------------------------------------------------------*/

void procEntryExit(Level *level, Exp *body) {
	T::Stm *result = F::F_procEntryExit1(level->frame, body->UnNx());
  if(fraglist == nullptr){
    fraglist = new F::FragList(new F::ProcFrag(result, level->frame), nullptr);
    fraglistCur = fraglist;
  }
	else{
    fraglistCur->tail = new F::FragList(new F::ProcFrag(result, level->frame), nullptr);
    fraglistCur = fraglistCur->tail;
  }
}

F::FragList *TranslateProgram(A::Exp *root) {
  ExpAndTy expty(nullptr, nullptr);
  Level *level = Level::NewLevel(Outermost(), TEMP::NamedLabel("tigermain"), new U::BoolList(true, nullptr));
  T::ExpStm *expStm = new T::ExpStm(new T::ConstExp(0));
  procEntryExit(level, new NxExp(expStm)); 
  if (root) {
    expty = root->Translate(E::BaseVEnv(), E::BaseTEnv(), level, nullptr);
    expStm->exp = expty.exp->UnEx();
  }
  //procEntryExit(level, new NxExp(expStm));

  return fraglist;
}

Access *Access::AllocLocal(Level *level, bool escape) {
  F::Access *f_access = level->frame->allocLocal(escape);
  return new Access(level, f_access);
}

}  // namespace TR

namespace {
static TY::TyList *make_formal_tylist(S::Table<TY::Ty> *tenv, A::FieldList *params) {
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

static TY::FieldList *make_fieldlist(S::Table<TY::Ty> *tenv, A::FieldList *fields) {
  if (fields == nullptr) {
    return nullptr;
  }

  TY::Ty *ty = tenv->Look(fields->head->typ);
  return new TY::FieldList(new TY::Field(fields->head->name, ty),
                           make_fieldlist(tenv, fields->tail));
} 

}  // namespace

namespace A {

TR::ExpAndTy SimpleVar::Translate(S::Table<E::EnvEntry> *venv,
                                  S::Table<TY::Ty> *tenv, TR::Level *level,
                                  TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  TY::Ty *ty = tenv->Look(this->sym);
  if (ty == nullptr) {
    errormsg.Error(this->pos, "undefined variable %s", this->sym->Name().c_str());
    return TR::ExpAndTy(new TR::ExExp(new T::ConstExp(0)), TY::NilTy::Instance());
  }

  E::EnvEntry *vy = venv->Look(this->sym);
  TR::Level *declevel = ((E::VarEntry *)vy)->access->level;

  TR::Exp *exp = nullptr;
  //实现静态链
  T::Exp *staticLink = new T::TempExp(F::FP());
  TR::Level *it = level;
  while(true){
    if(it == TR::Outermost()){
      break;
    }
    if(it == declevel){
      break;
    }
    else{
      it = it->parent;
      staticLink = it->Formals()->head->access->ToExp(staticLink);
    }
  }
  exp = new TR::ExExp(
    ((E::VarEntry *)vy)->access->access->ToExp(staticLink)
  );
  
  return TR::ExpAndTy(exp, ty);
}

TR::ExpAndTy FieldVar::Translate(S::Table<E::EnvEntry> *venv,
                                 S::Table<TY::Ty> *tenv, TR::Level *level,
                                 TEMP::Label *label) const {
  TR::Exp *exp = nullptr;
  TR::ExpAndTy varexpty = this->var->Translate(venv, tenv, level, label);

  TY::Ty *fieldty = varexpty.ty, *actualty = nullptr;
  int offset = 0;
  if(fieldty->kind == TY::Ty::RECORD){
    for(auto it = ((TY::RecordTy *)fieldty)->fields; it; it = it->tail){
      if(it->head->name->Name() == this->sym->Name()){
        actualty = it->head->ty;
        break;
      }
      offset += F::addressSize; 
    }

    if(actualty == nullptr){
      errormsg.Error(this->pos, "field %s doesn't exist", this->sym->Name().c_str());
      return TR::ExpAndTy(new TR::ExExp(new T::ConstExp(0)), TY::VoidTy::Instance());
    }
  }
  else{
    errormsg.Error(this->pos, "not a record type");
    return TR::ExpAndTy(new TR::ExExp(new T::ConstExp(0)), TY::VoidTy::Instance());
  }

  exp = new TR::ExExp(
    new T::MemExp(
      new T::BinopExp(
        T::PLUS_OP,
        varexpty.exp->UnEx(),
        new T::ConstExp(offset)
      )
    )
  );

  return TR::ExpAndTy(exp, actualty);
}

TR::ExpAndTy SubscriptVar::Translate(S::Table<E::EnvEntry> *venv,
                                     S::Table<TY::Ty> *tenv, TR::Level *level,
                                     TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  TR::Exp *exp = nullptr;
  TR::ExpAndTy varexpty = this->var->Translate(venv, tenv, level, label);
  TY::Ty *arrayty = varexpty.ty, *ty = nullptr;
  if(arrayty->kind != TY::Ty::ARRAY){
    errormsg.Error(this->pos, "array type required");
    return TR::ExpAndTy(new TR::ExExp(new T::ConstExp(0)), TY::VoidTy::Instance());
  }
  TR::ExpAndTy subscriptexpty = this->subscript->Translate(venv, tenv, level, label);
  TY::Ty *subscriptty = subscriptexpty.ty;
  if(subscriptty->kind != TY::Ty::INT){
    errormsg.Error(this->pos, "subscript type int required");
    return TR::ExpAndTy(new TR::ExExp(new T::ConstExp(0)), TY::VoidTy::Instance());
  }
  ty = ((TY::ArrayTy *)arrayty)->ty;
   
  exp = new TR::ExExp(
    new T::MemExp(
      new T::BinopExp(
        T::PLUS_OP,
        varexpty.exp->UnEx(),
        new T::BinopExp(
          T::BinOp::MUL_OP,
          subscriptexpty.exp->UnEx(),
          new T::ConstExp(F::addressSize)
        )
      )
    )
  );

  return TR::ExpAndTy(exp, ty);
}

TR::ExpAndTy VarExp::Translate(S::Table<E::EnvEntry> *venv,
                               S::Table<TY::Ty> *tenv, TR::Level *level,
                               TEMP::Label *label) const {
  TR::ExpAndTy expty = this->var->Translate(venv, tenv, level, label);
  return expty;
}

TR::ExpAndTy NilExp::Translate(S::Table<E::EnvEntry> *venv,
                               S::Table<TY::Ty> *tenv, TR::Level *level,
                               TEMP::Label *label) const {
  return TR::ExpAndTy(new TR::ExExp(new T::ConstExp(0)), TY::NilTy::Instance());
}

TR::ExpAndTy IntExp::Translate(S::Table<E::EnvEntry> *venv,
                               S::Table<TY::Ty> *tenv, TR::Level *level,
                               TEMP::Label *label) const {
  TR::Exp *exp = new TR::ExExp(
    new T::ConstExp(this->i)
  );
  return TR::ExpAndTy(exp, TY::IntTy::Instance());//todo
}

TR::ExpAndTy StringExp::Translate(S::Table<E::EnvEntry> *venv,
                                  S::Table<TY::Ty> *tenv, TR::Level *level,
                                  TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  TEMP::Label *stringlabel = TEMP::NewLabel();
  //call
  //赋值
  //
  TR::Exp *exp = new TR::ExExp(
    new T::NameExp(stringlabel)
  );
  
  TR::fraglist = new F::FragList(new F::StringFrag(stringlabel, this->s), TR::fraglist);
  
  return TR::ExpAndTy(exp, TY::StringTy::Instance());
}

TR::ExpAndTy CallExp::Translate(S::Table<E::EnvEntry> *venv,
                                S::Table<TY::Ty> *tenv, TR::Level *level,
                                TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  TR::Exp *exp = nullptr;
  TY::Ty *ty = tenv->Look(this->func);
  bool external = false;
  E::EnvEntry *vy = venv->Look(this->func);
  if(ty == nullptr){
    ty = ((E::FunEntry *)vy)->result;
    external = true;
  }
  if(vy == nullptr){
    errormsg.Error(this->pos, "undefined function %s", this->func->Name().c_str());
    return TR::ExpAndTy(new TR::ExExp(new T::ConstExp(0)), TY::VoidTy::Instance());
  }

  if(vy->kind != E::EnvEntry::FUN){
    errormsg.Error(this->pos, "not function %s", this->func->Name().c_str());
    return TR::ExpAndTy(new TR::ExExp(new T::ConstExp(0)), TY::VoidTy::Instance());
  }
  
  auto parait = ((E::FunEntry *)vy)->formals;
  auto argsit = this->args;
  for(; parait && argsit; parait = parait->tail, argsit = argsit->tail){
  }

  if(parait != nullptr && argsit == nullptr){
    errormsg.Error(this->pos, "para type mismatch");
    return TR::ExpAndTy(new TR::ExExp(new T::ConstExp(0)), ty);
  }
  else if(parait == nullptr && argsit != nullptr){
    errormsg.Error(this->pos, "too many params in function %s", this->func->Name().c_str());
    return TR::ExpAndTy(new TR::ExExp(new T::ConstExp(0)), ty);
  }

  parait = ((E::FunEntry *)vy)->formals;
  argsit = this->args;

  T::ExpList *argslist = nullptr;
  T::ExpList *argsCur = nullptr;
  for(; parait && argsit; parait = parait->tail, argsit = argsit->tail){

    TR::ExpAndTy argsexpty = argsit->head->Translate(venv, tenv, level, label);

    if(!parait->head->IsSameType(argsexpty.ty)){
      errormsg.Error(argsit->head->pos, "para type mismatch");
    }
    
    if(argslist == nullptr){
      argslist = new T::ExpList(argsexpty.exp->UnEx(), nullptr);
      argsCur = argslist;
    }
    else{
      argsCur->tail = new T::ExpList(argsexpty.exp->UnEx(), nullptr);
      argsCur = argsCur->tail;
    }
  }

  TEMP::Label *funLabel = ((E::FunEntry *)vy)->label;
  if(funLabel == nullptr){
    funLabel = TEMP::NamedLabel(this->func->Name());
  }

 
  
  if(external){
    exp = new TR::ExExp(
      F::externalCall(level->frame, new T::NameExp(funLabel), argslist)
    );
  }
  else{
    T::Exp *staticLink = new T::TempExp(F::FP());
    TR::Level *calllevel = level; //call level
    TR::Level *calleelevel = ((E::FunEntry *)vy)->level; //callee define level
    // printf("%s caller\n", calllevel->frame->label->Name().c_str());
    // printf("%s callees\n", calleelevel->frame->label->Name().c_str());
    // fflush(stdout);
    if(calleelevel == calllevel){ //calllevel = calleelevel
      staticLink = new T::TempExp(F::FP());
    }
    else{// calllevel > calleelevel
      while(true){
        if(calllevel == TR::Outermost()){
          errormsg.Error(this->pos, "static link error");
          break;
        }
        if(calleelevel == calllevel){
          break;
        }
        else{
          calllevel = calllevel->parent;
          staticLink = calllevel->Formals()->head->access->ToExp(staticLink);
          printf("%s call change\n", calleelevel->frame->label->Name().c_str());
        }
      }
    }
    argslist = new T::ExpList(staticLink, argslist);//静态链第一个参数
    exp = new TR::ExExp(
      F::normalCall(level->frame, new T::NameExp(funLabel), argslist)
    );
  }

  return TR::ExpAndTy(exp, ty);
}

TR::ExpAndTy OpExp::Translate(S::Table<E::EnvEntry> *venv,
                              S::Table<TY::Ty> *tenv, TR::Level *level,
                              TEMP::Label *label) const {
  TR::Exp *exp = nullptr;
  TR::ExpAndTy leftexpty = this->left->Translate(venv, tenv, level, label);
  TR::ExpAndTy rightexpty = this->right->Translate(venv, tenv, level, label);
  TY::Ty *leftty = leftexpty.ty;
  TY::Ty *rightty = rightexpty.ty;
  TEMP::Label *trueLabel = nullptr;
  TEMP::Label *falseLabel = nullptr;
  TEMP::Temp *temp = nullptr;
  T::RelOp relop = T::EQ_OP;
  switch (this->oper)
  {
  case PLUS_OP:
  case MINUS_OP:
  case TIMES_OP:
  case DIVIDE_OP:
    if(!leftty->IsSameType(TY::IntTy::Instance())){
      errormsg.Error(this->left->pos, "integer required");
      return TR::ExpAndTy(exp, TY::IntTy::Instance());
    }
    if(!rightty->IsSameType(TY::IntTy::Instance())){
      errormsg.Error(this->right->pos, "integer required");
      return TR::ExpAndTy(exp, TY::IntTy::Instance());
    }
    exp = new TR::ExExp(
      new T::BinopExp(
        T::BinOp(this->oper),//此处T与A的操作符的枚举值一一对应
        leftexpty.exp->UnEx(),
        rightexpty.exp->UnEx()
      )
    );
    return TR::ExpAndTy(exp, TY::IntTy::Instance());
  case EQ_OP:
  case NEQ_OP:
  case LT_OP:
  case LE_OP:
  case GT_OP:
  case GE_OP:
    if(!leftty->IsSameType(rightty)){
      errormsg.Error(this->right->pos, "same type required");
      return TR::ExpAndTy(new TR::ExExp(new T::ConstExp(0)), TY::IntTy::Instance());
    }
    switch (this->oper)
    {
      case EQ_OP:
        relop = T::EQ_OP;
        break;
      case NEQ_OP:
        relop = T::NE_OP;
        break;
      case LT_OP:
        relop = T::LT_OP;
        break;
      case LE_OP:
        relop = T::LE_OP;
        break;
      case GT_OP:
        relop = T::GT_OP;
        break;
      case GE_OP:
        relop = T::GE_OP;
        break;
    default:
      break;
    }
    if(leftty->kind == TY::Ty::STRING){
      switch (this->oper)
      {
        case EQ_OP:
          exp = new TR::ExExp(
            F::externalCall(
              level->frame,
              new T::NameExp(TEMP::NamedLabel("stringEqual")),
              new T::ExpList(leftexpty.exp->UnEx(), new T::ExpList(rightexpty.exp->UnEx(), nullptr))
            )
          );
          return TR::ExpAndTy(exp, TY::IntTy::Instance());
        case NEQ_OP:
          exp = new TR::ExExp(
            F::externalCall(
              level->frame,
              new T::NameExp(TEMP::NamedLabel("not")),
              new T::ExpList(
                F::externalCall(
                  level->frame,
                  new T::NameExp(TEMP::NamedLabel("stringEqual")),
                  new T::ExpList(leftexpty.exp->UnEx(), new T::ExpList(rightexpty.exp->UnEx(), nullptr))
                ),
                nullptr
              )
            )
          );
          return TR::ExpAndTy(exp, TY::IntTy::Instance());
        case LT_OP:
          relop = T::LT_OP;
          break;
        case LE_OP:
          relop = T::LE_OP;
          break;
        case GT_OP:
          relop = T::GT_OP;
          break;
        case GE_OP:
          relop = T::GE_OP;
          break;
      default:
        break;
      }
    }
    trueLabel = TEMP::NewLabel();
    falseLabel = TEMP::NewLabel();
    temp = TEMP::Temp::NewTemp();
    exp = new TR::CxExp(
      TR::Cx(
        new TR::PatchList(&trueLabel ,nullptr),
        new TR::PatchList(&falseLabel, nullptr),
        new T::CjumpStm(
          relop,
          leftexpty.exp->UnEx(),
          rightexpty.exp->UnEx(),
          trueLabel,
          falseLabel
        )
      )
    );
    exp = new TR::ExExp(
      exp->UnEx()
    );
    
    return TR::ExpAndTy(exp, TY::IntTy::Instance());
  default:
    errormsg.Error(this->right->pos, "undefined operation");
    return TR::ExpAndTy(new TR::ExExp(new T::ConstExp(0)), TY::VoidTy::Instance());
  }
  
}

TR::ExpAndTy RecordExp::Translate(S::Table<E::EnvEntry> *venv,
                                  S::Table<TY::Ty> *tenv, TR::Level *level,
                                  TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  TR::Exp *exp = nullptr;
  TY::Ty *ty = tenv->Look(this->typ);
  if(ty == nullptr){
    errormsg.Error(this->pos, "undefined type %s", this->typ->Name().c_str());
    return TR::ExpAndTy(new TR::ExExp(new T::ConstExp(0)), new TY::RecordTy(nullptr));
  }
  
  if(ty->kind != TY::Ty::RECORD){
    errormsg.Error(this->pos, "not record type %s", this->typ->Name().c_str());
  }

  auto paramit = ((TY::RecordTy *)ty)->fields;
  T::Stm *initexplist = nullptr;
  TEMP::Temp *temp = TEMP::Temp::NewTemp();
  int size = 0;
  for(auto argsit = this->fields; argsit && paramit; argsit = argsit->tail, paramit = paramit->tail){
    if(argsit->head->name->Name() != paramit->head->name->Name()){
      errormsg.Error(this->pos, "field %s doesn't exist", argsit->head->name->Name().c_str());
      initexplist = nullptr;
      break;
    }
    TR::ExpAndTy argsexpty = argsit->head->exp->Translate(venv, tenv, level, label);
    if(argsexpty.ty->kind != paramit->head->ty->kind && argsexpty.ty->kind != TY::Ty::NIL){
      errormsg.Error(this->pos, "field %s type error", argsit->head->name->Name().c_str());
      errormsg.Error(this->pos, "type error %d %d", argsexpty.ty->kind, paramit->head->ty->kind);
      initexplist = nullptr;
      break;
    }

    if(argsit == this-> fields){
      if(argsit->tail == nullptr){
        initexplist = new T::MoveStm(
          new T::MemExp(
            new T::BinopExp(
              T::PLUS_OP,
              new T::TempExp(temp),
              new T::ConstExp(size)
            )
          ),
          argsexpty.exp->UnEx()
        );
      }
      else{
        initexplist = new T::SeqStm(
          new T::MoveStm(
            new T::MemExp(
              new T::BinopExp(
                T::PLUS_OP,
                new T::TempExp(temp),
                new T::ConstExp(size)
              )
            ),
            argsexpty.exp->UnEx()
          ),
          nullptr
        );
      }
    }
    else{
      if(argsit->tail == nullptr){
        T::SeqStm *cur = (T::SeqStm *)initexplist;
        while(cur->right != nullptr){
          cur = (T::SeqStm *)cur->right;
        }
        cur->right = new T::MoveStm(
          new T::MemExp(
            new T::BinopExp(
              T::PLUS_OP,
              new T::TempExp(temp),
              new T::ConstExp(size)
            )
          ),
          argsexpty.exp->UnEx()
        );
      }
      else{
        initexplist = new T::SeqStm(
          new T::MoveStm(
            new T::MemExp(
              new T::BinopExp(
                T::PLUS_OP,
                new T::TempExp(temp),
                new T::ConstExp(size)
              )
            ),
            argsexpty.exp->UnEx()
          ),
          initexplist
        );
      }
    }

    size += F::addressSize;
  }

  exp = new TR::ExExp(
    new T::EseqExp(
      new T::SeqStm(
        new T::MoveStm(
          new T::TempExp(temp),
          F::externalCall(
            level->frame,
            new T::NameExp(TEMP::NamedLabel("allocRecord")),
            new T::ExpList(new T::ConstExp(size), nullptr)
          )
        ),
        initexplist
      ),
      new T::TempExp(temp)
    )
  );

  return TR::ExpAndTy(exp, ty);
}

TR::ExpAndTy SeqExp::Translate(S::Table<E::EnvEntry> *venv,
                               S::Table<TY::Ty> *tenv, TR::Level *level,
                               TEMP::Label *label) const {
  TR::Exp *exp = nullptr;
  TY::Ty *ty = nullptr;

  if(this->seq == nullptr){
    errormsg.Error(this->seq->head->pos, "empty seq");
    return TR::ExpAndTy(new TR::ExExp(new T::ConstExp(0)), TY::VoidTy::Instance());
  }

  TR::ExpAndTy headexpty = this->seq->head->Translate(venv, tenv, level, label);
  if(this->seq->tail == nullptr){
    exp = headexpty.exp;
    ty = headexpty.ty;
  }
  else{
    exp = new TR::ExExp(new T::EseqExp(headexpty.exp->UnNx(), nullptr));
    for(auto it = this->seq->tail; it; it = it->tail){
      T::Exp *cur = ((TR::ExExp *)exp)->exp;
      while(((T::EseqExp *)cur)->exp != nullptr){
        cur = ((T::EseqExp *)cur)->exp;
      }
      if(it->tail == nullptr){
        TR::ExpAndTy tailexpty = it->head->Translate(venv, tenv, level, label);
        ((T::EseqExp *)cur)->exp = tailexpty.exp->UnEx();
        ty = tailexpty.ty;
      }
      else{
        TR::ExpAndTy tailexpty = it->head->Translate(venv, tenv, level, label);
        ((T::EseqExp *)cur)->exp = new T::EseqExp(tailexpty.exp->UnNx(), nullptr);
      }
    }
  }

  return TR::ExpAndTy(exp, ty);
}

TR::ExpAndTy AssignExp::Translate(S::Table<E::EnvEntry> *venv,
                                  S::Table<TY::Ty> *tenv, TR::Level *level,
                                  TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  TR::Exp *exp = nullptr;

  TR::ExpAndTy varty = this->var->Translate(venv, tenv, level, label);
  TR::ExpAndTy expty = this->exp->Translate(venv, tenv, level, label);

  SimpleVar *simple = nullptr;

  if(!varty.ty->IsSameType(expty.ty)){
    errormsg.Error(this->pos, "unmatched assign exp %d : %d", varty.ty->kind, expty.ty->kind);
  }
  switch (varty.ty->kind)
  {
  case TY::Ty::RECORD:
    if(expty.ty->kind == TY::Ty::RECORD){
      simple = (SimpleVar *)this->var;
      RecordExp *record = (RecordExp *)this->exp;
      if(simple->sym->Name() != record->typ->Name()){
        errormsg.Error(this->pos, "type mismatch");
      }
    }
    break;
  default:
    break;
  }
  
  if(simple != nullptr){
    E::EnvEntry *varentry = venv->Look(simple->sym);
    if(varentry != nullptr){
      if(varentry->kind == E::EnvEntry::VAR){
        if(((E::VarEntry *)varentry)->readonly){
          errormsg.Error(this->pos, "%s is read only", simple->sym->Name().c_str());
        }
      }
    }
  }

  exp = new TR::NxExp(
    new T::MoveStm(
      varty.exp->UnEx(),
      expty.exp->UnEx()
    )
  );

  return TR::ExpAndTy(exp, TY::VoidTy::Instance());
}

TR::ExpAndTy IfExp::Translate(S::Table<E::EnvEntry> *venv,
                              S::Table<TY::Ty> *tenv, TR::Level *level,
                              TEMP::Label *label) const {
  TR::Exp *exp = nullptr;
  TR::ExpAndTy testexpty = this->test->Translate(venv, tenv, level, label);
  TY::Ty *testty = testexpty.ty;
  if(!testty->IsSameType(TY::IntTy::Instance())){
    errormsg.Error(this->pos, "If test exp not int type");
  }

  //check then
  TR::ExpAndTy thenexpty = this->then->Translate(venv, tenv, level, label);
  TY::Ty *thenty = thenexpty.ty;
  if(!thenty->IsSameType(TY::VoidTy::Instance())){
    //errormsg.Error(this->pos, "if-then exp's body must produce no value");
  }
  //check else
  TY::Ty *elsety = nullptr;
  TR::ExpAndTy elseexpty(nullptr, nullptr);
  if(this->elsee != nullptr){
    elseexpty = this->elsee->Translate(venv, tenv, level, label);
    elsety = elseexpty.ty;
    if(!elsety->IsSameType(TY::VoidTy::Instance())){
      //errormsg.Error(this->pos, "if-then exp's body must produce no value");
    }
  }
  if(elsety != nullptr){
    if(!thenty->IsSameType(elsety)){
      errormsg.Error(this->pos, "then exp and else exp type mismatch");
    }
  }
  
  TEMP::Label *thenLabel = TEMP::NewLabel();
  TEMP::Label *elseLabel = TEMP::NewLabel();
  TEMP::Label *endLabel = TEMP::NewLabel();
  TEMP::Temp *result = nullptr;
  if(thenexpty.exp->kind == TR::Exp::NX || thenexpty.ty->kind == TY::Ty::VOID){
    if(this->elsee == nullptr){
      exp = new TR::NxExp(
        new T::SeqStm(
          new T::CjumpStm(
            T::GT_OP,
            testexpty.exp->UnEx(),
            new T::ConstExp(0),
            thenLabel,
            elseLabel
          ),
          new T::SeqStm(
            new T::LabelStm(thenLabel),
            new T::SeqStm(
              thenexpty.exp->UnNx(),
              new T::LabelStm(elseLabel)
            )
          )
        )
      );
    }
    else{
      exp = new TR::NxExp(
        new T::SeqStm(
          new T::CjumpStm(
            T::GT_OP,
            testexpty.exp->UnEx(),
            new T::ConstExp(0),
            thenLabel,
            elseLabel
          ),
          new T::SeqStm(
            new T::LabelStm(thenLabel),
            new T::SeqStm(
              thenexpty.exp->UnNx(),
              new T::SeqStm(
                new T::LabelStm(endLabel),
                new T::SeqStm(
                  new T::LabelStm(elseLabel),
                  new T::SeqStm(
                    elseexpty.exp->UnNx(),
                    new T::LabelStm(endLabel)
                  )
                )
              )
            )
          ) 
        )
      );
    }
  }
  else{
    result = TEMP::Temp::NewTemp();
    exp = new TR::ExExp(
      new T::EseqExp(
        new T::SeqStm(
          new T::CjumpStm(
            T::GT_OP,
            testexpty.exp->UnEx(),
            new T::ConstExp(0),
            thenLabel,
            elseLabel
          ),
          new T::SeqStm(
            new T::LabelStm(thenLabel),
            new T::SeqStm(
              new T::MoveStm(
                new T::TempExp(result),
                thenexpty.exp->UnEx()
              ),
              new T::SeqStm(
                new T::LabelStm(endLabel),
                new T::SeqStm(
                  new T::LabelStm(elseLabel),
                  new T::SeqStm(
                    new T::MoveStm(
                      new T::TempExp(result),
                      elseexpty.exp->UnEx()
                    ),
                    new T::LabelStm(endLabel)
                  )
                )
              )
            )
          )
        ),
        new T::TempExp(result)
      )
    );
  }
  return TR::ExpAndTy(exp, thenty);
}

TR::ExpAndTy WhileExp::Translate(S::Table<E::EnvEntry> *venv,
                                 S::Table<TY::Ty> *tenv, TR::Level *level,
                                 TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  TEMP::Label *testLabel = TEMP::NewLabel();
  TEMP::Label *bodyLabel = TEMP::NewLabel();
  TEMP::Label *doneLabel = TEMP::NewLabel();
  TR::Exp *exp = nullptr;
  TR::ExpAndTy testexpty = this->test->Translate(venv, tenv, level, label);
  TY::Ty *testty = testexpty.ty;
  if (!testty->IsSameType(TY::IntTy::Instance())) {
    errormsg.Error(this->pos, "test error type");
  }
  //check body
  TR::ExpAndTy bodyexpty = this->body->Translate(venv, tenv, level, doneLabel);
  TY::Ty *bodyty = bodyexpty.ty;
  if (!bodyty->IsSameType(TY::VoidTy::Instance())) {
    errormsg.Error(this->pos, "while body must produce no value");
  }

  exp = new TR::NxExp(
    new T::SeqStm(
      new T::LabelStm(testLabel),
      new T::SeqStm(
        new T::CjumpStm(
          T::GT_OP,
          testexpty.exp->UnEx(),
          new T::ConstExp(0),
          bodyLabel,
          doneLabel
        ),
        new T::SeqStm(
          new T::LabelStm(bodyLabel),
          new T::SeqStm(
            bodyexpty.exp->UnNx(),
            new T::SeqStm(
              new T::JumpStm(
                new T::NameExp(testLabel),
                new TEMP::LabelList(testLabel, nullptr)
              ),
              new T::LabelStm(doneLabel)
            )
          )
        )
      )
    )
  );

  return TR::ExpAndTy(exp, TY::VoidTy::Instance());
}

TR::ExpAndTy ForExp::Translate(S::Table<E::EnvEntry> *venv,
                               S::Table<TY::Ty> *tenv, TR::Level *level,
                               TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  TR::Exp *exp = nullptr;
  TEMP::Label *testLabel = TEMP::NewLabel();
  TEMP::Label *bodyLabel = TEMP::NewLabel();
  TEMP::Label *doneLabel = TEMP::NewLabel();
  venv->BeginScope();
  tenv->BeginScope();
  //check range
  TR::ExpAndTy lowexpty = this->lo->Translate(venv, tenv, level, label);
  TR::ExpAndTy highexpty = this->hi->Translate(venv, tenv, level, label);

  TY::Ty *lowty = lowexpty.ty;
  TY::Ty *highty = highexpty.ty;
  if (!lowty->IsSameType(TY::IntTy::Instance())) {
    errormsg.Error(this->pos, "for exp's range type is not integer");
  }
  if (!highty->IsSameType(TY::IntTy::Instance())) {
    errormsg.Error(this->pos, "for exp's range type is not integer");
  }
  TR::Access *i_access = TR::Access::AllocLocal(level, true);
  venv->Enter(this->var, new E::VarEntry(i_access, TY::IntTy::Instance(), false));
  tenv->Enter(this->var, TY::IntTy::Instance());

  //check body
  TR::ExpAndTy bodyexpty = this->body->Translate(venv, tenv, level, doneLabel);
  TY::Ty *bodyty = bodyexpty.ty;
  if (!bodyty->IsSameType(TY::VoidTy::Instance())) {
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

  TEMP::Temp *highTemp = TEMP::Temp::NewTemp();
  
  exp = new TR::NxExp(
    new T::SeqStm(
      new T::MoveStm(
        i_access->access->ToExp(new T::TempExp(F::FP())),
        lowexpty.exp->UnEx()
      ),
      new T::SeqStm(
        new T::MoveStm(
          new T::TempExp(highTemp),
          highexpty.exp->UnEx()
        ),
        new T::SeqStm(
          new T::LabelStm(testLabel),
          new T::SeqStm(
            new T::CjumpStm(
              T::LE_OP,
              i_access->access->ToExp(new T::TempExp(F::FP())),
              new T::TempExp(highTemp),
              bodyLabel,
              doneLabel
            ),
            new T::SeqStm(
              new T::LabelStm(bodyLabel),
              new T::SeqStm(
                bodyexpty.exp->UnNx(),
                new T::SeqStm(
                  new T::MoveStm(
                    i_access->access->ToExp(new T::TempExp(F::FP())),
                    new T::BinopExp(
                      T::PLUS_OP,
                      i_access->access->ToExp(new T::TempExp(F::FP())),
                      new T::ConstExp(1)
                    )
                  ),
                  new T::SeqStm(
                    new T::JumpStm(
                      new T::NameExp(testLabel),
                      new TEMP::LabelList(testLabel, nullptr)
                    ),
                    new T::LabelStm(doneLabel)
                  )
                )
              )
            )
          )
        )
      )
    )
  );

  return TR::ExpAndTy(exp, TY::VoidTy::Instance());
}

TR::ExpAndTy BreakExp::Translate(S::Table<E::EnvEntry> *venv,
                                 S::Table<TY::Ty> *tenv, TR::Level *level,
                                 TEMP::Label *label) const {
  // TODO: Put your codes here (lab5).
  TR::Exp *exp = new TR::NxExp(
    new T::JumpStm(
      new T::NameExp(label),
      new TEMP::LabelList(label, nullptr)
    )
  );
  return TR::ExpAndTy(exp, TY::VoidTy::Instance());
}

TR::ExpAndTy LetExp::Translate(S::Table<E::EnvEntry> *venv,
                               S::Table<TY::Ty> *tenv, TR::Level *level,
                               TEMP::Label *label) const {
  TR::ExExp *exp = nullptr;
  TY::Ty *ty = nullptr;
  
  venv->BeginScope();
  tenv->BeginScope();

  for(auto it = this->decs; it; it = it->tail){
    TR::Exp *decsexp = it->head->Translate(venv, tenv, level, label);
    if(exp == nullptr){
      exp = new TR::ExExp(new T::EseqExp(decsexp->UnNx(), nullptr));
    }
    else {
      T::EseqExp *cur = (T::EseqExp *)(exp->exp);
      while(cur->exp != nullptr){
        cur = (T::EseqExp *)cur->exp;
      }
      cur->exp = new T::EseqExp(
        decsexp->UnNx(), 
        nullptr
      );
    }
  }

  TR::ExpAndTy bodyexpty = this->body->Translate(venv, tenv, level, label);
  ty = bodyexpty.ty;
  if(exp == nullptr){
    exp = new TR::ExExp(bodyexpty.exp->UnEx());
  }
  else {
    T::EseqExp *cur = (T::EseqExp *)exp->exp;
    while(cur->exp != nullptr){
      cur = (T::EseqExp *)cur->exp;
    }
    cur->exp = bodyexpty.exp->UnEx();
  } 

  venv->EndScope();
  tenv->EndScope();

  return TR::ExpAndTy(exp, ty);
}

TR::ExpAndTy ArrayExp::Translate(S::Table<E::EnvEntry> *venv,
                                 S::Table<TY::Ty> *tenv, TR::Level *level,
                                 TEMP::Label *label) const {
  TR::ExExp *exp = nullptr;
  TY::Ty *ty = tenv->Look(this->typ);
  
  if(ty->kind != TY::Ty::ARRAY){
    if(ty->kind == TY::Ty::NAME){
      if(((TY::NameTy *)ty)->ty->kind != TY::Ty::ARRAY){
        errormsg.Error(this->pos, "not array");
        return TR::ExpAndTy(new TR::ExExp(new T::ConstExp(0)), TY::VoidTy::Instance());
      }
      else{
        ty = ((TY::NameTy *)ty)->ty;
      }
    }
    else{
      errormsg.Error(this->pos, "not array");
      return TR::ExpAndTy(new TR::ExExp(new T::ConstExp(0)), TY::VoidTy::Instance());
    }
  }

  TR::ExpAndTy initexpty = this->init->Translate(venv, tenv, level, label);
  TR::ExpAndTy sizeexpty = this->size->Translate(venv, tenv, level, label); 
  TY::Ty *expsty = initexpty.ty;
  TY::Ty *arrayty = ((TY::ArrayTy *)ty)->ty;
  if(!arrayty->IsSameType(expsty)){
    errormsg.Error(this->pos, "type mismatch");
  }

  TEMP::Temp *temp = TEMP::Temp::NewTemp();

  exp = new TR::ExExp(
    new T::EseqExp(
      new T::MoveStm(
        new T::TempExp(temp),
        F::externalCall(
          level->frame,
          new T::NameExp(TEMP::NamedLabel("initArray")),
          new T::ExpList(sizeexpty.exp->UnEx(), new T::ExpList(initexpty.exp->UnEx(), nullptr))
        )
      ),
      new T::TempExp(temp)
    )
  );

  return TR::ExpAndTy(exp, ty); 
}

TR::ExpAndTy VoidExp::Translate(S::Table<E::EnvEntry> *venv,
                                S::Table<TY::Ty> *tenv, TR::Level *level,
                                TEMP::Label *label) const {
  return TR::ExpAndTy(new TR::ExExp(new T::ConstExp(0)), TY::VoidTy::Instance());
}

TR::Exp *FunctionDec::Translate(S::Table<E::EnvEntry> *venv,
                                S::Table<TY::Ty> *tenv, TR::Level *level,
                                TEMP::Label *label) const {
  TR::Exp *exp = nullptr;
  std::vector<TEMP::Label *> funLabelList;

  std::vector<S::Symbol *> redec;
  for(auto it = this->functions; it; it = it->tail){
    funLabelList.push_back(TEMP::NamedLabel(it->head->name->Name()));
    TY::Ty *ty = nullptr, *vy = nullptr;
    if(it->head->result == nullptr){
      ty = TY::VoidTy::Instance();
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
      }
      if(flag){
        errormsg.Error(this->pos, "two functions have the same name");
      }
    }
    else{
      tenv->Enter(it->head->name, ty);
    }

    if(venv->Look(it->head->name) != nullptr){
      venv->Set(it->head->name, new E::FunEntry(level, funLabelList[funLabelList.size() - 1], make_formal_tylist(tenv ,it->head->params), ty));
    }
    else{
      venv->Enter(it->head->name, new E::FunEntry(level, funLabelList[funLabelList.size() - 1], make_formal_tylist(tenv ,it->head->params), ty));
    }
    redec.push_back(it->head->name);
  }

  int i_funlabel = 0;
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

    U::BoolList *formalsEscape = nullptr, *cur = nullptr;

    for(auto i = it->head->params; i; i = i ->tail){
      if(formalsEscape == nullptr){
        formalsEscape = new U::BoolList(i->head->escape, nullptr);
        cur = formalsEscape;
      }
      else
      {
        cur->tail = new U::BoolList(i->head->escape, nullptr);
        cur = cur->tail;
      }
    }

    //静态链作为第一个参数
    formalsEscape = new U::BoolList(true, formalsEscape);
    TR::Level *newLevel = TR::Level::NewLevel(level, funLabelList[i_funlabel], formalsEscape);
    TR::NxExp *funexp = new TR::NxExp(
      new T::MoveStm(
        new T::TempExp(
          F::RV()
        ),
        nullptr
      )
    );
    TR::procEntryExit(newLevel, funexp);//此函数必须要在函数体开始翻译前进行
    
    TR::AccessList *k = newLevel->Formals()->tail;//跳过第一个静态链参数

    for(auto i = it->head->params; i; i = i ->tail){
      venv->Enter(i->head->name, new E::VarEntry(k->head, tenv->Look(i->head->name), false));
      k = k->tail;
    }

    TR::ExpAndTy expty = it->head->body->Translate(venv, tenv, newLevel, label);
    if(it->head->result != nullptr){
      if(expty.ty->kind != tenv->Look(it->head->result)->kind){
        errormsg.Error(this->pos, "function %s body type error", it->head->name->Name().c_str(), int(expty.ty->kind));
      }
    }
    else{
      if(expty.ty->kind != TY::Ty::VOID){
        errormsg.Error(this->pos, "procedure returns value");
      }
    }
    //增加返回值
    ((T::MoveStm *)funexp->stm)->src = expty.exp->UnEx();
 
    
    venv->EndScope();
    tenv->EndScope();

    i_funlabel++;
  }
      
  exp = new TR::ExExp(new T::ConstExp(0));

  return exp;
}

TR::Exp *VarDec::Translate(S::Table<E::EnvEntry> *venv, S::Table<TY::Ty> *tenv,
                           TR::Level *level, TEMP::Label *label) const {
  TR::Exp *exp = nullptr;
  TR::ExpAndTy initexpty = this->init->Translate(venv, tenv, level, label);
  
  TY::Ty *varty = nullptr, *initexpsty = initexpty.ty;
  if(this->typ != nullptr){
    varty = tenv->Look(this->typ);
    if(varty->kind == TY::Ty::NAME){
      varty = ((TY::NameTy *)varty)->ty;
    }

    if(!varty->IsSameType(initexpsty)){
      if(initexpsty->kind != TY::Ty::NIL){
        errormsg.Error(this->pos, "type mismatch varty: %d, exp: %d", varty->kind, initexpsty->kind);
      }
    }
    if(varty->kind == TY::Ty::RECORD && initexpsty->kind == TY::Ty::RECORD){
      if(varty != initexpsty){
        //errormsg.Error(this->pos, "type mismatch");
      }
    }
    if(varty->kind == TY::Ty::ARRAY && initexpsty->kind == TY::Ty::ARRAY){
      if(varty != initexpsty){
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
      tenv->Set(this->var, initexpsty);
      if(initexpsty->kind == TY::Ty::NIL){
        errormsg.Error(this->pos, "init should not be nil without type specified");
      }
    }
    else{
      tenv->Enter(this->var, initexpsty);
      if(initexpsty->kind == TY::Ty::NIL){
        errormsg.Error(this->pos, "init should not be nil without type specified");
      }
    }
  }

  TR::Access *access = TR::Access::AllocLocal(level, true);

  venv->Enter(this->var, new E::VarEntry(access, varty, false));

  T::TempExp *tempExp = new T::TempExp(TEMP::Temp::NewTemp());
  exp = new TR::NxExp(
    new T::MoveStm(
      access->access->ToExp(
        new T::TempExp(F::FP())
      ),
      initexpty.exp->UnEx()
    )
  );

  return exp;
}

TR::Exp *TypeDec::Translate(S::Table<E::EnvEntry> *venv, S::Table<TY::Ty> *tenv,
                            TR::Level *level, TEMP::Label *label) const {
  TR::Exp *exp = nullptr;

  for(auto it = this->types; it; it = it->tail){
    TY::Ty *ty = it->head->ty->Translate(tenv);
    if(tenv->Look(it->head->name) != nullptr){
      tenv->Set(it->head->name, ty);
    }
    else{
      tenv->Enter(it->head->name, ty);
    }
  }

  TY::Ty *headty, *decty;
  bool it_flag = true;
  for(auto it = this->types; it && it_flag; it = it->tail){
    headty = tenv->Look(it->head->name);
    decty = it->head->ty->Translate(tenv);
    
    if(headty->kind == TY::Ty::NAME){
      TY::Ty *actualty = nullptr;
      if(((TY::NameTy *)headty)->ty == nullptr){
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
            errormsg.Error(this->pos, "undefined namety: %s", it->head->name->Name().c_str());
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
            i->head->ty = ty;
          }
        }
      }
    }
  }

  exp = new TR::ExExp(new T::ConstExp(0));
  
  return exp;
}

TY::Ty *NameTy::Translate(S::Table<TY::Ty> *tenv) const {
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
    }
  }
  return ty;
}

TY::Ty *RecordTy::Translate(S::Table<TY::Ty> *tenv) const {
  TY::Ty *ty;
  if(this->record == nullptr){
    ty = new TY::RecordTy(nullptr);
  }
  else {
    for(auto it = this->record; it; it = it->tail){
      ty = tenv->Look(it->head->typ);
      if(ty == nullptr){
        tenv->Enter(it->head->typ, new TY::NameTy(it->head->typ, nullptr));
      }
    }
    ty = new TY::RecordTy(make_fieldlist(tenv, this->record));
  }
  return ty;
}

TY::Ty *ArrayTy::Translate(S::Table<TY::Ty> *tenv) const {
  TY::Ty *arrayty = tenv->Look(this->array);
  if(arrayty == nullptr){
    errormsg.Error(this->pos, "array type undefined");
  }
  return new TY::ArrayTy(arrayty);
}

}  // namespace A
