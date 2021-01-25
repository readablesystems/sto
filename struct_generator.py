#!/usr/bin/env python3

import argparse
import configparser
import sys

def output_struct(outfile, struct, sdata):
    '''Output a single struct's variants.'''
    colcount = len(sdata)
    data = {
            'colcount': colcount,
            'lbrace': '{',
            'ns': '{}_datatypes'.format(struct),
            'rbrace': '}',
            'splitstruct': 'split_value',
            'unifiedstruct': 'unified_value',
            'struct': struct,
            }

    outfile.write('namespace {ns} {lbrace}\n\n'.format(**data))

    output_struct_namedcolumns(data, sdata)

    outfile.write('''\
template <size_t StartIndex, size_t EndIndex>
struct {splitstruct};

template <size_t SplitIndex>
struct {unifiedstruct};

'''.format(**data))

    for splitindex in range(colcount):
        output_struct_split_variant(data, sdata, 0, splitindex + 1)
        if splitindex + 1 < colcount:
            output_struct_split_variant(data, sdata, splitindex + 1, colcount)
        output_struct_unified_variant(data, sdata, splitindex + 1)
        outfile.write('\n');

    outfile.write('''\
struct {struct} {lbrace}
    explicit {struct}() = default;

    using NamedColumn = {ns}::NamedColumn;
    
'''.format(**data))

    output_struct_accessors(data, sdata)

    outfile.write('\n    std::variant<')
    for splitindex in range(colcount, 0, -1):
        if splitindex < colcount:
            outfile.write(',')
        outfile.write('\n        {unifiedstruct}<{index}>'.format(
            index=splitindex, **data))
    outfile.write('> value;\n')

    outfile.write('''\
{rbrace};

{rbrace};  // namespace {ns}

using {struct} = {ns}::{struct};
CREATE_ADAPTER({struct}, {colcount});
'''.format(**data))

def output_struct_accessors(data, sdata):
    '''Output the accessors.'''
    for member, ctype in sdata.items():
        for const in ('', 'const '):
            outfile.write(
                    '    {const}{ctype}& {member}() {const}{lbrace}\n'.format(
                        const=const, ctype=ctype, member=member, **data))
            for splitindex in range(data['colcount']):
                if splitindex + 1 < data['colcount']:
                    outfile.write('''\
        if (auto val = std::get_if<{unifiedstruct}<{index}>>(&value)) {lbrace}
            return val->{member}();
        {rbrace}
'''.format(index=splitindex + 1, member=member, **data))
                else:
                    outfile.write('''\
        return std::get<{unifiedstruct}<{index}>>(value).{member}();
'''.format(index=splitindex + 1, member=member, **data))
            outfile.write('    {rbrace}\n'.format(
                ctype=ctype, member=member, **data))

def output_struct_namedcolumns(data, sdata):
    '''Output the NamedColumn for the given type.'''
    outfile.write('enum class NamedColumn : int {lbrace}\n'.format(**data))
    first_member = True
    for member in sdata:
        if first_member:
            outfile.write('    {member} = 0'.format(member=member))
            first_member = False
        else:
            outfile.write(',\n    {member}'.format(member=member))
    if first_member:
        outfile.write('    COLCOUNT = {colcount}\n'.format(**data))
    else:
        outfile.write(',\n    COLCOUNT\n')
    outfile.write('{rbrace};\n\n'.format(**data))

def output_struct_split_variant(data, sdata, startindex, endindex):
    '''Output a single split variant of a struct.

    splitindex indicates the split to output, with 0 indicating the unsplit
    base type.
    '''
    data['start'] = startindex
    data['end'] = endindex
    outfile.write('template <>\n')
    struct_def = 'struct {splitstruct}<{start}, {end}> {lbrace}\n'
    outfile.write(struct_def.format(**data))
    outfile.write('    explicit {splitstruct}() = default;\n\n'.format(
        **data))

    index = 0
    for member, ctype in sdata.items():
        if startindex <= index < endindex:
            outfile.write('    {ctype} {member};\n'.format(
                ctype=ctype, member=member))
        index += 1

    outfile.write('{rbrace};\n'.format(**data))

def output_struct_unified_variant(data, sdata, splitindex):
    '''Output the unified variant of the given split.'''
    data['splitindex'] = splitindex
    outfile.write('template <>\n')
    struct_def = 'struct {unifiedstruct}<{splitindex}> {lbrace}\n'
    outfile.write(struct_def.format(**data))

    index = 0
    for member, ctype in sdata.items():
        for const in ('', 'const '):
            outfile.write(
                    '    {const}{ctype}& {member}() {const}{lbrace}\n'.format(
                        const=const, ctype=ctype, member=member, **data))
            outfile.write(
                    '        return split_{variant}.{member};\n'.format(
                        variant=0 if index < splitindex else 1,
                        member=member,
                        **data))
            outfile.write('    {rbrace}\n'.format(**data))
        index += 1
    outfile.write('\n')

    outfile.write(
            '    {splitstruct}<0, {splitindex}> split_0;\n'.format(**data))
    if splitindex < data['colcount']:
        tstr = '    {splitstruct}<{splitindex}, {colcount}> split_1;\n'
        outfile.write(tstr.format(**data))

    outfile.write('{rbrace};\n'.format(**data))

if '__main__' == __name__:
    parser = argparse.ArgumentParser(description='STO struct generator')
    parser.add_argument(
            'file', type=str, help='INI-style struct definition file')
    parser.add_argument(
            '-o', '--out', default=None, type=str,
            help='Output file for generated structs, defaults to stdout')

    args = parser.parse_args()

    config = configparser.ConfigParser()
    config.read(args.file)

    outfile = open(args.out, 'w') if args.out else sys.stdout

    for struct in config.sections():
        output_struct(outfile, struct, config[struct])

    outfile.close()
