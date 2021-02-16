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

#include "Adapter.hh"
#include "Sto.hh"
#include "VersionSelector.hh"
''')

        for ns in self._namespaces:
            self.writeln('namespace {nsname} {lbrace}\n', nsname=ns)

        self.writeln('namespace {ns} {lbrace}\n')

        self.convert_namedcolumns()

        self.writelns('''\
template <NamedColumn Column>
struct {accessorstruct};

template <NamedColumn StartIndex, NamedColumn EndIndex>
struct {splitstruct};

template <NamedColumn SplitIndex>
struct {unifiedstruct};

struct {struct};

DEFINE_ADAPTER({struct}, NamedColumn);
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
using NamedColumn = {ns}::NamedColumn;
static constexpr auto MAX_SPLITS = 2;

explicit {struct}() = default;

static inline void resplit(
{indent}{indent}{struct}& newvalue, const {struct}& oldvalue, NamedColumn index);

template <NamedColumn Column>
static inline void set_unified({struct}& value, NamedColumn index);

template <NamedColumn Column>
static inline void copy_data({struct}& newvalue, const {struct}& oldvalue);
''')

        self.convert_accessors()

        self.writeln('std::variant<')

        self.indent()

        for splitindex in range(self.colcount, 0, -1):
            self.writei('{unifiedstruct}<NamedColumn({index})>', index=splitindex)
            if splitindex > 1:
                self.write(',')
            self.writeln()
        self.writeln('> value;')

        self.unindent(2)

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

            self.writelns('''\
template <>
struct {infostruct}<NamedColumn::{member}> {lbrace}
{indent}using type = {ctype};
{rbrace};
''', ctype=ctype, member=member)

            index += count

        self.writelns('''\
template <NamedColumn Column>
struct {accessorstruct} {lbrace}\
''')

        self.indent()

        self.writelns('''\
using type = typename {infostruct}<Column>::type;

{accessorstruct}() = default;
{accessorstruct}(type& value) : value_(value) {lbrace}{rbrace}
{accessorstruct}(const type& value) : value_(const_cast<type&>(value)) {lbrace}{rbrace}

operator type() {lbrace}
{indent}ADAPTER_OF({struct})::CountWrite(Column + index_);
{indent}return value_;
{rbrace}

operator const type() const {lbrace}
{indent}ADAPTER_OF({struct})::CountRead(Column + index_);
{indent}return value_;
{rbrace}

operator type&() {lbrace}
{indent}ADAPTER_OF({struct})::CountWrite(Column + index_);
{indent}return value_;
{rbrace}

operator const type&() const {lbrace}
{indent}ADAPTER_OF({struct})::CountRead(Column + index_);
{indent}return value_;
{rbrace}

type operator =(const type& other) {lbrace}
{indent}ADAPTER_OF({struct})::CountWrite(Column + index_);
{indent}return value_ = other;
{rbrace}

type operator =(const {accessorstruct}<Column>& other) {lbrace}
{indent}ADAPTER_OF({struct})::CountWrite(Column + index_);
{indent}return value_ = other.value_;
{rbrace}

type operator *() {lbrace}
{indent}ADAPTER_OF({struct})::CountWrite(Column + index_);
{indent}return value_;
{rbrace}

const type operator *() const {lbrace}
{indent}ADAPTER_OF({struct})::CountRead(Column + index_);
{indent}return value_;
{rbrace}

type* operator ->() {lbrace}
{indent}ADAPTER_OF({struct})::CountWrite(Column + index_);
{indent}return &value_;
{rbrace}

const type* operator ->() const {lbrace}
{indent}ADAPTER_OF({struct})::CountRead(Column + index_);
{indent}return &value_;
{rbrace}

std::underlying_type_t<NamedColumn> index_ = 0;
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
                'struct {splitstruct}<NamedColumn({start}), NamedColumn({end})> {lbrace}',
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
                            '{accessorstruct}<NamedColumn::{member}> {member};',
                            member=member)
                else:
                    size = min(index + count, endindex) - max(startindex, index)
                    self.writeln(
                            'std::array<{accessorstruct}<NamedColumn::{member}>, {count}> {member};',
                            count=size, member=member)
            index += count

        self.unindent()

        self.writeln('{rbrace};')

    def convert_unified_variant(self, splitindex):
        '''Output the unified variant of the given split.'''
        self.writeln('template <>')
        self.writeln(
                'struct {unifiedstruct}<NamedColumn({splitindex})> {lbrace}',
                splitindex=splitindex)

        self.indent()

        index = 0
        for member in self.sdata:
            member, nesting, count = Output.extract_member_type(member)
            rettype = '{accessorstruct}<{index}>'
            for const in (False, True):
                if count == 1:
                    fmtstring = '''\
{const}auto& {member}() {const}{lbrace}
{indent}return split_{variant}.{member};
{rbrace}\
'''
                elif ((index < splitindex) == (index + count <= splitindex)):
                    fmtstring = '''\
{const}auto& {member}(size_t index) {const}{lbrace}
{indent}return split_{variant}.{member}[index];
{rbrace}\
'''
                else:
                    fmtstring = '''\
{const}auto& {member}(size_t index) {const}{lbrace}
{indent}if (index < {splitindex}) {lbrace}
{indent}{indent}return split_0.{member}[index];
{indent}{rbrace}
{indent}return split_1.{member}[index - {indexdiff}];
{rbrace}\
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

        self.writelns('''
