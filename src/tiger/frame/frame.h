#ifndef TIGER_FRAME_FRAME_H_
#define TIGER_FRAME_FRAME_H_

#include <string>

#include "tiger/codegen/assem.h"
#include "tiger/translate/tree.h"
#include "tiger/util/util.h"


namespace F {

extern const int wordSize;
extern const int addressSize;

class Access {
 public:
  enum Kind { INFRAME, INREG };

  Kind kind;

  Access(Kind kind) : kind(kind) {}
  
  virtual T::Exp* ToExp(T::Exp* framePtr) const = 0;
};

class AccessList {
 public:
  Access *head;
  AccessList *tail;

  AccessList(Access *head, AccessList *tail) : head(head), tail(tail) {}
};

class Frame {
  public:
    TEMP::Label *label;
    virtual Access *allocLocal(bool escape, int size) = 0;
    virtual AccessList *getFormals() = 0;
    virtual TEMP::Label *getLabel() = 0;
};

/*
 * Fragments
 */

class Frag {
 public:
  enum Kind { STRING, PROC };

  Kind kind;

  Frag(Kind kind) : kind(kind) {}
};

class StringFrag : public Frag {
 public:
  TEMP::Label *label;
  std::string str;

  StringFrag(TEMP::Label *label, std::string str)
      : Frag(STRING), label(label), str(str) {}
};

class ProcFrag : public Frag {
 public:
  T::Stm *body;
  Frame *frame;

  ProcFrag(T::Stm *body, Frame *frame) : Frag(PROC), body(body), frame(frame) {}
};

class FragList {
 public:
  Frag *head;
  FragList *tail;

  FragList(Frag *head, FragList *tail) : head(head), tail(tail) {}
};

Frame *newFrame(TEMP::Label *name, U::BoolList *formalsEscape, U::IntList *formalsOffset);
T::Exp *externalCall(T::NameExp *fun, T::ExpList *args);

T::Stm *F_procEntryExit1(Frame *frame, T::Stm *body);
AS::InstrList *F_procEntryExit2(AS::InstrList *body);
AS::Proc *F_procEntryExit3(Frame *frame, AS::InstrList *il);

//16个寄存器
TEMP::Temp *RV();//rax
TEMP::Temp *RB();//rbx
TEMP::Temp *RC();//rcx
TEMP::Temp *RD();//rdx
TEMP::Temp *SI();//rsi
TEMP::Temp *DI();//rdi
TEMP::Temp *FP();//fbp
TEMP::Temp *SP();//fsp
TEMP::Temp *R8();//r8
TEMP::Temp *R9();//r9
TEMP::Temp *R10();//r10
TEMP::Temp *R11();//r11
TEMP::Temp *R12();//r12
TEMP::Temp *R13();//r13
TEMP::Temp *R14();//r14
TEMP::Temp *R15();//r15
TEMP::Temp *R16();//r16
TEMP::TempList *argsReg();
TEMP::TempList *CallerSaved();
TEMP::TempList *CalleeSaved();
}  // namespace F

#endif