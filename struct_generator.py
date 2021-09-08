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
                'raccessor': 'RecordAccessor'.format(struct),
                'vaccessor': '::sto::adapter::ValueAccessor',
                'statstype': '::sto::adapter::Stats<{}>'.format(struct),
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
#include "AdapterStructs.hh"
''')

        for ns in self._namespaces:
            self.writeln('namespace {nsname} {{\n', nsname=ns)

        self.writeln('namespace {ns} {{\n')

        self.writelns('''\
class {raccessor};
enum class NamedColumn;
''')

        self.convert_base_struct()

        self.convert_namedcolumns()

        self.convert_column_accessors()

        self.writelns('class {raccessor} {{')

        self.writeln('public:')
        self.indent()
        self.writelns('''\
using NamedColumn = {ns}::NamedColumn;
using StatsType = {statstype};
template <NamedColumn Column>
using ValueAccessor = {vaccessor}<{infostruct}<Column>>;
using ValueType = {struct};
static constexpr auto DEFAULT_SPLIT = NamedColumn::{defaultsplit};
static constexpr auto MAX_SPLITS = 2;
''', defaultsplit=self.mdata.get('split', 'COLCOUNT'))

        self.writelns('''\
{raccessor}() = default;
{raccessor}({struct}& value, StatsType* sptr=nullptr)
{indent}{indent}: {raccessor}(&value, sptr) {{}}
{raccessor}({struct}* vptr, StatsType* sptr=nullptr)\
''')
        self.writei('{indent}{indent}: ')
        for index, member in enumerate(self.sdata, start=0):
            member, *_ = Output.extract_member_type(member)
            self.write('{member}(this), ', member=member)
        self.write('vptr_(vptr), stats_(sptr) {{}}')
        self.writeln()
        self.writeln()

        self.convert_coercions()

        self.convert_getters()

        self.convert_accessors()

        self.convert_members()

        self.unindent()

        self.writeln('}};')

        #self.convert_resplitter()

        #self.convert_offsets()

        #self.convert_indexed_accessors()

        self.writelns('''
}};  // namespace {ns}

using {struct} = {ns}::{struct};
''')

        for ns in self._namespaces:
            self.writeln('}};  // namespace {nsname}\n', nsname=ns)

        #self.convert_version_selectors()

    def convert_base_struct(self):
        '''Output the base struct value type.'''
        self.writeln('struct {struct} {{')

        self.indent()
        self.writelns('''\
