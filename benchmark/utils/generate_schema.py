#!/usr/bin/python3

import json,argparse
from ddlparse import DdlParse

datatype_map = {
    'big_int': 'int64_t',
    'small_int': 'int32_t',
    'float': 'float',
    'FLOAT': 'float',
    'VARCHAR2' : 'var_string',
    'VARCHAR' : 'var_string',
    'CLOB': 'char *'
}

def to_cpp_type(type_name, length, scale):
    if type_name == 'VARCHAR2':
        return '{}<{}>'.format(datatype_map[type_name], length)
    else:
        if type_name == 'NUMBER':
            if scale != None:
                return datatype_map['float']
            elif length > 10:
                return datatype_map['big_int']
            else:
                return datatype_map['small_int']
        else:
            return datatype_map[type_name]

parser = argparse.ArgumentParser(description='Generate STO-compatible database schema definition from DDL.')
parser.add_argument('filename', metavar='file', type=str, help='JSON file contain a list of DDLs')

args = parser.parse_args()

ddl_string_list = []
with open(args.filename, 'r') as ddl_file:
    ddl_string_list = json.load(ddl_file)

def is_integral_type(col):
    cpp_type = to_cpp_type(col.data_type, col.length, col.scale)
    return (cpp_type in ['int32_t', 'int64_t'])

for ddl_string in ddl_string_list:
    table = DdlParse().parse(ddl=ddl_string, source_database=DdlParse.DATABASE.oracle)
    print("// New table: {}\n".format(table.name))

    print("struct __attribute__((packed)) {}_key {{".format(table.name))
    for col in table.columns.values():
        if col.primary_key or col.unique:
            print("    {} {};".format(to_cpp_type(col.data_type, col.length, col.scale), col.name))
    ctr = "    explicit {}_key(".format(table.name)
    param_list = []
    init_list = []
    for col in table.columns.values():
        if col.primary_key or col.unique:
            param_list.append("{} {}".format(to_cpp_type(col.data_type, col.length, col.scale), "p_{}".format(col.name)))
    ctr += ", ".join(param_list)
    ctr += ")\n        : "
    for col in table.columns.values():
        if col.primary_key or col.unique:
            if is_integral_type(col):
                init_list.append("{}(bswap({}))".format(col.name, "p_{}".format(col.name)))
            else:
                init_list.append("{}(({}))".format(col.name, "p_{}".format(col.name)))
    ctr += ", ".join(init_list)
    ctr += " {}"
    print(ctr)
    print("};\n")

    print("struct {}_row {{".format(table.name))
    for col in table.columns.values():
        if not col.primary_key and not col.unique:
            print("    {} {};".format(to_cpp_type(col.data_type, col.length, col.scale), col.name))
    print("};\n")
