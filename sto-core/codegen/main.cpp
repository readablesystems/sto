#include <iostream>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <vector>
#include <utility>
#include <string>

#include <map>
#include <set>

#include "driver.hpp"

const std::string TName_str[] = {"int64_t", "int32_t", "float", "var_string", "fix_string"};

unsigned int integer_log2(uint64_t input) {
    if (input == 0 && input == 1)
        return 0;
    input -= 1;
    unsigned int log = 0;
    while (input) { ++log; input >>= 1; }
    return log;
}

std::string cxx_type_name(const FieldType& t) {
    std::stringstream ss;
    ss << TName_str[t.tname];
    assert(t.tname >= BigInt && t.tname <= Char);
    if (t.tname == VarChar || t.tname == Char) {
        ss << '<' << t.len << '>';
    }
    return ss.str();
}

uint64_t cpp_col_cell_map_uint64(StructSpec& result) {
    std::vector<int> ints;
    auto& fields = result.fields;
    auto& groups = result.groups;

    int gidx_width = integer_log2(groups.size());
    if ((gidx_width * fields.size()) > 64) {
        std::cerr << "CodeGen Error: mapping information does not fit uint64_t" << std::endl;
        abort();
    }

    for (auto& f : fields) {
        size_t gidx = 0;
        for (auto& g : groups) {
            if (std::find(g.begin(), g.end(), f.name) != g.end()) {
                // found in group/cell no. gidx
                ints.push_back(gidx);
                break;
            }
            ++gidx;
        }
    }

    uint64_t map_val = 0;
    int shift = 0;
    for (auto i : ints) {
        map_val += i << shift;
        shift += gidx_width;
    }

    return map_val;
}

void print_single(StructSpec &result) {
   using namespace std;

   cout << "struct name: " << result.struct_name << endl;

   auto &fields = result.fields;
   cout << "Number of fields: " << fields.size() << endl;
   for (size_t i = 0; i < fields.size(); ++i) {
      cout << fields[i].name << " " << cxx_type_name(fields[i].t) << endl;
   }

   auto &groups = result.groups;
   cout << "Number of groups: " << groups.size() << endl;
   for (size_t i = 0; i < groups.size(); ++i) {
      for (size_t j = 0; j < groups[i].size(); j++) {
         cout << groups[i][j] << " ";
      }
      cout << endl;
   }
}

void print(std::vector<StructSpec> &result) {
   std::cout << "Printing result..." << std::endl;
   for (auto &spec : result) {
      print_single(spec);
   }
}

bool type_check_single(StructSpec &result) {
    std::set<std::string> field_name_set;
    auto& struct_name = result.struct_name;
    auto& fields = result.fields;
    for (auto& f : fields) {
        if (field_name_set.find(f.name) != field_name_set.end()) {
            std::cerr << "Error: Struct " << struct_name << " has a duplicate field name \"" << f.name << "\"" << std::endl;
            return false;
        }
        field_name_set.insert(f.name);
    }

    assert(field_name_set.size() == fields.size());

    auto& groups = result.groups;
    std::set<std::string> group_fname_set;
    size_t gfields = 0;
    for (auto& g : groups) {
        for (auto& fn : g) {
            if (field_name_set.find(fn) == field_name_set.end()) {
                std::cerr << "Error: Struct " << struct_name << " has a field \"" << fn << "\" referenced but undeclared" << std::endl;
                return false;
            }
            if (group_fname_set.find(fn) != group_fname_set.end()) {
                std::cerr << "Error: Struct " << struct_name << " has a field \"" << fn << "\" referenced in multiple groups" << std::endl;
                return false;
            }
            group_fname_set.insert(fn);
        }
        gfields += g.size();
    }

    assert(group_fname_set.size() == field_name_set.size());
    assert(group_fname_set.size() == gfields);

    return true;
}

bool type_check(std::vector<StructSpec> &result) {
   for (auto &spec : result) {
      if (!type_check_single(spec)) {
         return false;
      }
   }
   return true;
}

void generate_code_single_struct(StructSpec &result) {
    std::stringstream ss;
    const std::string idt = "    ";
    std::string struct_name = result.struct_name;

    auto& fields = result.fields;
    ss << "// Definitions for row type: " << struct_name << std::endl;

    // Generate struct declaration
    ss << "struct " << struct_name << " {" << std::endl << std::endl;

    ss << idt << "enum class NamedColumn : int { ";
    size_t i = 0;
    for (auto& f : fields) {
        ss << f.name;
        if (i == 0)
            ss << " = 0";
        if (i != (fields.size() - 1))
            ss << ", ";
        ++i;
    }
    ss << " };" << std::endl << std::endl;

    for (auto& f : fields)
        ss << idt << cxx_type_name(f.t) << ' ' << f.name << ';' << std::endl;
    ss << "};" << std::endl;

    ss << std::endl;
    std::cout << ss.str() << std::endl;
}


