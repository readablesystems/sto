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
    def count_columns(members):
        '''Count the number of columns, taking arrays into account.'''
        total_count = 0
        for member in members:
            _, __, count = Output.extract_member_type(member)
            total_count += count
        return total_count

    @staticmethod
    def extract_member_type(member):
        '''Pull the base member name and the array "nesting" format.'''
        original = member
        nesting = '{}'
        total_count = 1
        while member.rfind(']') >= 0:
            brackets = (member.rfind('['), member.rfind(']'))
            assert brackets[0] < brackets[1]
            count = member[brackets[0] + 1 : brackets[1]]
            if not count.isdigit():
                raise ValueError(
                        'Array-style declarations must use integers: {}'.format(original))
            nesting = 'std::array<{ctype}, {count}>'.format(
                    ctype=nesting, count=count)
            total_count *= int(count)
            member = member[:brackets[0]]
        return member, nesting, total_count

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
        self.colcount = Output.count_columns(sdata.keys())
        self._data = {
                'accessorstruct': 'accessor',
                'colcount': self.colcount,
                'infostruct': 'accessor_info',
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

        self.writeln('{rbrace};')

        #self.convert_indexed_accessors()

        self.writelns('''\
{rbrace};  // namespace {ns}

using {struct} = {ns}::{struct};
using ADAPTER_OF({struct}) = ADAPTER_OF({ns}::{struct});
''')

    def convert_namedcolumns(self):
        '''Output the NamedColumn for the given type.'''
        self.writelns('enum class NamedColumn : int {lbrace}')

        self.indent()
        specify_value = True
        total_count = 0
        for member in self.sdata:
            member, _, count = Output.extract_member_type(member)
            if specify_value:
                value = total_count
                self.writeln('{member} = {value},', member=member, value=value)
                specify_value = False
            else:
                self.writeln('{member},', member=member)
            if count == 1:
                specify_value = True
            total_count += count
        if specify_value:
            self.writeln('COLCOUNT = {colcount}')
        else:
            self.writeln('COLCOUNT')
        self.unindent()

        self.writelns('{rbrace};\n')

    def convert_column_accessors(self):
        '''Output the accessor wrapper for each column.'''

        self.writelns('''\
template <size_t ColIndex>
struct {infostruct};
''')

        index = 0
        for member, ctype in self.sdata.items():
            member, nesting, count = Output.extract_member_type(member)

            self.writelns('''\
template <>
struct {infostruct}<{index}> {lbrace}
{indent}using type = {ctype};
{rbrace};
''', index=index, ctype=ctype)

            index += count

        self.writelns('''\
template <size_t ColIndex>
struct {accessorstruct} {lbrace}\
''')

        self.indent()

        self.writelns('''\
using type = typename {infostruct}<ColIndex>::type;

{accessorstruct}() = default;
{accessorstruct}(type& value) : value_(value) {lbrace}{rbrace}
{accessorstruct}(const type& value) : value_(const_cast<type&>(value)) {lbrace}{rbrace}

operator type() {lbrace}
{indent}ADAPTER_OF({struct})::CountRead(ColIndex + index_);
{indent}return value_;
{rbrace}

operator const type() const {lbrace}
{indent}ADAPTER_OF({struct})::CountRead(ColIndex + index_);
{indent}return value_;
{rbrace}

operator type&() {lbrace}
{indent}ADAPTER_OF({struct})::CountRead(ColIndex + index_);
{indent}return value_;
{rbrace}

operator const type&() const {lbrace}
{indent}ADAPTER_OF({struct})::CountRead(ColIndex + index_);
{indent}return value_;
{rbrace}

type operator =(const type& other) {lbrace}
{indent}ADAPTER_OF({struct})::CountWrite(ColIndex + index_);
{indent}return value_ = other;
{rbrace}

type operator =(const {accessorstruct}<ColIndex>& other) {lbrace}
{indent}ADAPTER_OF({struct})::CountWrite(ColIndex + index_);
{indent}return value_ = other.value_;
{rbrace}

type operator *() {lbrace}
{indent}ADAPTER_OF({struct})::CountRead(ColIndex + index_);
{indent}return value_;
{rbrace}

const type operator *() const {lbrace}
{indent}ADAPTER_OF({struct})::CountRead(ColIndex + index_);
{indent}return value_;
{rbrace}

type* operator ->() {lbrace}
{indent}ADAPTER_OF({struct})::CountRead(ColIndex + index_);
{indent}return &value_;
{rbrace}

const type* operator ->() const {lbrace}
{indent}ADAPTER_OF({struct})::CountRead(ColIndex + index_);
{indent}return &value_;
{rbrace}

size_t index_ = 0;
type value_;\
''')

        self.unindent()

        self.writeln('{rbrace};')
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

        has_array = False
        index = 0
        for member in self.sdata:
            member, nesting, count = Output.extract_member_type(member)
            if startindex < index + count != index < endindex:
                if count > 1:
                    has_array = True
                    break
            index += count

        if has_array:
            self.writeln('explicit {splitstruct}() {lbrace}')

            self.indent()
            index = 0
            for member in self.sdata:
                member, nesting, count = Output.extract_member_type(member)
                if startindex < index + count != index < endindex:
                    if count > 1:
                        for arrindex in range(index, index + count):
                            if startindex <= arrindex < endindex:
                                self.writeln('{member}[{arrindex}].index_ = {index};',
                                        member=member,
                                        arrindex=arrindex - max(index, startindex),
                                        index=arrindex - index)
                index += count
            self.unindent()

            self.writeln('{rbrace}')
        else:
            self.writeln('explicit {splitstruct}() = default;')

        index = 0
        for member in self.sdata:
            member, nesting, count = Output.extract_member_type(member)
            if startindex < index + count != index < endindex:
                if count == 1:
                    self.writeln(
                            '{accessorstruct}<{index}> {member};',
                            index=index, member=member)
                else:
                    size = min(index + count, endindex) - max(startindex, index)
                    self.writeln(
                            'std::array<{accessorstruct}<{index}>, {count}> {member};',
                            count=size, index=index, member=member)
            index += count

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
            member, nesting, count = Output.extract_member_type(member)
            rettype = '{accessorstruct}<{index}>'
            for const in (False, True):
                if count == 1:
                    fmtstring = '''\
{const}auto& {member}(size_t index) {const}{lbrace}
{indent}return split_{variant}.{member};
{rbrace}
'''
                elif ((index < splitindex) == (index + count <= splitindex)):
                    fmtstring = '''\
{const}auto& {member}(size_t index) {const}{lbrace}
{indent}return split_{variant}.{member}[index];
{rbrace}
'''
                else:
                    fmtstring = '''\
{const}auto& {member}(size_t index) {const}{lbrace}
{indent}if (index < {splitindex}) {lbrace}
{indent}{indent}return split_0.{member}[index];
{indent}{rbrace}
{indent}return split_1.{member}[index - {indexdiff}];
{rbrace}
'''
                self.writelns(
                        fmtstring,
                        const='const ' if const else '',
                        index=index,
                        splitindex=splitindex,
                        member=member,
                        variant=0 if index < splitindex else 1,
                        indexdiff=splitindex - index,
                        )
            index += count

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

    def convert_accessors(self):
        '''Output the accessors.'''

        if False:
            self.writelns('''\
inline auto& get(size_t index);
inline const auto& get(size_t index) const;
''')

        index = 0
        for member in self.sdata:
            member, nesting, count = Output.extract_member_type(member)
            for const in (False, True):
                if count == 1:
                    fmtstring = '''\
{const}auto& {member}() {const}{lbrace}
{indent}return std::visit([this] (auto&& val) -> {const}auto& {lbrace}
{indent}{indent}{indent}return val.{member}();
{indent}{indent}{rbrace}, value);
{rbrace}\
'''
                else:
                    fmtstring = '''\
{const}auto& {member}(size_t index) {const}{lbrace}
{indent}return std::visit([this, index] (auto&& val) -> {const}auto& {lbrace}
{indent}{indent}{indent}return val.{member}(index);
{indent}{indent}{rbrace}, value);
{rbrace}\
'''
                self.writelns(
                        fmtstring,
                        const='const ' if const else '',
                        index=index,
                        member=member)
            index += count
        self.writeln()

    def convert_indexed_accessors(self):
        '''Output the indexed accessor wrappers.'''

        for const in (False, True):
            fmtstring = '''\
inline {const}auto& {struct}::get(size_t index) {const}{lbrace}\
'''
            self.writeln(
                    fmtstring,
                    const='const ' if const else '')

            self.indent()
            self.writeln('switch (index) {lbrace}')

            self.indent()
            index = 0
            for member in self.sdata:
                member, _, count = Output.extract_member_type(member)
                for realindex in range(index, index + count):
                    self.writei('case {index}: ', index=realindex)
                    if count == 1:
                        self.write('return {member}();', member=member)
                    else:
                        self.write(
                                'return {member}({arrindex});',
                                arrindex=realindex - index,
                                member=member)
                    self.writeln()
                index += count
            self.unindent()

            self.writeln('{rbrace}')
            self.unindent()

            self.writeln('{rbrace}')

        self.writeln()

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