const auto split_of(NamedColumn index) const {lbrace}
{indent}return index < NamedColumn({splitindex}) ? 0 : 1;
{rbrace}
''', splitindex=splitindex)

        self.writeln(
                '{splitstruct}<NamedColumn(0), NamedColumn({splitindex})> split_0;',
                splitindex=splitindex)
        if splitindex < self.colcount:
            self.writeln(
                    '{splitstruct}<NamedColumn({splitindex}), NamedColumn({colcount})> split_1;',
                    splitindex=splitindex)

        self.unindent()

        self.writeln('{rbrace};')

    def convert_accessors(self):
        '''Output the accessors.'''

        self.writelns('''\
template <NamedColumn Column>
inline accessor<RoundedNamedColumn<Column>()>& get();

template <NamedColumn Column>
inline const accessor<RoundedNamedColumn<Column>()>& get() const;
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
        self.writelns('''
const auto split_of(NamedColumn index) const {lbrace}
{indent}return std::visit([this, index] (auto&& val) -> const auto {lbrace}
{indent}{indent}{indent}return val.split_of(index);
{indent}{indent}{rbrace}, value);
{rbrace}
''')

    def convert_resplitter(self):
        '''Output the resplitting functionality.'''

        self.writelns('''
inline void {struct}::resplit(
{indent}{indent}{struct}& newvalue, const {struct}& oldvalue, NamedColumn index) {lbrace}
{indent}assert(NamedColumn(0) < index && index <= NamedColumn::COLCOUNT);
{indent}set_unified<NamedColumn(1)>(newvalue, index);
{indent}if (newvalue.value.index() == oldvalue.value.index()) {lbrace}
{indent}{indent}newvalue.value = oldvalue.value;
{indent}{rbrace} else {lbrace}
{indent}{indent}copy_data<NamedColumn(0)>(newvalue, oldvalue);
{indent}{rbrace}
{rbrace}

template <NamedColumn Column>
inline void {struct}::set_unified({struct}& value, NamedColumn index) {lbrace}
{indent}static_assert(Column <= NamedColumn::COLCOUNT);
{indent}if (Column == index) {lbrace}
{indent}{indent}value.value = {unifiedstruct}<Column>();
{indent}{indent}return;
{indent}{rbrace}
{indent}if constexpr (Column < NamedColumn::COLCOUNT) {lbrace}
{indent}{indent}set_unified<Column + 1>(value, index);
{indent}{rbrace}
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
inline {const}accessor<RoundedNamedColumn<NamedColumn({index})>()>& {struct}::get<NamedColumn({realindex})>() {const}{lbrace}
{indent}return {member}({arrindex});
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
                            member=member,
                            arrindex='' if count == 1 else realindex - index)
            index += count

    def convert_version_selectors(self):
        '''Output the version selector implementations.'''
        self.writeln('''\
namespace ver_sel {lbrace}

template <typename VersImpl>
class VerSel<{qns}::{struct}, VersImpl> : public VerSelBase<VerSel<{qns}::{struct}, VersImpl>, VersImpl> {lbrace}
public:
    typedef VersImpl version_type;
    static constexpr size_t num_versions = 2;

    explicit VerSel(type v) : vers_() {lbrace}
        new (&vers_[0]) version_type(v);
    {rbrace}
    
    VerSel(type v, bool insert) : vers_() {lbrace}
        new (&vers_[0]) version_type(v, insert);
    {rbrace}

    constexpr static int map_impl(int col_n) {lbrace}
        typedef {qns}::{struct}::NamedColumn nc;
        auto col_name = static_cast<nc>(col_n);
        if (col_name == nc::odd_columns)
            return 0;
        else
            return 1;
    {rbrace}

    version_type& version_at_impl(int cell) {lbrace}
        return vers_[cell];
    {rbrace}

    void install_by_cell_impl(ycsb::ycsb_value *dst, const ycsb::ycsb_value *src, int cell) {lbrace}
        if (cell != 1 && cell != 0) {lbrace}
            always_assert(false, "Invalid cell id.");
        {rbrace}
        if (cell == 0) {lbrace}
            dst->odd_columns = src->odd_columns;
        {rbrace} else {lbrace}
            dst->even_columns = src->even_columns;
        {rbrace}
    {rbrace}

private:
    version_type vers_[num_versions];
{rbrace};
''')

        self.writeln('{rbrace};  // namespace ver_sel')

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
