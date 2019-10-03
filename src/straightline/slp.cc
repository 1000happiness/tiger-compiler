#include "straightline/slp.h"

namespace A {
int A::CompoundStm::MaxArgs() const {
  int left_max_args = this->stm1->MaxArgs();
  int right_max_args = this->stm2->MaxArgs();
  return (left_max_args >= right_max_args ? left_max_args : right_max_args);
}

Table *A::CompoundStm::Interp(Table *t) const {
  Table *temp_table = nullptr, *return_table = nullptr;
  temp_table = this->stm1->Interp(t);
  return_table = this->stm2->Interp(temp_table);
  return return_table;
}

int A::AssignStm::MaxArgs() const {
  return this->exp->MaxArgs();
}

Table *A::AssignStm::Interp(Table *t) const {
  IntAndTable *result = this->exp->Interp(t);
  Table *return_table = result->t->Update(this->id, result->i);
  return return_table;
}

int A::PrintStm::MaxArgs() const {
  ExpList * now_exp = this->exps;
  int out = 1;
  int in = now_exp->MaxArgs();
  while(now_exp->Getkind() == PAIR_EXP_LIST){
    out++;
    now_exp = now_exp->Gettail();
  }
  return (out > in ? out : in);
}

Table *A::PrintStm::Interp(Table *t) const {
  
  ExpList * now_exp = this->exps;
  Table *now_table = nullptr;
  IntAndTable *temp_result = new IntAndTable(0, t);
  while(now_exp->Getkind() == PAIR_EXP_LIST){
    temp_result = now_exp->Interp(temp_result->t);
    std::cout << temp_result->i << ' ';
    now_exp = now_exp->Gettail();
  }
  temp_result = now_exp->Interp(temp_result->t);
  std::cout << temp_result->i << std::endl;
  return temp_result->t;
}

int A::IdExp::MaxArgs() const {
  return 0;
}

IntAndTable *A::IdExp::Interp(Table *t) const {
  int i = t->Lookup(this->id);
  return new IntAndTable(i, t);
}

int A::NumExp::MaxArgs() const {
  return 0;
}

IntAndTable *A::NumExp::Interp(Table *t) const {
  return new IntAndTable(this->num, t);
}

int A::OpExp::MaxArgs() const {
  int left_max_args = this->left->MaxArgs();
  int right_max_args = this->right->MaxArgs();
  return (left_max_args >= right_max_args ? left_max_args : right_max_args);
}

IntAndTable *A::OpExp::Interp(Table *t) const {
  int i = 0;
  int temp_left_value = 0;
  int temp_right_value = 0;
  IntAndTable *temp_result = new IntAndTable(0, t);

  temp_result = this->left->Interp(temp_result->t);
  temp_left_value = temp_result->i;
  temp_result = this->right->Interp(temp_result->t);
  temp_right_value = temp_result->i;

  switch (this->oper)
  {
  case PLUS:
    //std::cout <<temp_left_value << "+" <<temp_right_value;
    i = temp_left_value + temp_right_value;
    break;
  case MINUS:
    i = temp_left_value - temp_right_value;
    break;
  case TIMES:
    i = temp_left_value * temp_right_value;
    break;
  case DIV:
    i = temp_left_value / temp_right_value;
    break;
  default:
    break;
  }
  return new IntAndTable(i, temp_result->t);
}

int A::EseqExp::MaxArgs() const {
  int left_max_args = this->stm->MaxArgs();
  int right_max_args = this->exp->MaxArgs();
  return (left_max_args >= right_max_args ? left_max_args : right_max_args);
}

IntAndTable *A::EseqExp::Interp(Table *t) const {
  IntAndTable *temp_result = new IntAndTable(0, t);
  temp_result->t = this->stm->Interp(temp_result -> t);
  temp_result = this->exp->Interp(temp_result->t);
  return temp_result;
}

int A::PairExpList::MaxArgs() const {
  int left_max_args = this->head->MaxArgs();
  int right_max_args = this->tail->MaxArgs();
  return (left_max_args >= right_max_args ? left_max_args : right_max_args);
}

IntAndTable *A::PairExpList::Interp(Table *t) const {
  IntAndTable *temp_result = new IntAndTable(0, t);
  temp_result = this->head->Interp(temp_result->t);
  return temp_result;
}

ExpListKind A::PairExpList::Getkind() const {
  return this->kind;
}

ExpList *A::PairExpList::Gettail() const {
  return this->tail;
}

int A::LastExpList::MaxArgs() const {
  return this->last->MaxArgs();
}

IntAndTable *A::LastExpList::Interp(Table *t) const {
  IntAndTable *temp_result = new IntAndTable(0, t);
  temp_result = this->last->Interp(temp_result->t);
  return temp_result;
}

ExpListKind A::LastExpList::Getkind() const {
  return this->kind;
}

ExpList *A::LastExpList::Gettail() const {
  return nullptr;
}

int Table::Lookup(std::string key) const {
  if (id == key) {
    return value;
  } else if (tail != nullptr) {
    return tail->Lookup(key);
  } else {
    assert(false);
  }
}

Table *Table::Update(std::string key, int value) const {
  return new Table(key, value, this);
}
}  // namespace A


