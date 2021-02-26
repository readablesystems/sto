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
                'qns': ''.join('::' + ns for ns in self._namespaces),  # Globally-qualified namespace
                'struct': struct,
                }
        self.sdata = sdata

        self.writelns('''\
#pragma once

#include <type_traits>

#include "Adapter.hh"
#include "Sto.hh"
''')

        for ns in self._namespaces:
            self.writeln('namespace {nsname} {lbrace}\n', nsname=ns)

        self.writeln('namespace {ns} {lbrace}\n')

        self.convert_namedcolumns()

        self.writelns('''\
template <NamedColumn Column>
struct {accessorstruct};

struct {struct};

DEFINE_ADAPTER({struct}, NamedColumn);
''')

        self.convert_column_accessors()

        self.writeln('struct {struct} {lbrace}')

        self.indent()

        self.writelns('''\
using NamedColumn = {ns}::NamedColumn;
static constexpr auto MAX_SPLITS = 2;

explicit {struct}() = default;

static inline void resplit(
{indent}{indent}{struct}& newvalue, const {struct}& oldvalue, NamedColumn index);

template <NamedColumn Column>
static inline void copy_data({struct}& newvalue, const {struct}& oldvalue);

template <NamedColumn Column>
inline accessor<RoundedNamedColumn<Column>()>& get();

template <NamedColumn Column>
inline const accessor<RoundedNamedColumn<Column>()>& get() const;
''')

        self.convert_accessors()

        self.convert_members()

        self.unindent()

        self.writeln('{rbrace};')

        self.convert_resplitter()

        self.convert_indexed_accessors()

        self.writelns('''
{rbrace};  // namespace {ns}

using {struct} = {ns}::{struct};
using ADAPTER_OF({struct}) = ADAPTER_OF({ns}::{struct});
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

constexpr NamedColumn operator+(NamedColumn nc, std::underlying_type_t<NamedColumn> index) {lbrace}
{indent}return NamedColumn(static_cast<std::underlying_type_t<NamedColumn>>(nc) + index);
{rbrace}

NamedColumn& operator+=(NamedColumn& nc, std::underlying_type_t<NamedColumn> index) {lbrace}
{indent}nc = static_cast<NamedColumn>(static_cast<std::underlying_type_t<NamedColumn>>(nc) + index);
{indent}return nc;
{rbrace}

NamedColumn& operator++(NamedColumn& nc) {lbrace}
{indent}return nc += 1;
{rbrace}

NamedColumn& operator++(NamedColumn& nc, int) {lbrace}
{indent}return nc += 1;
{rbrace}

constexpr NamedColumn operator-(NamedColumn nc, std::underlying_type_t<NamedColumn> index) {lbrace}
{indent}return NamedColumn(static_cast<std::underlying_type_t<NamedColumn>>(nc) - index);
{rbrace}

NamedColumn& operator-=(NamedColumn& nc, std::underlying_type_t<NamedColumn> index) {lbrace}
{indent}nc = static_cast<NamedColumn>(static_cast<std::underlying_type_t<NamedColumn>>(nc) - index);
{indent}return nc;
{rbrace}

std::ostream& operator<<(std::ostream& out, NamedColumn& nc) {lbrace}
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
{indent}using type = {ctype};
{indent}using value_type = {vtype};
{indent}static constexpr bool is_array = {isarray};
{rbrace};
''', ctype=ctype, isarray=isarray, member=member, vtype=vtype)

            index += count

        self.writelns('''\
template <NamedColumn Column>
struct {accessorstruct} {lbrace}\
''')

        self.indent()

        self.writelns('''\
using adapter_type = ADAPTER_OF({struct});
using type = typename {infostruct}<Column>::type;
using value_type = typename {infostruct}<Column>::value_type;

{accessorstruct}() = default;
template <typename... Args>
explicit {accessorstruct}(Args&&... args) : value_(std::forward<Args>(args)...) {lbrace}{rbrace}

operator const value_type() const {lbrace}
{indent}if constexpr ({infostruct}<Column>::is_array) {lbrace}
{indent}{indent}adapter_type::CountReads(Column, Column + value_.size());
{indent}{rbrace} else {lbrace}
{indent}{indent}adapter_type::CountRead(Column);
{indent}{rbrace}
{indent}return value_;
{rbrace}

value_type operator =(const value_type& other) {lbrace}
{indent}if constexpr ({infostruct}<Column>::is_array) {lbrace}
{indent}{indent}adapter_type::CountWrites(Column, Column + value_.size());
{indent}{rbrace} else {lbrace}
{indent}{indent}adapter_type::CountWrite(Column);
{indent}{rbrace}
{indent}return value_ = other;
{rbrace}

value_type operator =({accessorstruct}<Column>& other) {lbrace}
{indent}if constexpr ({infostruct}<Column>::is_array) {lbrace}
{indent}{indent}adapter_type::CountReads(Column, Column + other.value_.size());
{indent}{indent}adapter_type::CountWrites(Column, Column + value_.size());
{indent}{rbrace} else {lbrace}
{indent}{indent}adapter_type::CountRead(Column);
{indent}{indent}adapter_type::CountWrite(Column);
{indent}{rbrace}
{indent}return value_ = other.value_;
{rbrace}

template <NamedColumn OtherColumn>
value_type operator =(const {accessorstruct}<OtherColumn>& other) {lbrace}
{indent}if constexpr ({infostruct}<Column>::is_array && {infostruct}<OtherColumn>::is_array) {lbrace}
{indent}{indent}adapter_type::CountReads(OtherColumn, OtherColumn + other.value_.size());
{indent}{indent}adapter_type::CountWrites(Column, Column + value_.size());
{indent}{rbrace} else {lbrace}
{indent}{indent}adapter_type::CountRead(OtherColumn);
{indent}{indent}adapter_type::CountWrite(Column);
{indent}{rbrace}
{indent}return value_ = other.value_;
{rbrace}

template <bool is_array = {infostruct}<Column>::is_array>
std::enable_if_t<is_array, void>
operator ()(const std::underlying_type_t<NamedColumn>& index, const type& value) {lbrace}
{indent}adapter_type::CountWrite(Column + index);
{indent}value_[index] = value;
{rbrace}

template <bool is_array = {infostruct}<Column>::is_array>
std::enable_if_t<is_array, type&>
operator ()(const std::underlying_type_t<NamedColumn>& index) {lbrace}
{indent}adapter_type::CountWrite(Column + index);
{indent}return value_[index];
{rbrace}

template <bool is_array = {infostruct}<Column>::is_array>
std::enable_if_t<is_array, const type&>
operator [](const std::underlying_type_t<NamedColumn>& index) {lbrace}
{indent}adapter_type::CountRead(Column + index);
{indent}return value_[index];
{rbrace}

template <bool is_array = {infostruct}<Column>::is_array>
std::enable_if_t<!is_array, type&>
operator *() {lbrace}
{indent}adapter_type::CountWrite(Column);
{indent}return value_;
{rbrace}

template <bool is_array = {infostruct}<Column>::is_array>
std::enable_if_t<!is_array, type*>
operator ->() {lbrace}
{indent}adapter_type::CountWrite(Column);
{indent}return &value_;
{rbrace}

value_type value_;\
''')

        self.unindent()

        self.writeln('{rbrace};')
        self.writeln()

    def convert_accessors(self):
        '''Output the accessors.'''

        self.writelns('''
const auto split_of(NamedColumn index) const {lbrace}
{indent}return index < splitindex_ ? 0 : 1;
{rbrace}
''')

    def convert_members(self):
        '''Output the member variables.'''

        self.writeln('NamedColumn splitindex_ = NamedColumn::COLCOUNT;');

        for member in self.sdata:
            member, _, count = Output.extract_member_type(member)
            self.writeln(
                    'accessor<NamedColumn::{member}> {member};', member=member)

    def convert_resplitter(self):
        '''Output the resplitting functionality.'''

        self.writelns('''
inline void {struct}::resplit(
{indent}{indent}{struct}& newvalue, const {struct}& oldvalue, NamedColumn index) {lbrace}
{indent}assert(NamedColumn(0) < index && index <= NamedColumn::COLCOUNT);
{indent}copy_data<NamedColumn(0)>(newvalue, oldvalue);
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

    def convert_indexed_accessors(self):
        '''Output the indexed accessor wrappers.'''

        fmtstring = '''
template <>
inline {const}accessor<NamedColumn::{member}>& {struct}::get<NamedColumn::{member}{offindex}>() {const}{lbrace}
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

    for struct in config.sections():
        Output(outfile, namespaces).convert_struct(struct, config[struct])

    outfile.close()