void generate_code_single_versel(StructSpec &result) {
    std::stringstream ss;
    const std::string idt = "    ";
    auto& struct_name = result.struct_name;
    auto& groups = result.groups;
    auto gidx_width = integer_log2(groups.size());

    // Generate VerSel
    ss << "template <typename VersImpl>" << std::endl;
    ss << "class VerSel<" << struct_name << ", VersImpl> : public VerSelBase<VerSel<" << struct_name << ", VersImpl>, VersImpl> {" << std::endl;
    ss << "public:" << std::endl;
    ss << idt << "typedef VersImpl version_type;" << std::endl;
    ss << idt << "static constexpr size_t num_versions = " << groups.size() << ';' << std::endl << std::endl;

    ss << idt << "explicit VerSel(type v) : vers_() { (void)v; }" << std::endl;
    ss << idt << "VerSel(type v, bool insert) : vers_() { (void)v; (void)insert; }" << std::endl << std::endl;

    ss << idt << "static int map_impl(int col_n) {" << std::endl;
    ss << idt << idt << "uint64_t mask = ~(~0ul << vidx_width) ;" << std::endl;
    ss << idt << idt << "int shift = col_n * vidx_width;" << std::endl;
    ss << idt << idt << "return static_cast<int>((col_cell_map & (mask << shift)) >> shift);" << std::endl;
    ss << idt << '}' << std::endl << std::endl;

    ss << idt << "version_type& version_at_impl(int cell) {" << std::endl;
    ss << idt << idt << "return vers_[cell];" << std::endl;
    ss << idt << '}' << std::endl << std::endl;

    ss << idt << "void install_by_cell_impl(" << struct_name << " *dst, const " << struct_name << " *src, int cell) {" << std::endl;
    ss << idt << idt << "switch (cell) {" << std::endl;

    size_t idx = 0;
    for (auto& g : groups) {
        ss << idt << idt << "case " << idx << ':' << std::endl;
        for (auto& fn : g)
            ss << idt << idt << idt << "dst->" << fn << " = " << "src->" << fn << ';' << std::endl;
        ss << idt << idt << idt << "break;" << std::endl;
        ++idx;
    }

    ss << idt << idt << "default:" << std::endl;
    ss << idt << idt << idt << "always_assert(false, \"cell id out of bound\\n\");" << std::endl;
    ss << idt << idt << idt << "break;" << std::endl;
    ss << idt << idt << '}' << std::endl;
    ss << idt << '}' << std::endl << std::endl;

    ss << "private:" << std::endl;
    ss << idt << "version_type vers_[num_versions];" << std::endl;
    ss << idt << "static constexpr unsigned int vidx_width = " << gidx_width << "u;" << std::endl;
    ss << idt << "static constexpr uint64_t col_cell_map = " << cpp_col_cell_map_uint64(result) << "ul;" << std::endl;

    ss << "};" << std::endl << std::endl;
    std::cout << ss.str() << std::endl;
}



void generate_code(std::vector<StructSpec> &result) {
    std::cout << "#pragma once" << std::endl << std::endl;

    // Print comments
    std::cout << "// The following code is automatically generated by Hao & Yihe's parser/codegen"  << std::endl;
    std::cout << "// Please do not manually modify!" << std::endl << std::endl;

    for (auto &spec : result) {
        generate_code_single_struct(spec);
    }

    std::cout << std::endl << "namespace ver_sel {" << std::endl << std::endl;
    for (auto &spec : result) {
        generate_code_single_versel(spec);
    }
    std::cout << "}; // namespace ver_sel" << std::endl;
}

int main(const int argc, const char **argv) {
    /** check for the right # of arguments **/
    std::vector<StructSpec> result;
    if( argc == 2 ) {
        MC::MC_Driver driver;
        /** example for piping input from terminal, i.e., using cat **/
        if( std::strncmp( argv[ 1 ], "-o", 2 ) == 0 ) {
            driver.parse( std::cin, result );
        }
        /** simple help menu **/
        else if( std::strncmp( argv[ 1 ], "-h", 2 ) == 0 ) {
            std::cout << "use -o for pipe to std::cin\n";
            std::cout << "just give a filename to count from a file\n";
            std::cout << "use -h to get this menu\n";
            return( EXIT_SUCCESS );
        }
        /** example reading input from a file **/
        else {
            /** assume file, prod code, use stat to check **/
            driver.parse( argv[1], result );
        }
    } else {
        /** exit with failure condition **/
        return ( EXIT_FAILURE );
    }

    
    if (result.empty()) {
        std::cout << "No structures identified." << std::endl;
        return ( EXIT_FAILURE );     
    }

    // print(result);

    if (!type_check(result)) {
        std::cout << "Type checker error: Exited." << std::endl;
        return ( EXIT_FAILURE );
    }

    generate_code(result);

    return( EXIT_SUCCESS );
}
