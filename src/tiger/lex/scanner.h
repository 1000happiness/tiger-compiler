#ifndef TIGER_LEX_SCANNER_H_
#define TIGER_LEX_SCANNER_H_

#include <algorithm>
#include <string>
#include <sstream>

#include "scannerbase.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/parse/parserbase.h"

extern EM::ErrorMsg errormsg;

class Scanner : public ScannerBase {
 public:
  explicit Scanner(std::istream &in = std::cin, std::ostream &out = std::cout);

  Scanner(std::string const &infile, std::string const &outfile);

  int lex();

  int comment_counts = 0;

 private:
  int lex__();
  int executeAction__(size_t ruleNr);

  void print();
  void preCode();
  void postCode(PostEnum__ type);
  void adjust();
  void adjustStr();
  void changeStrMatched();

  int commentLevel_;
  std::string stringBuf_;
  int charPos_;
};

inline Scanner::Scanner(std::istream &in, std::ostream &out)
    : ScannerBase(in, out), charPos_(1) {}

inline Scanner::Scanner(std::string const &infile, std::string const &outfile)
    : ScannerBase(infile, outfile), charPos_(1) {}

inline int Scanner::lex() { return lex__(); }

inline void Scanner::preCode() {
  // optionally replace by your own code
}

inline void Scanner::postCode(PostEnum__ type) {
  // optionally replace by your own code
}

inline void Scanner::print() { print__(); }

inline void Scanner::adjust() {
  errormsg.tokPos = charPos_;
  charPos_ += length();
}

inline void Scanner::adjustStr() { charPos_ += length(); }

/* this function is called after adjust(), 
 * it will change the text matched(eg. str") in folowing two ways.
 * 1: delete the " in the first and last position
 * 2: translate \****(eg. \n, \111) if it exists*/
inline void Scanner::changeStrMatched(){
  
  std::string matched_string(matched());
  //delete the " in the first position
  matched_string = matched_string.substr(1);
  
  //delete the " in the last position
  matched_string.pop_back();
  //std::cout<<"matched"<<matched_string<<std::endl;
  //translate \****(like \n, \111) if it exists
  
  std::stringstream matched_stream(matched_string);
  
  std::string temp;
  //std::getline(matched_stream,temp);
  //std::cout<<"temp"<<temp<<std::endl;
  int temp_int;
  char temp_char1, temp_char2;
  std::string changed_string;
  while(true){
    temp_char1=matched_stream.get();
    //std::cout<<"1:"<<int(temp_char1)<<std::endl;
    if(matched_stream.eof()){
      break;
    }
    if(temp_char1 != '\\'){
      changed_string.push_back(temp_char1);
    }
    else{
      temp_char2 = matched_stream.get();
      if(temp_char2 >= '0' && temp_char2 <= '9'){
        matched_stream.putback(temp_char2);
        matched_stream >> temp_int;       
        changed_string.push_back(char(temp_int));
      }
      else {
        //std::cout<<"2:"<<temp_char2<<std::endl;
        switch (temp_char2)
        {
        case '^':
          temp_char1 = matched_stream.get();
          //std::cout<<temp_char1<<":"<< temp_char1 - 'A' + 1 <<std::endl;
          if(temp_char1 == ' '){
            changed_string.push_back(char(32));
          }
          else if(temp_char1 == '?'){
            changed_string.push_back(char(127));
          }
          else{
            changed_string.push_back(char(temp_char1 - '@'));
          }
          break;
        case 'n':            
          changed_string.push_back('\n');
          break;
        case 't':
          changed_string.push_back('\t');
          break;
        case '\"':
          changed_string.push_back('\"');
          break;
        case '\\':
          changed_string.push_back('\\');
          //std::cout<<"???"<<std::endl;
          break;
        case 'x':
          matched_stream >> std::hex >> temp_int;
          matched_stream >> std::dec;
          changed_string.push_back(char(temp_int));
          break;
        default:
          //std::cout<<"!!!"<<std::endl;
          while(true){
            matched_stream >> temp_char1;
            //std::cout<<int(temp_char1)<<":";
            if(temp_char1 == '\\'){
              break;
            }
            else if(matched_stream.eof()){
              errormsg.Error(errormsg.tokPos, "illegal string");
              break;
            }
          }
          break;
        }
      }
    }
    
  }

  setMatched(changed_string);
}

#endif  // TIGER_LEX_SCANNER_H_

