#!/usr/bin/env python3

import argparse
import configparser
import re
import sys

class Output:
    def __init__(self, writer):
        self._data = {}
        self._indent = 0
        self._writer = writer
        self._tabwidth = 4

    @staticmethod
    def extract_member_type(member):
        '''Pull the base member name and the array "nesting" format.'''
        nesting = '{}'
        while member.rfind(']') >= 0:
            brackets = (member.rfind('['), member.rfind(']'))
            assert brackets[0] < brackets[1]
            nesting = 'std::array<{ctype}, {count}>'.format(
                    ctype=nesting, count=member[brackets[0] + 1 : brackets[1]])
            member = member[:brackets[0]]
        return member, nesting

    def indent(self, amount=1):
        self._indent += amount

    def unindent(self, amount=1):
        self._indent = max(self._indent - amount, 0)

    def write(self, fmstring, **fmargs):
        self._data['indent'] = ' ' * self._tabwidth
        self._writer.write(fmstring.format(**fmargs, **self._data))

    def writei(self, fmstring, **fmargs):
        self.write('{}{}'.format(
            ' ' * self._tabwidth * self._indent if fmstring else '',
            fmstring),
            **fmargs)

    def writeln(self, fmstring='', **fmargs):
        if fmstring:
            self.writei(fmstring + '\n', **fmargs)
        else:
            self.write('\n')

    def writelns(self, fmstring='', **fmargs):
        for line in fmstring.split('\n'):
            self.writeln(line, **fmargs)

    ### Generation methods; hierarchical, not lexicographical

    def convert_struct(self, struct, sdata):
        '''Output a single struct's variants.'''
        self.colcount = len(sdata)
        self._data = {
                'accessorstruct': 'accessor',
                'colcount': self.colcount,
                'lbrace': '{',
                'ns': '{}_datatypes'.format(struct),
                'rbrace': '}',
                'splitstruct': 'split_value',
                'unifiedstruct': 'unified_value',
                'struct': struct,
                }
        self.sdata = sdata

        self.writelns('namespace {ns} {lbrace}\n')

        self.convert_namedcolumns()

        self.writelns('''\
template <size_t ColIndex>
struct {accessorstruct};

template <size_t StartIndex, size_t EndIndex>
struct {splitstruct};

template <size_t SplitIndex>
struct {unifiedstruct};

struct {struct};

CREATE_ADAPTER({struct}, {colcount});
''')

        self.convert_column_accessors()

        for splitindex in range(self.colcount):
            self.convert_split_variant(0, splitindex + 1)
            if splitindex + 1 < self.colcount:
                self.convert_split_variant(splitindex + 1, self.colcount)
            self.convert_unified_variant(splitindex + 1)
            self.writelns();

        self.writeln('struct {struct} {lbrace}')

        self.indent()

        self.writelns('''\
explicit {struct}() = default;

using NamedColumn = {ns}::NamedColumn;

''')

        self.convert_accessors()

        self.writeln('std::variant<')

        self.indent()

        for splitindex in range(self.colcount, 0, -1):
            self.writei('{unifiedstruct}<{index}>', index=splitindex)
            if splitindex > 1:
                self.write(',')
            self.writeln()
        self.writeln('> value;')

        self.unindent(2)

        self.writelns('''\
{rbrace};

{rbrace};  // namespace {ns}

using {struct} = {ns}::{struct};
using ADAPTER_OF({struct}) = ADAPTER_OF({ns}::{struct});
''')

    def convert_namedcolumns(self):
        '''Output the NamedColumn for the given type.'''
        self.writelns('enum class NamedColumn : int {lbrace}')

        self.indent()
        first_member = True
        for member in self.sdata:
            member, _ = Output.extract_member_type(member)
            if first_member:
                self.writeln('{member} = 0,'.format(member=member))
                first_member = False
            else:
                self.writeln('{member},'.format(member=member))
        if first_member:
            self.writeln('COLCOUNT = {colcount}')
        else:
            self.writeln('COLCOUNT')
        self.unindent()

        self.writelns('{rbrace};\n')

    def convert_column_accessors(self):
        '''Output the accessor wrapper for each column.'''

        index = 0
        for ctype in self.sdata.values():
            self.writelns('''\
template <>
struct {accessorstruct}<{index}> {lbrace}\
''', index=index)

            self.indent()

            self.writelns('using type = {ctype};\n', ctype=ctype)

            self.writeln('''\
{accessorstruct}() = default;\
{accessorstruct}(type& value) : value_(value) {lbrace}{rbrace}\
{accessorstruct}(const type& value) : value_((type)value) {lbrace}{rbrace}\
''')

            self.writelns('''\
operator type() {lbrace}
{indent}ADAPTER_OF({struct})::CountRead({index});
{indent}return value_;
{rbrace}''', index=index)

            self.writelns('''\
operator const type() const {lbrace}
{indent}ADAPTER_OF({struct})::CountRead({index});
{indent}return value_;
{rbrace}''', index=index)

            self.writelns('''\
operator type&() {lbrace}
{indent}ADAPTER_OF({struct})::CountRead({index});
{indent}return value_;
{rbrace}''', index=index)

            self.writelns('''\
operator const type&() const {lbrace}
{indent}ADAPTER_OF({struct})::CountRead({index});
{indent}return value_;
{rbrace}''', index=index)

            self.writelns('''\
type operator =(const type& other) {lbrace}
{indent}ADAPTER_OF({struct})::CountWrite({index});
{indent}return value_ = other;
{rbrace}''', index=index)

            self.writelns('''\
type operator =(const {accessorstruct}<{index}>& other) {lbrace}
{indent}ADAPTER_OF({struct})::CountWrite({index});
{indent}return value_ = other.value_;
{rbrace}''', index=index)

            self.writelns('\ntype value_;')

            self.unindent()

            self.writeln('{rbrace};')

            index += 1
        self.writeln()

    def convert_accessors(self):
        '''Output the accessors.'''
        index = 0
        for member in self.sdata:
            member, nesting = Output.extract_member_type(member)
            rettype = nesting.format('{accessorstruct}<{index}>')
            for const in (False, True):
                if const:
                    fmtstring = 'const ' + rettype + '& {member}() const {lbrace}'
                else:
                    fmtstring = rettype + '& {member}() {lbrace}'
                self.writeln(fmtstring, index=index, member=member)

                self.indent()

                # Generate member accessing
                for splitindex in range(self.colcount):
                    if splitindex + 1 < self.colcount:
                        self.writelns('''\
if (auto val = std::get_if<{unifiedstruct}<{index}>>(&value)) {lbrace}
{indent}return val->{member}();
{rbrace}\
''', index=splitindex + 1, member=member)
                    else:
                        self.writeln('''\
return std::get<{unifiedstruct}<{index}>>(value).{member}();\
''', index=splitindex + 1, member=member)

                self.unindent()
                self.writeln('{rbrace}')
            index += 1
        self.writeln()

    def convert_split_variant(self, startindex, endindex):
        '''Output a single split variant of a struct.

        The split includes columns at [startindex, endindex).
        '''
        self.writeln('template <>')
        self.writeln(
                'struct {splitstruct}<{start}, {end}> {lbrace}',
                start=startindex, end=endindex)

        self.indent()

        self.writeln('explicit {splitstruct}() = default;')

        index = 0
        for member in self.sdata:
            if startindex <= index < endindex:
                member, nesting = Output.extract_member_type(member)
                rettype = nesting.format('{accessorstruct}<{index}>')
                self.writeln(
                        rettype + ' {member};', index=index, member=member)
            index += 1

        self.unindent()

        self.writeln('{rbrace};')

    def convert_unified_variant(self, splitindex):
        '''Output the unified variant of the given split.'''
        self.writeln('template <>')
        self.writeln(
                'struct {unifiedstruct}<{splitindex}> {lbrace}',
                splitindex=splitindex)

        self.indent()

        index = 0
        for member in self.sdata:
            member, nesting = Output.extract_member_type(member)
            rettype = nesting.format('{accessorstruct}<{index}>')
            for const in (False, True):
                if const:
                    fmtstring = 'const ' + rettype + '& {member}() const {lbrace}'
                else:
                    fmtstring = rettype + '& {member}() {lbrace}'
                self.writeln(fmtstring, index=index, member=member)

                self.indent()
                self.writeln(
                        'return split_{variant}.{member};',
                        variant=0 if index < splitindex else 1,
                        member=member)
                self.unindent()

                self.writeln('{rbrace}')
            index += 1

        self.writeln()

        self.writeln(
                '{splitstruct}<0, {splitindex}> split_0;',
                splitindex=splitindex)
        if splitindex < self.colcount:
            self.writeln(
                    '{splitstruct}<{splitindex}, {colcount}> split_1;',
                    splitindex=splitindex)

        self.unindent()

        self.writeln('{rbrace};')

if '__main__' == __name__:
    parser = argparse.ArgumentParser(description='STO struct generator')
    parser.add_argument(
            'file', type=str, help='INI-style struct definition file')
    parser.add_argument(
            '-o', '--out', default=None, type=str,
            help='Output file for generated structs, defaults to stdout')

    args = parser.parse_args()

    config = configparser.ConfigParser()
    config.optionxform = lambda option: option  # Verbatim options
    config.SECTCRE = re.compile(r'\[\s*(?P<header>[^]]+?)\s*\]')
    config.read(args.file)

    outfile = open(args.out, 'w') if args.out else sys.stdout

    for struct in config.sections():
        Output(outfile).convert_struct(struct, config[struct])

    outfile.close()
