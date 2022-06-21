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
        # Read in struct data (non-prefixed by @)
        self.sdata = dict(filter(lambda item: item[0][0] != '@', sdata.items()))

        # Read in metadata (prefixed by @)
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
                'ns': '{}_datatypes'.format(struct),
                'qns': ''.join('::' + ns for ns in self._namespaces),  # Globally-qualified namespace
                'struct': struct,
                }
        self._data['qns'] += '::' + self._data['ns']

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
{indent}inline static constexpr size_t cell_col_count(int cell) {{\
''', split=', '.join(str(cell) for cell in split), variant=variant)
            self.indent(2)
            cells = collections.defaultdict(tuple)
            for col, cell in enumerate(split, start=0):
                cells[cell] += (col,)
            for cell, columns in cells.items():
                self.writelns('''\
if (cell == {cell}) {{
{indent}return {count};
}}\
''', cell=cell, count=len(cells[cell]))
            self.writeln('return 0;')
            self.unindent(2)
            self.writelns('''\
{indent}}}
{indent}template <int Cell>
{indent}inline static constexpr void copy_cell({struct}* dest, {struct}* src) {{\
''')

            self.indent(2)
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

inline static constexpr size_t cell_col_count(int index, int cell) {{\
''')
        self.indent()
        for variant, _ in enumerate(self.mdata['splits'], start=0):
            self.writelns('''\
if (index == {variant}) {{
{indent}return SplitPolicy<{variant}>::cell_col_count(cell);
}}\
''', variant=variant)
        self.unindent()

        self.writelns('''\
{indent}return 0;
}}

void copy_into({struct}* vptr, int index=0) {{\
''')
        self.indent()
        for cell in range(max_splits + 1):
            self.writeln(
                    'copy_cell(index, {cell}, vptr, vptrs_[{cell}]);',
                    cell=cell)
        self.unindent()
        self.writeln('}}\n')

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

    ### Selector-based classes generation methods; hierarchical, not lexicographical

    def generate_selectors(self, struct, sdata):
        '''Output a single struct's Selector-based classes.'''
        self.sdata = dict(filter(lambda item: item[0][0] != '@', sdata.items()))
        self.mdata = dict(map(lambda item: (item[0][1:], item[1]),
            filter(lambda item: item[0][0] == '@', sdata.items())))

        self.colcount = Output.count_columns(self.sdata.keys())
        self.mdata['splits'] = ((0,) * self.colcount,) \
                + tuple(tuple(int(cell.strip()) for cell in split.split(','))
                        for split in self.mdata.get('splits', '').split('\n')
                        if split)
        # Use the first custom split policy as the STS policy
        self.mdata['sts'] = self.mdata['splits'][min(1, len(self.mdata['splits']) - 1)]
        self._data = {
                'splitparams': 'SplitParams',
                'baccessor': 'RecordAccessor',
                'uaccessor': 'UniRecordAccessor',
                'saccessor': 'SplitRecordAccessor',
                'value0': '{}_cell0'.format(struct),
                'value1': '{}_cell1'.format(struct),
                'ns': '{}_split'.format(struct),
                'dns': '{}_datatypes'.format(struct),
                'sns': '{}_split_structs'.format(struct),  # Namespace for structs
                'qns': ''.join('::' + ns for ns in self._namespaces),  # Globally-qualified namespace
                'struct': struct,
                }
        self._data['qns'] += '::' + self._data['ns']

        for ns in self._namespaces:
            self.writeln('namespace {nsname} {{\n', nsname=ns)

        self.writeln('namespace {sns} {{')

        self.generate_split_structs()

        self.writelns('''
}};  // namespace {sns}
''')

        for ns in self._namespaces:
            self.writeln('}};  // namespace {nsname}\n', nsname=ns)

        self.writeln('namespace bench {{')

        for sts in (False, True):
            self.generate_split_params(sts)
            self.generate_base_record_accessor(sts)
            self.generate_uni_record_accessor(sts)
            self.generate_split_record_accessor(sts)

        self.writelns('''\
}};  // namespace bench
''')

    def generate_split_structs(self):
        '''Generate the split-cell value structs.'''
        sts = self.mdata['sts']
        if not sts:
            return

        for cell in (0, 1):
            value = self._data['value{}'.format(cell)]
            self.writelns('''
struct {value} {{
{indent}using NamedColumn = typename {dns}::{struct}::NamedColumn;
''', value=value)

            self.indent()
            index = 0
            for member, ctype in self.sdata.items():
                member, nesting, count = Output.extract_member_type(member)
                array_policy = sts[index : index + count]
                for assigned_cell in array_policy:
                    if assigned_cell == cell:
                        self.writeln('{ctype} {member};', member=member, ctype=nesting.format(ctype))
                        break
                index += count
            self.unindent()

            self.writeln('}};')

    def generate_split_params(self, sts=False):
        '''Generate the SplitParams structs.'''
        import collections

        policy = self.mdata['sts'] if sts else self.mdata['splits'][0]
        cells = max(policy) + 1

        self.writelns('''\
template <>
struct {splitparams}<{dns}::{struct}, {sts}> {{''', sts=str(sts).lower())

        self.indent()
        if cells > 1:
            self.writelns('using split_type_list = std::tuple<{sns}::{value0}, {sns}::{value1}>;')
        else:
            self.writeln('using split_type_list = std::tuple<{dns}::{struct}>;')
        self.writelns('''\
using layout_type = typename SplitMvObjectBuilder<split_type_list>::type;
static constexpr size_t num_splits = std::tuple_size<split_type_list>::value;

static constexpr auto split_builder = std::make_tuple(''')
        self.indent()
        for cell in range(cells):
            if cells > 1:
                self.writelns('''\
[] (const {dns}::{struct}& in) -> {sns}::{value} {{
{indent}{sns}::{value} out;''', value=self._data['value{}'.format(cell)])
            else:
                self.writelns('''\
[] (const {dns}::{struct}& in) -> {dns}::{struct} {{
{indent}{dns}::{struct} out;''')

            self.indent()
            index = 0
            for member, ctype in self.sdata.items():
                member, _, count = Output.extract_member_type(member)
                if count > 1:
                    in_cell = 0
                    array_policy = policy[index : index + count]
                    for assigned_cell in array_policy:
                        if assigned_cell == cell:
                            in_cell += 1
                    assert in_cell <= count
                    if in_cell == count:
                        self.writeln('out.{member} = in.{member};', member=member, ctype=ctype)
                    else:
                        for col, assigned_cell in enumerate(array_policy):
                            if assigned_cell == cell:
                                self.writeln('out.{member}[{index}] = in.{member}[{index}];', member=member, ctype=ctype, index=col)
                else:
                    assert count == 1
                    if policy[index] == cell:
                        self.writeln('out.{member} = in.{member};', member=member, ctype=ctype)
                index += count
            self.writeln('return out;')
            self.unindent()

            if cell + 1 < cells:
                self.writeln('}},')
            else:
                self.writeln('}}')
        self.unindent()

        self.writelns('''\
);

static constexpr auto split_merger = std::make_tuple(''')
        self.indent()
        for cell in range(cells):
            if cells > 1:
                self.writeln(
                        '[] ({dns}::{struct}* out, const {sns}::{value}& in) -> void {{',
                        value=self._data['value{}'.format(cell)])
            else:
                self.writeln(
                        '[] ({dns}::{struct}* out, const {dns}::{struct}& in) -> void {{')

            self.indent()
            index = 0
            for member, ctype in self.sdata.items():
                member, _, count = Output.extract_member_type(member)
                if count > 1:
                    in_cell = 0
                    array_policy = policy[index : index + count]
                    for assigned_cell in array_policy:
                        if assigned_cell == cell:
                            in_cell += 1
                    assert in_cell <= count
                    if in_cell == count:
                        self.writeln('out->{member} = in.{member};', member=member, ctype=ctype)
                    else:
                        for col, assigned_cell in enumerate(array_policy):
                            if assigned_cell == cell:
                                self.writeln('out->{member}[{index}] = in.{member}[{index}];', member=member, ctype=ctype, index=col)
                else:
                    assert count == 1
                    if policy[index] == cell:
                        self.writeln('out->{member} = in.{member};', member=member, ctype=ctype)
                index += count
            self.unindent()

            if cell + 1 < cells:
                self.writeln('}},')
            else:
                self.writeln('}}')
        self.unindent()

        self.writelns('''\
);
''')
        if cells > 1:
            self.writeln('static constexpr auto map = [] (int col_n) -> int {{')
        else:
            self.writeln('static constexpr auto map = [] (int) -> int {{')
        self.indent()
        if cells > 1:
            index_cells = collections.defaultdict(list)
            for index, cell in enumerate(policy):
                if cell:
                    index_cells[cell].append(index)
            self.writeln('switch (col_n) {{')
            for cell, indices in index_cells.items():
                self.writelns('''\
{cases}
{indent}return {cell};''',
                    cases=''.join('case {index}: '.format(index=index) for index in indices),
                    cell=cell)
            self.writelns('''\
default:
{indent}break;
}}''')
        self.writeln('return 0;')
        self.unindent()
        self.writeln('}};')
        self.unindent()

        self.writeln('}};')
        self.writeln()

    def generate_base_record_accessor(self, sts=False):
        '''Generate the BaseRecordAccessor structs.'''
        policy = self.mdata['sts'] if sts else self.mdata['splits'][0]
        cells = max(policy) + 1

        self.writelns('''\
template <typename Accessor>
class {baccessor}<Accessor, {dns}::{struct}, {sts}> {{
public:''', sts=str(sts).lower())

        self.indent()
        for member, ctype in self.sdata.items():
            member, _, count = Output.extract_member_type(member)
            self.writelns('''\
const {ctype}& {member}({param}) const {{
{indent}return impl().{member}_impl({arg});
}}
''', ctype=ctype, member=member, param='size_t index' if count > 1 else '',
     arg='index' if count > 1 else '')
        self.unindent()

        self.writelns('''\
{indent}void copy_into({dns}::{struct}* dst) const {{
{indent}{indent}return impl().copy_into_impl(dst);
{indent}}}

private:
{indent}const Accessor& impl() const {{
{indent}{indent}return *static_cast<const Accessor*>(this);
{indent}}}''')
        self.unindent()

        self.writeln('}};')
        self.writeln()

    def generate_uni_record_accessor(self, sts=False):
        '''Generate the UniRecordAccessor structs.'''
        policy = self.mdata['sts'] if sts else self.mdata['splits'][0]
        cells = max(policy) + 1

        self.writelns('''\
template <>
class {uaccessor}<{dns}::{struct}, {sts}> : public {baccessor}<{uaccessor}<{dns}::{struct}, {sts}>, {dns}::{struct}, {sts}> {{
public:''', sts=str(sts).lower())

        self.indent()
        self.writelns('''\
{uaccessor}() = default;
{uaccessor}(const {dns}::{struct}* const vptr) : vptr_(vptr) {{}}

operator bool() const {{
{indent}return vptr_ != nullptr;
}}
''')
        self.unindent()

        self.writeln('private:')

        self.indent()
        for member, ctype in self.sdata.items():
            member, _, count = Output.extract_member_type(member)
            self.writelns('''\
const {ctype}& {member}_impl({param}) const {{
{indent}return vptr_->{member}{arg};
}}
''',
        ctype=ctype, member=member, param='size_t index' if count > 1 else '',
        arg='[index]' if count > 1 else '')

        self.writeln('void copy_into_impl({dns}::{struct}* dst) const {{')
        self.indent()
        #if cells > 1:
        for cell in range(cells):
            self.writeln('if (vptr_) {{', cell=cell)

            self.indent()
            index = 0
            for member, ctype in self.sdata.items():
                member, _, count = Output.extract_member_type(member)
                if count > 1:
                    in_cell = 0
                    array_policy = policy[index : index + count]
                    for assigned_cell in array_policy:
                        if assigned_cell == cell:
                            in_cell += 1
                    assert in_cell <= count
                    if in_cell == count:
                        self.writeln(
                                'dst->{member} = vptr_->{member};',
                                member=member, ctype=ctype, cell=cell)
                    else:
                        for col, assigned_cell in enumerate(array_policy):
                            if assigned_cell == cell:
                                self.writeln(
                                        'dst->{member}[{index}] = vptr_->{member}[{index}];',
                                        member=member, ctype=ctype, cell=cell, index=col)
                else:
                    assert count == 1
                    if policy[index] == cell:
                        self.writeln(
                                'dst->{member} = vptr_->{member};',
                                member=member, ctype=ctype, cell=cell)
                index += count
            self.unindent()
            self.writeln('}}')
#        else:
#            self.writelns('''\
#if (vptr_0_) {{
#{indent}memcpy(dst, vptr_0_, sizeof *dst);
#}}''')
        self.unindent()
        self.writeln('}}')
        self.writeln()

        self.writelns('''\
/*
void copy_into_impl({dns}::{struct}* dst) const {{
{indent}if (vptr_) {{
{indent}{indent}memcpy(dst, vptr_, sizeof *dst);
{indent}}}
}}
*/

const {dns}::{struct}* vptr_;
friend {baccessor}<{uaccessor}<{dns}::{struct}, {sts}>, {dns}::{struct}, {sts}>;''',
        sts=str(sts).lower())
        self.unindent()

        self.writeln('}};')
        self.writeln()

    def generate_split_record_accessor(self, sts=False):
        '''Generate the SplitRecordAccessor structs.'''
        import collections

        policy = self.mdata['sts'] if sts else self.mdata['splits'][0]
        cells = max(policy) + 1

        self.writelns('''\
template <>
class {saccessor}<{dns}::{struct}, {sts}> : public {baccessor}<{saccessor}<{dns}::{struct}, {sts}>, {dns}::{struct}, {sts}> {{
public:''', sts=str(sts).lower())

        self.indent()
        self.writelns('''\
static constexpr auto num_splits = {splitparams}<{dns}::{struct}, {sts}>::num_splits;

{saccessor}() = default;
{saccessor}(const std::array<void*, num_splits>& vptrs) : {args} {{}}

operator bool() const {{''',
        args=', '.join('vptr_{cell}_(reinterpret_cast<{type}*>(vptrs[{cell}]))'.format(
            cell=cell, type=(
                (('{sns}::{value' + str(cell) + '}').format(**self._data) \
                        if cells > 1 else '{dns}::{struct}'.format(**self._data)))) \
                        for cell in range(cells)),
        sts=str(sts).lower())

        self.indent()
        if cells > 1:
            self.writeln('return vptr_0_ != nullptr && vptr_1_ != nullptr;')
        else:
            self.writeln('return vptr_0_ != nullptr;')
        self.unindent()

        self.writeln('}}')
        self.unindent()

        self.writeln('private:')

        self.indent()
        index = 0
        for member, ctype in self.sdata.items():
            member, _, count = Output.extract_member_type(member)
            self.writeln('const {ctype}& {member}_impl({param}) const {{',
                    ctype=ctype, member=member, param='size_t index' if count > 1 else '',
                    arg='[index]' if count > 1 else '')
            if count > 1:
                in_cell = 0
                array_policy = policy[index : index + count]
                cell = array_policy[0]
                for assigned_cell in array_policy:
                    if assigned_cell == cell:
                        in_cell += 1
                assert in_cell <= count
                if in_cell == count:
                    self.writeln('return vptr_{cell}_->{member}[index];', cell=cell, member=member)
                else:
                    index_cells = collections.defaultdict(list)
                    for colind, cell in enumerate(policy):
                        if cell:
                            index_cells[cell].append(colind)
                    self.writeln('switch (index) {{')
                    for cell, indices in index_cells.items():
                        self.writelns('''\
{cases}
{indent}return vptr_{cell}_->{member}[index];''',
                            cases=''.join('case {index}: '.format(index=colind) for colind in indices),
                            cell=cell, member=member)
                    self.writelns('''\
default:
{indent}break;
}}''')
                    self.writeln('return vptr_0_->{member}[index];', member=member)
            else:
                assert count == 1
                self.writeln('return vptr_{cell}_->{member};', cell=policy[index], member=member)
            self.writeln('}}')
            self.writeln()
            index += count

        self.writeln('void copy_into_impl({dns}::{struct}* dst) const {{')
        self.indent()
        #if cells > 1:
        for cell in range(cells):
            self.writeln('if (vptr_{cell}_) {{', cell=cell)

            self.indent()
            index = 0
            for member, ctype in self.sdata.items():
                member, _, count = Output.extract_member_type(member)
                if count > 1:
                    in_cell = 0
                    array_policy = policy[index : index + count]
                    for assigned_cell in array_policy:
                        if assigned_cell == cell:
                            in_cell += 1
                    assert in_cell <= count
                    if in_cell == count:
                        self.writeln(
                                'dst->{member} = vptr_{cell}_->{member};',
                                member=member, ctype=ctype, cell=cell)
                    else:
                        for col, assigned_cell in enumerate(array_policy):
                            if assigned_cell == cell:
                                self.writeln(
                                        'dst->{member}[{index}] = vptr_{cell}_->{member}[{index}];',
                                        member=member, ctype=ctype, cell=cell, index=col)
                else:
                    assert count == 1
                    if policy[index] == cell:
                        self.writeln(
                                'dst->{member} = vptr_{cell}_->{member};',
                                member=member, ctype=ctype, cell=cell)
                index += count
            self.unindent()
            self.writeln('}}')
#        else:
#            self.writelns('''\
#if (vptr_0_) {{
#{indent}memcpy(dst, vptr_0_, sizeof *dst);
#}}''')
        self.unindent()
        self.writeln('}}')
        self.writeln()

        if cells > 1:
            for cell in range(cells):
                self.writeln(
                        'const {sns}::{value}* vptr_{cell}_;',
                        value=self._data['value{}'.format(cell)], cell=cell)
        else:
            self.writeln('const {dns}::{struct}* vptr_0_;')
        self.writeln(
                'friend {baccessor}<{saccessor}<{dns}::{struct}, {sts}>, {dns}::{struct}, {sts}>;',
                sts=str(sts).lower())
        self.unindent()

        self.writeln('}};')
        self.writeln()


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
    parser.add_argument(
            '-s', '--selectors', action='store_true', default=False,
            help='Whether to also output the Selectors-style accessors \
(this is typically true for new datatypes without SplitParams wrappers.)')

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

    if args.selectors:
        for struct in config.sections():
            Output(outfile, namespaces).generate_selectors(struct, config[struct])

    outfile.close()
