%filenames = "scanner"

 /*
  * Please don't modify the lines above.
  */

 /* You can add lex definitions here. */

%x COMMENT STR

%%

 /*
  * TODO: Put your codes here (lab2).
  *
  * Below is examples, which you can wipe out
  * and write regular expressions and actions of your own.
  *
  * All the tokens:
  *   Parser::ID
  *   Parser::STRING
  *   Parser::INT
  *   Parser::COMMA
  *   Parser::COLON
  *   Parser::SEMICOLON
  *   Parser::LPAREN
  *   Parser::RPAREN
  *   Parser::LBRACK
  *   Parser::RBRACK
  *   Parser::LBRACE
  *   Parser::RBRACE
  *   Parser::DOT
  *   Parser::PLUS
  *   Parser::MINUS
  *   Parser::TIMES
  *   Parser::DIVIDE
  *   Parser::EQ
  *   Parser::NEQ
  *   Parser::LT
  *   Parser::LE
  *   Parser::GT
  *   Parser::GE
  *   Parser::AND
  *   Parser::OR
  *   Parser::ASSIGN
  *   Parser::ARRAY
  *   Parser::IF
  *   Parser::THEN
  *   Parser::ELSE
  *   Parser::WHILE
  *   Parser::FOR
  *   Parser::TO
  *   Parser::DO
  *   Parser::LET
  *   Parser::IN
  *   Parser::END
  *   Parser::OF
  *   Parser::BREAK
  *   Parser::NIL
  *   Parser::FUNCTION
  *   Parser::VAR
  *   Parser::TYPE
  */

 /*
  * skip white space chars.
  * space, tabs and LF
  */

<INITIAL> [ \t]+ {adjust();}
<INITIAL> "\n" {adjust(); errormsg.Newline();}

 /* reserved words */
<INITIAL> "var" {adjust(); return Parser::VAR;}
<INITIAL> "type" {adjust(); return Parser::TYPE;}
<INITIAL> "let" {adjust(); return Parser::LET;}
<INITIAL> "in" {adjust(); return Parser::IN;}
<INITIAL> "end" {adjust(); return Parser::END;}
<INITIAL> "array" {adjust(); return Parser::ARRAY;}
<INITIAL> "of" {adjust(); return Parser::OF;}
<INITIAL> "function" {adjust(); return Parser::FUNCTION;}
<INITIAL> "if" {adjust(); return Parser::IF;}
<INITIAL> "then" {adjust(); return Parser::THEN;}
<INITIAL> "else" {adjust(); return Parser::ELSE;}
<INITIAL> "nil" {adjust(); return Parser::NIL;}
<INITIAL> "do" {adjust(); return Parser::DO;}
<INITIAL> "while" {adjust(); return Parser::WHILE;}
<INITIAL> "for" {adjust(); return Parser::FOR;}
<INITIAL> "to" {adjust(); return Parser::TO;}
<INITIAL> "break" {adjust(); return Parser::BREAK;}

 /* OPERATION */
<INITIAL> "+" {adjust(); return Parser::PLUS;}
<INITIAL> "-" {adjust(); return Parser::MINUS;}
<INITIAL> "*" {adjust(); return Parser::TIMES;}
<INITIAL> "/" {adjust(); return Parser::DIVIDE;}
<INITIAL> "=" {adjust(); return Parser::EQ;}
<INITIAL> ":=" {adjust(); return Parser::ASSIGN;}
<INITIAL> ">" {adjust(); return Parser::GT;}
<INITIAL> "<" {adjust(); return Parser::LT;}
<INITIAL> ">=" {adjust(); return Parser::GE;}
<INITIAL> "<=" {adjust(); return Parser::LE;}
<INITIAL> "<>" {adjust(); return Parser::NEQ;}
<INITIAL> "&" {adjust(); return Parser::AND;}
<INITIAL> "|" {adjust(); return Parser::OR;}


/* Separater */
<INITIAL> "." {adjust(); return Parser::DOT;}
<INITIAL> ":" {adjust(); return Parser::COLON;}
<INITIAL> "," {adjust(); return Parser::COMMA;}
<INITIAL> ";" {adjust(); return Parser::SEMICOLON;}
<INITIAL> "(" {adjust(); return Parser::LPAREN;}
<INITIAL> ")" {adjust(); return Parser::RPAREN;}
<INITIAL> "[" {adjust(); return Parser::LBRACK;}
<INITIAL> "]" {adjust(); return Parser::RBRACK;}
<INITIAL> "{" {adjust(); return Parser::LBRACE;}
<INITIAL> "}" {adjust(); return Parser::RBRACE;}

 /* ID AND INT */
<INITIAL> [a-z|A-Z][a-z|A-Z|0-9|_]* {adjust(); return Parser::ID;}
<INITIAL> [0-9]+ {adjust(); return Parser::INT;} 

 /* TO COMMENT */
<INITIAL> "/*" {adjust(); comment_counts = 0; begin(StartCondition__::COMMENT);}

 /* TO STR */
<INITIAL> "\"" {more(); begin(StartCondition__::STR);}

 /* ERROR */
<INITIAL> . {adjust(); errormsg.Error(errormsg.tokPos, "illegal token");}

 /* COMMENT TO INITIAL */
<COMMENT> "*/" {
                  adjust();
                  if(comment_counts == 0)
                    begin(StartCondition__::INITIAL);
                  else
                    comment_counts--;
               }
<COMMENT> "/*" {adjust(); comment_counts++;}
<COMMENT> '\n' {adjust();}
<COMMENT> . {adjust();}

 /* STR TO INITIAL */
<STR> "\"" {
              adjust();
              changeStrMatched();//change the text matched
              begin(StartCondition__::INITIAL);
              return Parser::STRING;
           }
<STR> "\\"["\"] {more();}
<STR> "\n" {more();}
<STR> . {more();}
