#include "tiger/frame/frame.h"

#include <string>
/*
 * x64frame的frame结构如下
 * 
 * static link    调用前%rsp的位置
 * -----------
 * ret address
 * -----------
 * 接收的参数
 * -----------
 * calleesaves
 * -----------
 * 临时变量
 * -----------
 * callersaves    //相当于把callersaves作为一种特殊的临时变量进行保护
 * -----------
 * spill变量
 * -----------
 * 传递的参数      调用后%rsp的位置
 * -----------
 * 
 * 如果是叶子节点，则如下
 * static link    调用前%rsp的位置
 * -----------
 * ret address
 * -----------
 * 接收的参数
 * -----------
 * calleesaves
 * -----------
 * 临时变量
 * -----------
 * spill变量        调用后%rsp的位置   
 * 
 */
namespace F {

const int wordSize = 4;
const int addressSize = 8;

class X64Frame : public Frame {
  public:
    F::AccessList *formals;

    Access *allocLocal(bool escape);
    AccessList *getFormals();
    TEMP::Label *getLabel();
    std::string namedFrameLength();
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

Access *X64Frame::allocLocal(bool escape){

  Access *access = nullptr;
  if(escape){
    access = new InFrameAccess(- this->length - addressSize);
    this->length = this->length + addressSize;
  }
  else{
    TEMP::Temp *temp = TEMP::Temp::NewTemp();
    access = new InRegAccess(temp);
  }
  return access;
}

AccessList *X64Frame::getFormals(){
  return this->formals;
}

TEMP::Label *X64Frame::getLabel(){
  return this->label;
}

std::string X64Frame::namedFrameLength(){
  return std::string(this->getLabel()->Name()) + std::string("_frameSize");
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
Frame *newFrame(TEMP::Label *name, U::BoolList *formalsEscape){
  
  X64Frame *x64frame = new X64Frame();
  x64frame->label = name;
  x64frame->length = 0;
  x64frame->maxFrameArgsNumber = 0;
  int offset = 0;
  AccessList *tempAccessList = nullptr, *cur = nullptr;

  U::BoolList *escapeptr = formalsEscape;
  TEMP::TempList *args = argsReg();
  for(; escapeptr; escapeptr = escapeptr->tail)
  {
    Access *access = nullptr;
    if(args != nullptr){
      access = new InRegAccess(args->head);
      args = args->tail;
    }
    else{
      access = new InFrameAccess(offset);
      offset += addressSize;
    }

    if(tempAccessList == nullptr){
      tempAccessList = new AccessList(access, nullptr); 
      cur = tempAccessList;
    }
    else{
      cur->tail = new AccessList(access, nullptr); 
      cur = cur->tail;
    }
  }
  x64frame->formals = tempAccessList;

  return x64frame;
}

//此处为了让externalCall的调用方式与T::CallExp相同，对接口进行了调整
T::Exp *externalCall(Frame *frame, T::NameExp *fun, T::ExpList *args){
  T::Exp *exp = normalCall(frame, fun, args);
  return exp;
}

//将Call通过frame实现是因为可能需要保存callersaves
T::Exp *normalCall(Frame *frame, T::NameExp *fun, T::ExpList *args){
  TEMP::TempList *callerSaves = CallerSaves();
  
  T::SeqStm *saveRegStms = nullptr;
  T::SeqStm *loadRegStms = nullptr;
  T::SeqStm *saveCur = nullptr;
  T::SeqStm *loadCur = nullptr;

  for(TEMP::TempList *it = callerSaves; it; it = it->tail){
    Access *access = frame->allocLocal(true);
    T::MoveStm *saveStm = new T::MoveStm(
      access->ToExp(new T::TempExp(FP())),
      new T::TempExp(it->head)
    );
    T::MoveStm *loadStm = new T::MoveStm(
      new T::TempExp(it->head),
      access->ToExp(new T::TempExp(FP()))
    );
    if(it == callerSaves){//第一个
      saveRegStms = new T::SeqStm(
        saveStm,
        nullptr
      );
      saveCur = saveRegStms;
      loadRegStms = new T::SeqStm(
        loadStm,
        nullptr
      );
      loadCur = loadRegStms;
    }
    else if(it->tail == nullptr){//最后一个
      saveCur->right = saveStm;
      loadCur->right = loadStm;
    }
    else{
      saveCur->right = new T::SeqStm(
        saveStm,
        nullptr
      );
      saveCur = (T::SeqStm *)saveCur->right;
      loadCur->right = new T::SeqStm(
        loadStm,
        nullptr
      );
      loadCur = (T::SeqStm *)loadCur->right;
    }
  }

  T::Exp *exp = nullptr;

  exp = new T::EseqExp(
    new T::SeqStm(
      saveRegStms,
      new T::SeqStm(
        new T::MoveStm(
          new T::TempExp(RV()),
          new T::CallExp(fun, args)
        ),
        loadRegStms
      )
    ),
    new T::TempExp(RV())
  );

  return exp;
}
//保护调用者保护的寄存器，在这个步骤完成之后，栈指针指向内存中保存的最后一个调用者保存的寄存器

T::Stm *F_procEntryExit1(Frame *frame, T::Stm *body){
  //接收参数的指令
  T::SeqStm *prologue = nullptr;
  T::SeqStm *prologueCur = nullptr;
  AccessList *formals = frame->getFormals();
  for(AccessList *it = formals; it; it = it->tail) {
    Access *access = frame->allocLocal(true);
    T::MoveStm *saveStm = new T::MoveStm(
      access->ToExp(new T::TempExp(FP())),
      it->head->ToExp(new T::TempExp(FP()))
    );
    it->head = access;
    if(prologue == nullptr){
      prologue = new T::SeqStm(
        saveStm,
        nullptr
      );
      prologueCur = prologue;
    }
    else{
      prologueCur->right = new T::SeqStm(
        saveStm,
        nullptr
      );
      prologueCur = (T::SeqStm *)prologueCur->right;
    }
  }
  
  //将被调用者保存的寄存器存入栈帧中，并在结尾恢复这些寄存器，这个文件中已经明确所使用的架构为X64，所以calleesave的寄存器数量必然为6个（大于1个）
  TEMP::TempList *calleeSaves = CalleeSaves();
  T::SeqStm *saveRegStms = nullptr;
  T::SeqStm *loadRegStms = nullptr;
  T::SeqStm *saveCur = nullptr;
  T::SeqStm *loadCur = nullptr;
  for(TEMP::TempList *it = calleeSaves; it; it = it->tail){
    Access *access = frame->allocLocal(false);
    T::MoveStm *saveStm = new T::MoveStm(
      access->ToExp(nullptr),
      new T::TempExp(it->head)
    );
    T::MoveStm *loadStm = new T::MoveStm(
      new T::TempExp(it->head),
      access->ToExp(nullptr)
    );
    if(it == calleeSaves){//第一个
      saveRegStms = new T::SeqStm(
        saveStm,
        nullptr
      );
      saveCur = saveRegStms;
      loadRegStms = new T::SeqStm(
        loadStm,
        nullptr
      );
      loadCur = loadRegStms;
    }
    else if(it->tail == nullptr){//最后一个
      saveCur->right = saveStm;
      loadCur->right = loadStm;
    }
    else{
      saveCur->right = new T::SeqStm(
        saveStm,
        nullptr
      );
      saveCur = (T::SeqStm *)saveCur->right;
      loadCur->right = new T::SeqStm(
        loadStm,
        nullptr
      );
      loadCur = (T::SeqStm *)loadCur->right;
    }
  }

  prologueCur->right = saveRegStms;
  return new T::SeqStm(
    prologue,
    new T::SeqStm(
      body,
      loadRegStms
    )
  );
}

AS::InstrList *F_procEntryExit2(AS::InstrList *body){
  static TEMP::TempList *returnSink = nullptr;
  if(returnSink == nullptr){
    returnSink = 
      new TEMP::TempList(F::RV(),
      new TEMP::TempList(F::SP(), 
      CalleeSaves()));
  }
  return AS::InstrList::Splice(body, new AS::InstrList(new AS::OperInstr("", nullptr, returnSink, nullptr), nullptr));
}

AS::Proc *F_procEntryExit3(Frame *frame, AS::InstrList *il){
  std::stringstream prologueStream;
  TEMP::Label *newLabel = TEMP::NewLabel();
  prologueStream << frame->label->Name() << ":\n";
  prologueStream << newLabel->Name() << ":\n";
  prologueStream << "subq $"<< frame->length <<", %rsp\n";
  
  //将framelength赋值
  AS::InstrList *it = il;
  while (it->tail != nullptr)
  {
    it = it->tail;
    
    std::string *assemAddress = nullptr;
    switch (it->head->kind)
    {
    case AS::Instr::OPER: 
      assemAddress = &((AS::OperInstr *)it->head)->assem;
      break;
    case AS::Instr::MOVE: 
      assemAddress = &((AS::MoveInstr *)it->head)->assem;
      break;
    case AS::Instr::LABEL: 
      assemAddress = &((AS::LabelInstr *)it->head)->assem;
      break;
    default:
      break;
    }
    std::stringstream frameAssemstream;
    int frameLengthLocation = (*assemAddress).find(frame->namedFrameLength());
    if(frameLengthLocation == -1){
      continue;
    }
    int addLocation = (*assemAddress).find("+");
    int bracketLocation = (*assemAddress).find("(");
    frameAssemstream << (*assemAddress).substr(0, frameLengthLocation);
    if(bracketLocation - 1 > addLocation){
      std::stringstream tempstream((*assemAddress).substr(addLocation + 1, bracketLocation - addLocation - 1));
      int offset = 0;
      tempstream >> offset;
      // fprintf(stdout, "%d\n",offset);
      // fflush(stdout);
      frameAssemstream << offset + frame->length;
      frameAssemstream << (*assemAddress).substr(bracketLocation);
    }
    else{
      // fprintf(stdout, "???%s\n", (*assemAddress).c_str());
      // fflush(stdout);
      if(bracketLocation == -1){
        frameAssemstream << frame->length;
        frameAssemstream << (*assemAddress).substr((*assemAddress).find(","));
      }
      else{
        frameAssemstream << frame->length;
        frameAssemstream << (*assemAddress).substr(bracketLocation);
      }
    }
    (*assemAddress) = frameAssemstream.str();
  }

  newLabel = TEMP::NewLabel();
  std::stringstream epilogStream;
  
  epilogStream << "addq $"<< frame->length <<",%rsp\n";
  epilogStream << "retq\n";
  epilogStream << newLabel->Name() << ":\n";
  return new AS::Proc(prologueStream.str(), il, epilogStream.str());
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

TEMP::Temp *FP_REAL(){
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

TEMP::Map * FrameTempMap(){
  static TEMP::Map *tempMap = nullptr;
  if(tempMap == nullptr){
    tempMap = TEMP::Map::Empty();
    tempMap->Enter(RV(), new std::string("%rax"));
    tempMap->Enter(RB(), new std::string("%rbx"));
    tempMap->Enter(RC(), new std::string("%rcx"));
    tempMap->Enter(RD(), new std::string("%rdx"));
    tempMap->Enter(SI(), new std::string("%rsi"));
    tempMap->Enter(DI(), new std::string("%rdi"));
    tempMap->Enter(FP(), new std::string("%rbp"));
    tempMap->Enter(FP_REAL(), new std::string("%rbp"));
    tempMap->Enter(SP(), new std::string("%rsp"));
    tempMap->Enter(R8(), new std::string("%r8"));
    tempMap->Enter(R9(), new std::string("%r9"));
    tempMap->Enter(R10(), new std::string("%r10"));
    tempMap->Enter(R11(), new std::string("%r11"));
    tempMap->Enter(R12(), new std::string("%r12"));
    tempMap->Enter(R13(), new std::string("%r13"));
    tempMap->Enter(R14(), new std::string("%r14"));
    tempMap->Enter(R15(), new std::string("%r15"));
    tempMap->Enter(R16(), new std::string("%r16"));
  }
  return tempMap;
}

TEMP::TempList *argsReg(){
  static TEMP::TempList *argsTempList = nullptr;
  if(argsTempList == nullptr){
    argsTempList = 
      new TEMP::TempList(DI(),
      new TEMP::TempList(SI(),
      new TEMP::TempList(RC(),
      new TEMP::TempList(RD(),
      new TEMP::TempList(R8(),
      new TEMP::TempList(R9(), 
      nullptr))))));
  }
  return argsTempList;
}

TEMP::TempList *CallerSaves(){
  static TEMP::TempList *CallerSavesTempList = nullptr;
  if(CallerSavesTempList == nullptr){
    CallerSavesTempList = 
      new TEMP::TempList(R10(),
      new TEMP::TempList(R11(),
      new TEMP::TempList(RC(),
      new TEMP::TempList(RD(),
      new TEMP::TempList(R9(),
      nullptr)))));
  }
  return CallerSavesTempList;
}

TEMP::TempList *CalleeSaves(){
  static TEMP::TempList *CalleeSavesTempList = nullptr;
  if(CalleeSavesTempList == nullptr){
    CalleeSavesTempList =  
      new TEMP::TempList(RB(),
      new TEMP::TempList(FP_REAL(),
      new TEMP::TempList(R12(),
      new TEMP::TempList(R13(),
      new TEMP::TempList(R14(),
      new TEMP::TempList(R15(), 
      nullptr))))));
  }
  return CalleeSavesTempList;
}

}  // namespace F