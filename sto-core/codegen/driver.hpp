#ifndef __MCDRIVER_HPP__
#define __MCDRIVER_HPP__ 1

#include <string>
#include <cstddef>
#include <istream>

#include "scanner.hpp"
#include "parser.tab.hh"


namespace MC{

class MC_Driver{
public:
   MC_Driver() = default;

   virtual ~MC_Driver();
   
   /** 
    * parse - parse from a file
    * @param filename - valid string with input file
    */
   void parse( const char * const filename, std::vector<StructSpec> &result );
   /** 
    * parse - parse from a c++ input stream
    * @param is - std::istream&, valid input stream
    */
   void parse( std::istream &iss, std::vector<StructSpec> &result );

private:

   void parse_helper( std::istream &stream, std::vector<StructSpec> &result );

   MC::MC_Parser  *parser  = nullptr;
   MC::MC_Scanner *scanner = nullptr;
  
};

} /* end namespace MC */
#endif /* END __MCDRIVER_HPP__ */