using {raccessor} = {ns}::{raccessor};
using NamedColumn = {ns}::NamedColumn;
''');
        for member, ctype in self.sdata.items():
            member, nesting, count = Output.extract_member_type(member)
            self.writeln(
                    '{type} {member};',
                    member=member, type=nesting.format(ctype))
        self.unindent()

        self.writeln('}};')
        self.writeln()

    def convert_namedcolumns(self):
        '''Output the NamedColumn for the given type.'''
        self.writeln('enum class NamedColumn : int {{')

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
}};

inline constexpr NamedColumn operator+(NamedColumn nc, std::underlying_type_t<NamedColumn> index) {{
{indent}return NamedColumn(static_cast<std::underlying_type_t<NamedColumn>>(nc) + index);
}}

inline constexpr NamedColumn operator+(NamedColumn nc, NamedColumn off) {{
{indent}return nc + static_cast<std::underlying_type_t<NamedColumn>>(off);
}}

inline NamedColumn& operator+=(NamedColumn& nc, std::underlying_type_t<NamedColumn> index) {{
{indent}nc = static_cast<NamedColumn>(static_cast<std::underlying_type_t<NamedColumn>>(nc) + index);
{indent}return nc;
}}

inline NamedColumn& operator++(NamedColumn& nc) {{
{indent}return nc += 1;
}}

inline NamedColumn& operator++(NamedColumn& nc, int) {{
{indent}return nc += 1;
}}

inline constexpr NamedColumn operator-(NamedColumn nc, std::underlying_type_t<NamedColumn> index) {{
{indent}return NamedColumn(static_cast<std::underlying_type_t<NamedColumn>>(nc) - index);
}}

inline constexpr NamedColumn operator-(NamedColumn nc, NamedColumn off) {{
{indent}return nc - static_cast<std::underlying_type_t<NamedColumn>>(off);
}}

inline NamedColumn& operator-=(NamedColumn& nc, std::underlying_type_t<NamedColumn> index) {{
{indent}nc = static_cast<NamedColumn>(static_cast<std::underlying_type_t<NamedColumn>>(nc) - index);
{indent}return nc;
}}

inline constexpr NamedColumn operator/(NamedColumn nc, std::underlying_type_t<NamedColumn> denom) {{
{indent}return NamedColumn(static_cast<std::underlying_type_t<NamedColumn>>(nc) / denom);
}}

inline std::ostream& operator<<(std::ostream& out, NamedColumn& nc) {{
{indent}out << static_cast<std::underlying_type_t<NamedColumn>>(nc);
{indent}return out;
}}
''')

        self.writelns('''\
template <NamedColumn Column>
constexpr NamedColumn RoundedNamedColumn() {{
{indent}static_assert(Column < NamedColumn::COLCOUNT);\
''')

        self.indent()
        keys = tuple(self.sdata.keys())
        for index in range(len(keys) - 1):
            member = Output.extract_member_type(keys[index])[0]
            nextmember = Output.extract_member_type(keys[index + 1])[0]
            self.writelns('''\
if constexpr (Column < NamedColumn::{nextmember}) {{
{indent}return NamedColumn::{member};
}}\
''', member=member, nextmember=nextmember)
        self.writeln(
                'return NamedColumn::{member};',
                member=Output.extract_member_type(keys[-1])[0])

        self.unindent();

        self.writeln('}}\n')

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
struct {infostruct}<NamedColumn::{member}> {{
{indent}using NamedColumn = {ns}::NamedColumn;
{indent}using struct_type = {raccessor};
{indent}using type = {ctype};
{indent}using value_type = {vtype};
{indent}static constexpr NamedColumn Column = NamedColumn::{member};
{indent}static constexpr bool is_array = {isarray};
}};
''', ctype=ctype, isarray=isarray, member=member, vtype=vtype)

            index += count

        self.writelns('''\
template <NamedColumn ColumnValue>
struct {infostruct} : {infostruct}<RoundedNamedColumn<ColumnValue>()> {{
{indent}static constexpr NamedColumn Column = ColumnValue;
}};
''')

    def convert_coercions(self):
        '''Output the type coercions.'''
        self.writelns('''\
