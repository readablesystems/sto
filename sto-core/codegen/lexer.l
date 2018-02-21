%{
/* C++ string header, for string ops below */
#include <string>

/* Implementation of yyFlexScanner */ 
#include "scanner.hpp"
#undef  YY_DECL
#define YY_DECL int MC::MC_Scanner::yylex( MC::MC_Parser::semantic_type * const lval, MC::MC_Parser::location_type *loc )

/* typedef to make the returns for the tokens shorter */
using token = MC::MC_Parser::token;

/* define yyterminate as this instead of NULL */
#define yyterminate() return( token::END )

/* msvc2010 requires that we exclude this header file. */
#define YY_NO_UNISTD_H

/* update location on matching */
#define YY_USER_ACTION loc->step(); loc->columns(yyleng);

%}

%option debug
%option nodefault
%option yyclass="MC::MC_Scanner"
%option noyywrap
%option c++

%%
%{          /** Code executed at the beginning of yylex **/
            yylval = lval;
%}

@           {
                return( token::AT );
            }

@fields     {
                return( token::FIELDS );
            }

@groups     {
                return( token::GROUPS );
            }

@name       {
                return ( token::NAME );
            }

\(          {
                return ( token::LPAREN );
            }

\)          {
                return ( token::RPAREN );
            }

\{          {
                return ( token::LBRACE );
            }

\}          {
                return ( token::RBRACE );
            }

:           {
                return ( token::COLON );
            }

,           {
                return ( token::COMMA );
            }


BIGINT      {
                return ( token::BIGINT );
            }

SMALLINT    {
                return ( token::SMALLINT );
            }

FLOAT       {
                return ( token::FLOAT );
            }

VARCHAR     {
                return ( token::VARCHAR );
            }

CHAR        {
                return ( token::CHAR );
            }

[0-9]+      {
                yylval->build<int>(atoi(yytext));
                return ( token::NUMBER );
            }
  
[a-zA-Z_][a-zA-Z0-9_]+ {
               /**
                * Section 10.1.5.1 of the 3.0.2 Bison Manual says the 
                * following should work:
                * yylval.build( yytext );
                * but it doesn't.
                * ref: http://goo.gl/KLn0w2
                */
               yylval->build< std::string >( yytext );
               return( token::IDENTIFIER );
            }

\n          {
               // Update line number
               loc->lines();
            }

.           {
            }

%%


