#!/usr/bin/env python3

import argparse
import configparser
import re
import sys

class Output:
    def __init__(self, writer, namespaces):
        self._data = {}
        self._indent = 0
        self._namespaces = namespaces
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

    def convert_struct(self, struct, sdata, headers=False):
        '''Output a single struct's variants.'''
        self.sdata = dict(filter(lambda item: item[0][0] != '@', sdata.items()))
        self.mdata = dict(map(lambda item: (item[0][1:], item[1]),
            filter(lambda item: item[0][0] == '@', sdata.items())))

        self.colcount = Output.count_columns(self.sdata.keys())
        self._data = {
                'accessorstruct': '::sto::adapter::Accessor',
                'colcount': self.colcount,
                'infostruct': 'accessor_info',
                'lbrace': '{',
                'ns': '{}_datatypes'.format(struct),
                'rbrace': '}',
                'splitstruct': 'split_value',
                'unifiedstruct': 'unified_value',
                'qns': ''.join('::' + ns for ns in self._namespaces),  # Globally-qualified namespace
                'struct': struct,
                }

        if headers:
            self.writelns('''\
#pragma once

#include <type_traits>
''')

        for ns in self._namespaces:
            self.writeln('namespace {nsname} {lbrace}\n', nsname=ns)

        self.writeln('namespace {ns} {lbrace}\n')

        self.convert_namedcolumns()

        self.writelns('''\
struct {struct};
''')

        self.convert_column_accessors()

        self.writeln('struct {struct} {lbrace}')

        self.indent()

        self.writelns('''\
using NamedColumn = {ns}::NamedColumn;
static constexpr auto DEFAULT_SPLIT = NamedColumn::{defaultsplit};
static constexpr auto MAX_SPLITS = 2;

explicit {struct}() = default;

template <NamedColumn Column>
static inline {struct}& of({accessorstruct}<{infostruct}<Column>>& accessor) {lbrace}
{indent}return *reinterpret_cast<{struct}*>(
{indent}{indent}reinterpret_cast<uintptr_t>(&accessor) - {infostruct}<Column>::offset());
{rbrace}

static inline void resplit(
{indent}{indent}{struct}& newvalue, const {struct}& oldvalue, NamedColumn index);

template <NamedColumn Column>
static inline void copy_data({struct}& newvalue, const {struct}& oldvalue);

inline void init(const {struct}* oldvalue = nullptr) {lbrace}
{indent}if (oldvalue) {lbrace}
{indent}{indent}auto split = oldvalue->splitindex_;
{indent}{indent}if (::sto::AdapterConfig::IsEnabled(::sto::AdapterConfig::Global)) {lbrace}
{indent}{indent}{indent}split = ADAPTER_OF({struct})::CurrentSplit();
{indent}{indent}{rbrace}
{indent}{indent}if (::sto::AdapterConfig::IsEnabled(::sto::AdapterConfig::Inline)) {lbrace}
{indent}{indent}{indent}split = split;
{indent}{indent}{rbrace}
{indent}{indent}{struct}::resplit(*this, *oldvalue, split);
{indent}{rbrace}
{rbrace}

template <NamedColumn Column>
inline {accessorstruct}<{infostruct}<RoundedNamedColumn<Column>()>>& get();

template <NamedColumn Column>
inline const {accessorstruct}<{infostruct}<RoundedNamedColumn<Column>()>>& get() const;
''', defaultsplit=self.mdata.get('split', 'COLCOUNT'))

        self.convert_accessors()

        self.convert_members()

        self.unindent()

        self.writeln('{rbrace};')

        self.convert_resplitter()

        self.convert_offsets()

        self.convert_indexed_accessors()

        self.writelns('''
{rbrace};  // namespace {ns}

using {struct} = {ns}::{struct};
''')

        for ns in self._namespaces:
            self.writeln('{rbrace};  // namespace {nsname}\n', nsname=ns)

        #self.convert_version_selectors()

    def convert_namedcolumns(self):
        '''Output the NamedColumn for the given type.'''
        self.writeln('enum class NamedColumn : int {lbrace}')

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
            if count != 1:
                specify_value = True
            total_count += count
        if specify_value:
            self.writeln('COLCOUNT = {colcount}')
        else:
            self.writeln('COLCOUNT')
        self.unindent()

        self.writelns('''\
{rbrace};

inline constexpr NamedColumn operator+(NamedColumn nc, std::underlying_type_t<NamedColumn> index) {lbrace}
{indent}return NamedColumn(static_cast<std::underlying_type_t<NamedColumn>>(nc) + index);
{rbrace}

inline constexpr NamedColumn operator+(NamedColumn nc, NamedColumn off) {lbrace}
{indent}return nc + static_cast<std::underlying_type_t<NamedColumn>>(off);
{rbrace}

inline NamedColumn& operator+=(NamedColumn& nc, std::underlying_type_t<NamedColumn> index) {lbrace}
{indent}nc = static_cast<NamedColumn>(static_cast<std::underlying_type_t<NamedColumn>>(nc) + index);
{indent}return nc;
{rbrace}

inline NamedColumn& operator++(NamedColumn& nc) {lbrace}
{indent}return nc += 1;
{rbrace}

inline NamedColumn& operator++(NamedColumn& nc, int) {lbrace}
{indent}return nc += 1;
{rbrace}

inline constexpr NamedColumn operator-(NamedColumn nc, std::underlying_type_t<NamedColumn> index) {lbrace}
{indent}return NamedColumn(static_cast<std::underlying_type_t<NamedColumn>>(nc) - index);
{rbrace}

inline constexpr NamedColumn operator-(NamedColumn nc, NamedColumn off) {lbrace}
{indent}return nc - static_cast<std::underlying_type_t<NamedColumn>>(off);
{rbrace}

inline NamedColumn& operator-=(NamedColumn& nc, std::underlying_type_t<NamedColumn> index) {lbrace}
{indent}nc = static_cast<NamedColumn>(static_cast<std::underlying_type_t<NamedColumn>>(nc) - index);
{indent}return nc;
{rbrace}

inline constexpr NamedColumn operator/(NamedColumn nc, std::underlying_type_t<NamedColumn> denom) {lbrace}
{indent}return NamedColumn(static_cast<std::underlying_type_t<NamedColumn>>(nc) / denom);
{rbrace}

inline std::ostream& operator<<(std::ostream& out, NamedColumn& nc) {lbrace}
{indent}out << static_cast<std::underlying_type_t<NamedColumn>>(nc);
{indent}return out;
{rbrace}
''')

        self.writelns('''\
template <NamedColumn Column>
constexpr NamedColumn RoundedNamedColumn() {lbrace}
{indent}static_assert(Column < NamedColumn::COLCOUNT);\
''')

        self.indent()
        keys = tuple(self.sdata.keys())
        for index in range(len(keys) - 1):
            member = Output.extract_member_type(keys[index])[0]
            nextmember = Output.extract_member_type(keys[index + 1])[0]
            self.writelns('''\
if constexpr (Column < NamedColumn::{nextmember}) {lbrace}
{indent}return NamedColumn::{member};
{rbrace}\
''', member=member, nextmember=nextmember)
        self.writeln(
                'return NamedColumn::{member};',
                member=Output.extract_member_type(keys[-1])[0])

        self.unindent();

        self.writeln('{rbrace}\n')

    def convert_column_accessors(self):
        '''Output the accessor wrapper for each column.'''

        self.writelns('''\
template <NamedColumn Column>
struct {infostruct};
''')

        index = 0
        for member, ctype in self.sdata.items():
            member, nesting, count = Output.extract_member_type(member)

            isarray = 'true' if count > 1 else 'false'
            vtype = 'std::array<{}, {}>'.format(ctype, count) \
                    if count > 1 else ctype

            self.writelns('''\
template <>
struct {infostruct}<NamedColumn::{member}> {lbrace}
{indent}using NamedColumn = {ns}::NamedColumn;
{indent}using struct_type = {struct};
{indent}using type = {ctype};
{indent}using value_type = {vtype};
{indent}static constexpr NamedColumn Column = NamedColumn::{member};
{indent}static constexpr bool is_array = {isarray};
{indent}static inline size_t offset();
{rbrace};
''', ctype=ctype, isarray=isarray, member=member, vtype=vtype)

            index += count

        self.writelns('''\
template <NamedColumn ColumnValue>
struct {infostruct} : {infostruct}<RoundedNamedColumn<ColumnValue>()> {lbrace}
{indent}static constexpr NamedColumn Column = ColumnValue;
{rbrace};
''')

    def convert_accessors(self):
        '''Output the accessors.'''

        self.writelns('''
const auto split_of(NamedColumn index) const {lbrace}
{indent}return index < splitindex_ ? 0 : 1;
{rbrace}
''')

    def convert_members(self):
        '''Output the member variables.'''

        for member in self.sdata:
            member, _, count = Output.extract_member_type(member)
            self.writeln(
                    '{accessorstruct}<{infostruct}<NamedColumn::{member}>> {member};',
                    member=member)

        self.writeln('NamedColumn splitindex_ = DEFAULT_SPLIT;')

    def convert_resplitter(self):
        '''Output the resplitting functionality.'''

        self.writelns('''
inline void {struct}::resplit(
{indent}{indent}{struct}& newvalue, const {struct}& oldvalue, NamedColumn index) {lbrace}
{indent}assert(NamedColumn(0) < index && index <= NamedColumn::COLCOUNT);
{indent}if (&newvalue != &oldvalue) {lbrace}
{indent}{indent}memcpy(&newvalue, &oldvalue, sizeof newvalue);
{indent}{indent}//copy_data<NamedColumn(0)>(newvalue, oldvalue);
{indent}{rbrace}
{indent}newvalue.splitindex_ = index;
{rbrace}

template <NamedColumn Column>
inline void {struct}::copy_data({struct}& newvalue, const {struct}& oldvalue) {lbrace}
{indent}static_assert(Column < NamedColumn::COLCOUNT);
{indent}newvalue.template get<Column>().value_ = oldvalue.template get<Column>().value_;
{indent}if constexpr (Column + 1 < NamedColumn::COLCOUNT) {lbrace}
{indent}{indent}copy_data<Column + 1>(newvalue, oldvalue);
{indent}{rbrace}
{rbrace}\
''')

    def convert_offsets(self):
        '''Output the offset computation functions.'''

        fmtstring = '''
inline size_t {infostruct}<NamedColumn::{member}>::offset() {lbrace}
{indent}{struct}* ptr = nullptr;
{indent}return static_cast<size_t>(
{indent}{indent}reinterpret_cast<uintptr_t>(&ptr->{member}) - reinterpret_cast<uintptr_t>(ptr));
{rbrace}\
'''
        for member in self.sdata:
            member, _, count = Output.extract_member_type(member)
            self.writelns(fmtstring, member=member)

    def convert_indexed_accessors(self):
        '''Output the indexed accessor wrappers.'''

        fmtstring = '''
template <>
inline {const}{accessorstruct}<{infostruct}<NamedColumn::{member}>>& {struct}::get<NamedColumn::{member}{offindex}>() {const}{lbrace}
{indent}return {member};
{rbrace}\
'''

        index = 0
        for member in self.sdata:
            member, _, count = Output.extract_member_type(member)
            for realindex in range(index, index + count):
                for const in (False, True):
                    self.writelns(
                            fmtstring,
                            const='const ' if const else '',
                            index=index,
                            realindex=realindex,
                            offindex='' if count == 1 else ' + {}'.format(realindex - index),
                            member=member,
                            arrindex='' if count == 1 else realindex - index)
            index += count

if '__main__' == __name__:
    parser = argparse.ArgumentParser(description='STO struct generator')
    parser.add_argument(
            'file', type=str, help='INI-style struct definition file')
    parser.add_argument(
            '-n', '--namespace', default='::', type=str,
            help='''\
The fully-qualified namespace for the struct, e.g. ::benchmark::tpcc\
''')
    parser.add_argument(
            '-o', '--out', default=None, type=str,
            help='Output file for generated structs, defaults to stdout')

    args = parser.parse_args()

    config = configparser.ConfigParser()
    config.optionxform = lambda option: option  # Verbatim options
    config.SECTCRE = re.compile(r'\[\s*(?P<header>[^]]+?)\s*\]')
    config.read(args.file)

    outfile = open(args.out, 'w') if args.out else sys.stdout

    namespace = args.namespace
    if not namespace.startswith('::'):
        namespace = '::' + namespace
    namespaces = namespace[2:].split('::')
    if not namespaces[-1]:
        namespaces = namespaces[:-1]

    headers = True
    for struct in config.sections():
        Output(outfile, namespaces).convert_struct(struct, config[struct], headers=headers)
        headers = False

    outfile.close()
