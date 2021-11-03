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
        self.mdata['splits'] = ((0,) * self.colcount,) \
                + tuple(tuple(int(cell.strip()) for cell in split.split(','))
                        for split in self.mdata.get('splits', '').split('\n')
                        if split)
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
//#include "AdapterStructs.hh"
''')

        for ns in self._namespaces:
            self.writeln('namespace {nsname} {{\n', nsname=ns)

        self.writeln('namespace {ns} {{\n')

        self.writelns('''\
enum class NamedColumn;

using SplitType = int;

class {raccessor};
''')

        self.convert_base_struct()

        self.convert_namedcolumns()

        self.convert_column_accessors()

        self.convert_struct_accessors()

        self.convert_splits()

        self.writelns('''\
class {raccessor} {{\
''')

        self.writeln('public:')
        self.indent()
        self.writelns('''\
using NamedColumn = {ns}::NamedColumn;
//using SplitTable = {ns}::SplitTable;
using SplitType = {ns}::SplitType;
//using StatsType = {statstype};
//template <NamedColumn Column>
//using ValueAccessor = {vaccessor}<{infostruct}<Column>>;
using ValueType = {struct};
static constexpr auto DEFAULT_SPLIT = {defaultsplit};
static constexpr auto MAX_SPLITS = 2;
static constexpr auto MAX_POINTERS = MAX_SPLITS;
static constexpr auto POLICIES = {variants};
''', defaultsplit=self.mdata.get('split', '0'),
     variants=len(self.mdata['splits']))

        self.writelns('''\
{raccessor}() = default;
template <typename... T>
{raccessor}(T ...vals) : vptrs_({{ pointer_of(vals)... }}) {{}}
''')

        self.convert_coercions()

        self.convert_accessors()

        self.convert_getters()

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

    def convert_struct_accessors(self):
        '''Output the struct accessors.'''
        keys = tuple(self.sdata.keys()) + ('COLCOUNT',)

        self.writeln('struct StructAccessor {{')

        self.indent()

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
        self.unindent()
        self.writeln('}};\n')

    def convert_splits(self):
        '''Output the valid split options.'''
        import collections
        self.writelns('''\
template <size_t Variant>
struct SplitPolicy;
''')
        index = 0
        members = ()
        for member, ctype in self.sdata.items():
            member, _, count = Output.extract_member_type(member)
            assert count
            if count == 1:
                members += (member,)
            else:
                for k in range(count):
                    members += ('{}[{}]'.format(member, k),)
            index += count

        for variant, split in enumerate(self.mdata['splits'], start=0):
            self.writelns('''\
template <>
struct SplitPolicy<{variant}> {{
{indent}static constexpr auto ColCount = \
static_cast<std::underlying_type_t<NamedColumn>>(NamedColumn::COLCOUNT);
{indent}static constexpr int policy[ColCount] = {{ {split} }};
{indent}inline static constexpr int column_to_cell(NamedColumn column) {{
{indent}{indent}return policy[static_cast<std::underlying_type_t<NamedColumn> >(column)];
{indent}}}
{indent}template <int Cell>
{indent}inline static constexpr void copy_cell({struct}* dest, {struct}* src) {{\
''', split=', '.join(str(cell) for cell in split), variant=variant)

            self.indent(2)
            cells = collections.defaultdict(tuple)
            for col, cell in enumerate(split, start=0):
                cells[cell] += (col,)
            for cell, columns in cells.items():
                self.writeln('if constexpr(Cell == {cell}) {{', cell=cell)
                self.indent()
                for column in columns:
                    self.writeln(
                            'dest->{column} = src->{column};',
                            column=members[column])
                self.unindent()
                self.writeln('}}')
            self.unindent(2)

            self.writeln('{indent}}}')
            self.writeln('}};\n')

    def convert_coercions(self):
        '''Output the type coercions.'''
        self.writelns('''\
inline operator bool() const {{
{indent}return vptrs_[0] != nullptr;
}}

/*
inline ValueType* pointer_of(ValueType& value) {{
{indent}return &value;
}}
*/

inline ValueType* pointer_of(ValueType* vptr) {{
{indent}return vptr;
}}
''')

    def convert_accessors(self):
        '''Output the accessors.'''

        self.writelns('''\
template <int Index>
inline static constexpr const auto split_of(NamedColumn column) {{\
''')

        self.indent()
        for variant, _ in enumerate(self.mdata['splits'], start=0):
            self.writelns('''\
if constexpr (Index == {variant}) {{
{indent}return SplitPolicy<{variant}>::column_to_cell(column);
}}\
''', variant=variant)
        self.unindent()

        self.writelns('''\
{indent}return 0;
}}

inline static constexpr const auto split_of(int index, NamedColumn column) {{\
''')

        self.indent()
        for variant, _ in enumerate(self.mdata['splits'], start=0):
            self.writelns('''\
if (index == {variant}) {{
{indent}return SplitPolicy<{variant}>::column_to_cell(column);
}}\
''', variant=variant)
        self.unindent()

        self.writelns('''\
{indent}return 0;
}}

const auto cell_of(NamedColumn column) const {{
{indent}return split_of(splitindex_, column);
}}

template <int Index>
inline static constexpr void copy_cell(int cell, ValueType* dest, ValueType* src) {{
{indent}if constexpr (Index >= 0 && Index < POLICIES) {{\
''')

        self.indent(2)
        max_splits = 0
        for _, splits in enumerate(self.mdata['splits'], start=0):
            max_splits = max(max_splits, *splits)
        for cell in range(max_splits + 1):
            self.writelns('''\
if (cell == {cell}) {{
{indent}SplitPolicy<Index>::template copy_cell<{cell}>(dest, src);
{indent}return;
}}\
''', cell=cell)
        self.unindent(2)

        '''
{indent}if constexpr (Index >= 0 && Index < POLICIES) {{
{indent}{indent}if (Cell == cell) {{
{indent}{indent}{indent}SplitPolicy<Index>::template copy_cell<Cell>(dest, src);
{indent}{indent}{indent}return;
{indent}{indent}}}
{indent}{indent}if constexpr (Cell + 1 < MAX_SPLITS) {{
{indent}{indent}{indent}copy_cell<Index, Cell + 1>(cell, dest, src);
{indent}{indent}}}
{indent}}} else {{
{indent}{indent}(void) dest;
{indent}{indent}(void) src;
{indent}}}
'''
        self.writelns('''\
{indent}}} else {{
{indent}{indent}(void) dest;
{indent}{indent}(void) src;
{indent}}}
}}

inline static constexpr void copy_cell(int index, int cell, ValueType* dest, ValueType* src) {{\
''')

        self.indent()
        for variant, _ in enumerate(self.mdata['splits'], start=0):
            self.writelns('''\
if (index == {variant}) {{
{indent}copy_cell<{variant}>(cell, dest, src);
{indent}return;
}}\
''', variant=variant)
        self.unindent()

        '''
{indent}if constexpr (Index >= 0 && Index < POLICIES) {{
{indent}{indent}if (Index == index) {{
{indent}{indent}{indent}copy_cell<Index>(cell, dest, src);
{indent}{indent}{indent}return;
{indent}{indent}}}
{indent}{indent}if constexpr (Index + 1 < POLICIES) {{
{indent}{indent}{indent}copy_cell<Index + 1>(index, cell, dest, src);
{indent}{indent}}}
{indent}}} else {{
{indent}{indent}(void) index;
{indent}{indent}(void) cell;
{indent}{indent}(void) dest;
{indent}{indent}(void) src;
{indent}}}
'''
        self.writelns('''\
}}

void copy_into({struct}* vptr) {{
{indent}memcpy(vptr, vptrs_[0], sizeof *vptr);
}}
''')

    def convert_getters(self):
        '''Output the constexpr getters.'''
        keys = tuple(self.sdata.keys()) + ('COLCOUNT',)

        for member, ctype in self.sdata.items():
            member, _, count = Output.extract_member_type(member)
            for const in ('', 'const '):
                self.writelns('''\
inline {const}typename {infostruct}<NamedColumn::{member}>::value_type& {member}() {const}{{
{indent}return vptrs_[cell_of(NamedColumn::{member})]->{member};
}}
''', const=const, member=member)
                if count > 1:  # array
                    self.writelns('''\
inline {const}typename {infostruct}<NamedColumn::{member}>::type& {member}(
{indent}{indent}const std::underlying_type_t<NamedColumn> index) {const}{{
{indent}return vptrs_[cell_of(NamedColumn::{member})]->{member}[index];
}}
''', const=const, member=member)

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
{indent}return vptrs_[cell_of(NamedColumn::{member})]->{member};
}}\
''', member=member, nextmember=nextmember)
        self.writeln('static_assert(Column < NamedColumn::COLCOUNT);')
        self.unindent()
        self.writeln('}}\n')

        self.writelns('''
template <NamedColumn Column, typename... Args>
static inline typename {infostruct}<RoundedNamedColumn<Column>()>::type& get_value(
{indent}{indent}Args&&... args) {{
{indent}return StructAccessor::template get_value<Column>(std::forward<Args>(args)...);
}}
''')
    def convert_members(self):
        '''Output the member variables.'''

        #for member in self.sdata:
        #    member, _, count = Output.extract_member_type(member)
        #    self.writeln(
        #            'ValueAccessor<NamedColumn::{member}> {member};',
        #            member=member)

        self.writeln('std::array<{struct}*, MAX_POINTERS> vptrs_ = {{ nullptr, }};')
        self.writeln('SplitType splitindex_ = {{}};')
        #self.writeln('StatsType* stats_ = nullptr;')

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
