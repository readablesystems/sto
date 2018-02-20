%skeleton "lalr1.cc"
%require  "3.0"
%debug 
%defines 
%define api.namespace {MC}
%define parser_class_name {MC_Parser}

%code requires{
   namespace MC {
      class MC_Driver;
      class MC_Scanner;
   }

// The following definitions is missing when %locations isn't used
# ifndef YY_NULLPTR
#  if defined __cplusplus && 201103L <= __cplusplus
#   define YY_NULLPTR nullptr
#  else
#   define YY_NULLPTR 0
#  endif
# endif

  #include <string>

  enum FieldTypeName { BigInt = 0, SmallInt, VarChar, Char };

  struct FieldType {
    FieldTypeName tname;
    int len;
  };

  struct Field {
    FieldType t;
    std::string name;
  }; 
}

%parse-param { MC_Scanner  &scanner  }
%parse-param { MC_Driver  &driver  }
%parse-param { std::pair<std::vector<Field>,std::vector<std::vector<std::string>>> &result }

%code{
   #include <iostream>
   #include <cstdlib>
   #include <fstream>
   
   /* include for all driver functions */
   #include "driver.hpp"

#undef yylex
#define yylex scanner.yylex
}

%define api.value.type variant
%define parse.assert

%token FIELDS GROUPS LBRACE RBRACE COLON COMMA AT
%token BIGINT SMALLINT VARCHAR CHAR LPAREN RPAREN
%token END 0 "end of file"
%token <std::string> IDENTIFIER
%token <int> NUMBER

%type <Field> field
%type <FieldType> field_type
%type <std::string> field_name
%type <std::vector<Field>> field_list
%type <std::vector<std::string>> field_name_list
%type <std::vector<Field>> field_spec
%type <std::vector<std::string>> group
%type <std::vector<std::vector<std::string>>> group_list
%type <std::vector<std::vector<std::string>>> group_spec
%type <std::pair<std::vector<Field>,std::vector<std::vector<std::string>>>> spec


%locations


%% /* Spec Grammr */

spec
  : AT AT AT field_spec group_spec AT AT AT END
    { result = std::make_pair($4, $5); }
  ;

field_spec
  : FIELDS COLON LBRACE field_list RBRACE
    { $$ = $4; }
  ;

group_spec
  : GROUPS COLON LBRACE group_list RBRACE
    { $$ = $4; }
  ;

field_list
  : field
    { $$ = std::vector<Field>(1, $1); }
  | field_list COMMA field
    { $1.push_back($3);
      $$ = $1; 
    }
  ;

group_list
  : group
    { $$ = std::vector<std::vector<std::string>>(1, $1); }
  | group_list COMMA group
    { $1.push_back($3); 
      $$ = $1;
    }
  ;

group
  : LBRACE field_name_list RBRACE
    { $$ = $2; }
  ;

field
  : field_name LPAREN field_type RPAREN
    { $$ = { $3, $1 }; }
  ;

field_name_list
  : field_name
    { $$ = std::vector<std::string>(1, $1); }
  | field_name_list COMMA field_name
    { $1.push_back($3);
      $$ = $1; 
    }
  ;

field_name
  : IDENTIFIER
    { $$ = $1; }
  ;

field_type
  : BIGINT
    { $$ = { BigInt, 0 }; }
  | SMALLINT
    { $$ = { SmallInt, 0 }; }
  | VARCHAR LPAREN NUMBER RPAREN
    { $$ = { VarChar, $3 }; }
  | CHAR LPAREN NUMBER RPAREN
    { $$ = { Char, $3 }; }
  ;

%% /* Spec Grammr */


void 
MC::MC_Parser::error( const location_type &l, const std::string &err_message )
{
   std::cerr << "Error: " << err_message << " at " << l << "\n";
}
