#include <iostream>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <utility>
#include <string>

#include "driver.hpp"


bool type_check(std::pair<std::vector<std::string>,std::vector<std::vector<std::string>>> &result) {
   return false;
}

void generate_code(std::pair<std::vector<std::string>,std::vector<std::vector<std::string>>> &result) {
   
}



int 
main( const int argc, const char **argv )
{
   /** check for the right # of arguments **/
   std::pair<std::vector<std::string>,std::vector<std::vector<std::string>>> result;
   if( argc == 2 )
   {
      MC::MC_Driver driver;
      /** example for piping input from terminal, i.e., using cat **/ 
      if( std::strncmp( argv[ 1 ], "-o", 2 ) == 0 )
      {
         driver.parse( std::cin, result );
      }
      /** simple help menu **/
      else if( std::strncmp( argv[ 1 ], "-h", 2 ) == 0 )
      {
         std::cout << "use -o for pipe to std::cin\n";
         std::cout << "just give a filename to count from a file\n";
         std::cout << "use -h to get this menu\n";
         return( EXIT_SUCCESS );
      }
      /** example reading input from a file **/
      else
      {
         /** assume file, prod code, use stat to check **/
         driver.parse( argv[1], result );
      }
   }
   else
   {
      /** exit with failure condition **/
      return ( EXIT_FAILURE );
   }

   if (!type_check(result)) {
      std::cout << "Type  checker error" << std::endl;
   }
   generate_code(result);


   return( EXIT_SUCCESS );
}