inline operator bool() const {{
{indent}return vptr_ != nullptr;
}}
''')

    def convert_getters(self):
        '''Output the constexpr getters.'''
        keys = tuple(self.sdata.keys()) + ('COLCOUNT',)

#        for const in ('', 'const '):
#            self.writelns('''\
#template <NamedColumn Column>
#inline {const}{vaccessor}<{infostruct}<RoundedNamedColumn<Column>()>>& get() {const}{{\
#''', const=const)
#            self.indent()
#            for index in range(len(keys) - 1):
#                member = Output.extract_member_type(keys[index])[0]
#                nextmember = Output.extract_member_type(keys[index + 1])[0]
#                self.writelns('''\
#if constexpr (NamedColumn::{member} <= Column && Column < NamedColumn::{nextmember}) {{
#{indent}return {member};
#}}\
#''', member=member, nextmember=nextmember)
#            self.writeln('static_assert(Column < NamedColumn::COLCOUNT);')
#            self.unindent()
#            self.writeln('}}')
#            self.writeln()

        self.writelns('''\
template <NamedColumn Column>
inline typename {infostruct}<RoundedNamedColumn<Column>()>::value_type& get_raw() {{\
''')
        self.indent()
        for index in range(len(keys) - 1):
            member = Output.extract_member_type(keys[index])[0]
            nextmember = Output.extract_member_type(keys[index + 1])[0]
            self.writelns('''\
if constexpr (NamedColumn::{member} <= Column && Column < NamedColumn::{nextmember}) {{
{indent}return vptr_->{member};
}}\
''', member=member, nextmember=nextmember)
        self.writeln('static_assert(Column < NamedColumn::COLCOUNT);')
        self.unindent()
        self.writeln('}}')
        self.writeln()

        self.writelns('''\
template <NamedColumn Column>
static inline typename {infostruct}<RoundedNamedColumn<Column>()>::type& get_value(
{indent}{indent}{struct}* ptr) {{\
''')
        self.indent()
        for index in range(len(keys) - 1):
            member, _, count = Output.extract_member_type(keys[index])
            nextmember = Output.extract_member_type(keys[index + 1])[0]
            indexing = ''
            if count > 1:
                indexing = '''\
[static_cast<std::underlying_type_t<NamedColumn>>(Column - NamedColumn::{member})]\
'''.format(member=member)
            self.writelns('''\
if constexpr (NamedColumn::{member} <= Column && Column < NamedColumn::{nextmember}) {{
{indent}return ptr->{member}{indexing};
}}\
''', indexing=indexing, member=member, nextmember=nextmember)
        self.writeln('static_assert(Column < NamedColumn::COLCOUNT);')
        self.unindent()
        self.writeln('}}')

    def convert_accessors(self):
        '''Output the accessors.'''

        self.writelns('''
const auto split_of(NamedColumn index) const {{
{indent}return index < splitindex_ ? 0 : 1;
}}

void read(NamedColumn column) {{
{indent}if (stats_) {{
{indent}{indent}stats_->read_data.set(static_cast<std::underlying_type_t<NamedColumn>>(column));
{indent}}}
}}

void write(NamedColumn column) {{
{indent}if (!written_) {{
{indent}{indent}{struct}* old_val = vptr_;
{indent}{indent}vptr_ = Sto::tx_alloc<{struct}>();
{indent}{indent}if (old_val) {{
{indent}{indent}{indent}memmove(vptr_, old_val, sizeof *old_val);
{indent}{indent}}} else {{
{indent}{indent}{indent}new (vptr_) {struct}();
{indent}{indent}}}
{indent}}}
{indent}if (stats_) {{
{indent}{indent}stats_->write_data.set(static_cast<std::underlying_type_t<NamedColumn>>(column));
{indent}}}
}}
''')

    def convert_members(self):
        '''Output the member variables.'''

        for member in self.sdata:
            member, _, count = Output.extract_member_type(member)
            self.writeln(
                    'ValueAccessor<NamedColumn::{member}> {member};',
                    member=member)

        self.writeln('{struct}* vptr_ = nullptr;')
        self.writeln('NamedColumn splitindex_ = DEFAULT_SPLIT;')
        self.writeln('StatsType* stats_ = nullptr;')
        self.writeln('bool written_ = false;')

    def convert_resplitter(self):
        '''Output the resplitting functionality.'''

        self.writelns('''
inline void {struct}::resplit(
{indent}{indent}{struct}& newvalue, const {struct}& oldvalue, NamedColumn index) {{
{indent}assert(NamedColumn(0) < index && index <= NamedColumn::COLCOUNT);
{indent}if (&newvalue != &oldvalue) {{
{indent}{indent}memcpy(&newvalue, &oldvalue, sizeof newvalue);
{indent}{indent}//copy_data<NamedColumn(0)>(newvalue, oldvalue);
{indent}}}
{indent}newvalue.splitindex_ = index;
}}

template <NamedColumn Column>
inline void {struct}::copy_data({struct}& newvalue, const {struct}& oldvalue) {{
{indent}static_assert(Column < NamedColumn::COLCOUNT);
{indent}newvalue.template get<Column>().value_ = oldvalue.template get<Column>().value_;
{indent}if constexpr (Column + 1 < NamedColumn::COLCOUNT) {{
{indent}{indent}copy_data<Column + 1>(newvalue, oldvalue);
{indent}}}
}}\
''')

    def convert_offsets(self):
        '''Output the offset computation functions.'''

        fmtstring = '''
inline size_t {infostruct}<NamedColumn::{member}>::offset() {{
{indent}{struct}* ptr = nullptr;
{indent}return static_cast<size_t>(
{indent}{indent}reinterpret_cast<uintptr_t>(&ptr->{member}) - reinterpret_cast<uintptr_t>(ptr));
}}\
'''
        for member in self.sdata:
            member, _, count = Output.extract_member_type(member)
            self.writelns(fmtstring, member=member)

    def convert_indexed_accessors(self):
        '''Output the indexed accessor wrappers.'''

        fmtstring = '''
template <>
inline {const}{vaccessor}<{infostruct}<NamedColumn::{member}>>& {struct}::get<NamedColumn::{member}{offindex}>() {const}{{
{indent}return {member};
}}\
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
