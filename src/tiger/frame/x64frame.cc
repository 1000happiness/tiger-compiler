#include "tiger/frame/frame.h"

#include <string>

namespace F {

const int wordSize = 4;
const int addressSize = 8;

class X64Frame : public Frame {
  public:
    F::AccessList *formals;
    F::AccessList *locals;

    int length;

    Access *allocLocal(bool escape, int size);
    AccessList *getFormals();
    TEMP::Label *getLabel();
};

class InFrameAccess : public Access {
 public:
  int offset;

  InFrameAccess(int offset) : Access(INFRAME), offset(offset) {}

  T::Exp* ToExp(T::Exp* framePtr) const;
};

class InRegAccess : public Access {
 public:
  TEMP::Temp* reg;

  InRegAccess(TEMP::Temp* reg) : Access(INREG), reg(reg) {}

  T::Exp* ToExp(T::Exp* framePtr) const;
};

//此处对课本上的接口进行了更改，因为如果传入参数的时候没有参数的大小，那么frame中
//的内存只能分配4byte，但是目前的实现理论上只能满足size = 4或者size = 8的内存分配
//在实现tiger时这样做没有问题
Access *X64Frame::allocLocal(bool escape, int size = wordSize){

  Access *access = nullptr;
  if(escape){
    access = new InFrameAccess(this->length - size);
  }
  else{
    TEMP::Temp *temp = TEMP::Temp::NewTemp();
    access = new InRegAccess(temp);
  }
  this->length = this->length - size;
  return access;
}

AccessList *X64Frame::getFormals(){
  return this->formals;
}

TEMP::Label *X64Frame::getLabel(){
  return this->label;
}

T::Exp* InFrameAccess::ToExp(T::Exp* framePtr) const{
  T::Exp *exp = new T::MemExp(
    new T::BinopExp(
      T::PLUS_OP,
      framePtr,
      new T::ConstExp(this->offset)
    )
  );
  return exp;
}

T::Exp* InRegAccess::ToExp(T::Exp* framePtr) const{
  return new T::TempExp(this->reg);
}

/*--------------------------------------------------------------*/
//此处对课本上的接口进行了更改，因为如果传入参数的时候没有参数的大小，那么frame中
//需要为静态链专门为第一个参数分配8byte的offset，后面的参数都是4byte的offset，frame
//的实现与静态链的实现没有解耦
Frame *newFrame(TEMP::Label *name, U::BoolList *formalsEscape, U::IntList *formalsOffset){
  //此处给formalsOffset赋默认值只是为了保持调用接口和课本描述的相同
  if(formalsOffset == nullptr){
    for(auto it = formalsEscape; it; it = it->tail){
      if(it == formalsEscape){
        formalsOffset = new U::IntList(addressSize, formalsOffset);
      }
      else{
        formalsOffset = new U::IntList(wordSize, formalsOffset);
      }
    }
  }
  
  X64Frame *x64frame = new X64Frame();
  x64frame->label = name;
  x64frame->length = 0;
  int offset = 0;
  AccessList *tempAccessList = nullptr;

  U::IntList *offsetptr = formalsOffset;
  U::BoolList *escapeptr = formalsEscape;
  for(; offsetptr && escapeptr; escapeptr = escapeptr->tail, offsetptr = offsetptr->tail)
  {
    Access *access = nullptr;
    access = new InFrameAccess(offsetptr->head);
    tempAccessList = new AccessList(access, tempAccessList);    
  }
  x64frame->formals = tempAccessList;

  return x64frame;
}

//此处为了让externalCall的调用方式与T::CallExp相同，对接口进行了调整
T::Exp *externalCall(T::NameExp *fun, T::ExpList *args){
  T::Exp *exp = new T::CallExp(fun, args);
  return exp;
}

T::Stm *F_procEntryExit1(Frame *frame, T::Stm *body){
  return body;
}

AS::InstrList *F_procEntryExit2(AS::InstrList *body){
  static TEMP::TempList *returnSink = nullptr;
  if(returnSink == nullptr){
    returnSink = 
      new TEMP::TempList(F::RV(),
      new TEMP::TempList(F::SP(), 
      CalleeSaved()));
  }
  return AS::InstrList::Splice(body, new AS::InstrList(new AS::OperInstr("", nullptr, returnSink, nullptr), nullptr));
}

AS::Proc *F_procEntryExit3(Frame *frame, AS::InstrList *il){
  return nullptr;
}

TEMP::Temp *RV(){
  static TEMP::Temp *rv = nullptr;
	if (rv == nullptr) {
		rv = TEMP::Temp::NewTemp();
	}
	return rv;
}

TEMP::Temp *RB(){
  static TEMP::Temp *rb = nullptr;
	if (rb == nullptr) {
		rb = TEMP::Temp::NewTemp();
	}
	return rb;
}

TEMP::Temp *RC(){
  static TEMP::Temp *rc = nullptr;
	if (rc == nullptr) {
		rc = TEMP::Temp::NewTemp();
	}
	return rc;
}

TEMP::Temp *RD(){
  static TEMP::Temp *rd = nullptr;
	if (rd == nullptr) {
		rd = TEMP::Temp::NewTemp();
	}
	return rd;
}

TEMP::Temp *SI(){
  static TEMP::Temp *si = nullptr;
	if (si == nullptr) {
		si = TEMP::Temp::NewTemp();
	}
	return si;
}

TEMP::Temp *DI(){
  static TEMP::Temp *di = nullptr;
	if (di == nullptr) {
		di = TEMP::Temp::NewTemp();
	}
	return di;
}

TEMP::Temp *FP(){
  static TEMP::Temp *fp = nullptr;
	if (fp == nullptr) {
		fp = TEMP::Temp::NewTemp();
	}
	return fp;
}

TEMP::Temp *SP(){
  static TEMP::Temp *sp = nullptr;
	if (sp == nullptr) {
		sp = TEMP::Temp::NewTemp();
	}
	return sp;
}

TEMP::Temp *R8(){
  static TEMP::Temp *r8 = nullptr;
	if (r8 == nullptr) {
		r8 = TEMP::Temp::NewTemp();
	}
	return r8;
}

TEMP::Temp *R9(){
  static TEMP::Temp *r9 = nullptr;
	if (r9 == nullptr) {
		r9 = TEMP::Temp::NewTemp();
	}
	return r9;
}

TEMP::Temp *R10(){
  static TEMP::Temp *r10 = nullptr;
	if (r10 == nullptr) {
		r10 = TEMP::Temp::NewTemp();
	}
	return r10;
}

TEMP::Temp *R11(){
  static TEMP::Temp *r11 = nullptr;
	if (r11 == nullptr) {
		r11 = TEMP::Temp::NewTemp();
	}
	return r11;
}

TEMP::Temp *R12(){
  static TEMP::Temp *r12 = nullptr;
	if (r12 == nullptr) {
		r12 = TEMP::Temp::NewTemp();
	}
	return r12;
}

TEMP::Temp *R13(){
  static TEMP::Temp *r13 = nullptr;
	if (r13 == nullptr) {
		r13 = TEMP::Temp::NewTemp();
	}
	return r13;
}

TEMP::Temp *R14(){
  static TEMP::Temp *r14 = nullptr;
	if (r14 == nullptr) {
		r14 = TEMP::Temp::NewTemp();
	}
	return r14;
}

TEMP::Temp *R15(){
  static TEMP::Temp *r15 = nullptr;
	if (r15 == nullptr) {
		r15 = TEMP::Temp::NewTemp();
	}
	return r15;
}

TEMP::Temp *R16(){
  static TEMP::Temp *r16 = nullptr;
	if (r16 == nullptr) {
		r16 = TEMP::Temp::NewTemp();
	}
	return r16;
}

TEMP::TempList *argsReg(){
  static TEMP::TempList *argsTempList = nullptr;
  if(argsTempList == nullptr){
    argsTempList = 
      new TEMP::TempList(DI(),
      new TEMP::TempList(SI(),
      new TEMP::TempList(RD(),
      new TEMP::TempList(RC(),
      new TEMP::TempList(R8(),
      new TEMP::TempList(R9(), 
      nullptr))))));
  }
  return argsTempList;
}

TEMP::TempList *CallerSaved(){
  static TEMP::TempList *callerSavedTempList = nullptr;
  if(callerSavedTempList == nullptr){
    callerSavedTempList = 
      new TEMP::TempList(R10(),
      new TEMP::TempList(R11(),
      nullptr));
  }
  return callerSavedTempList;
}

TEMP::TempList *CalleeSaved(){
  static TEMP::TempList *calleeSavedTempList = nullptr;
  if(calleeSavedTempList == nullptr){
    calleeSavedTempList =  
      new TEMP::TempList(RB(),
      new TEMP::TempList(FP(),
      new TEMP::TempList(R12(),
      new TEMP::TempList(R13(),
      new TEMP::TempList(R14(),
      new TEMP::TempList(R15(), 
      nullptr))))));
  }
  return calleeSavedTempList;
}

}  // namespace F