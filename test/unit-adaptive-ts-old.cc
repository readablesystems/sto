#include <iostream>

#include "Sto.hh"
#include "TBox.hh"
#include "TMvBox.hh"

struct datetime_t {
    static constexpr int64_t MILLISECONDS_PER_SECOND = 1000;
    static constexpr int64_t SECONDS_PER_DAY = 86400;
    static constexpr int64_t DAYS_PER_YEAR = 365;

    void update() {
        seconds += milliseconds / MILLISECONDS_PER_SECOND;
        milliseconds %= MILLISECONDS_PER_SECOND;

        day += seconds / SECONDS_PER_DAY;
        seconds %= SECONDS_PER_DAY;

        year += day / DAYS_PER_YEAR;
        day %= DAYS_PER_YEAR;
    };

    int64_t year;  // Infrequently updated
    int64_t day;  // Infrequently updated
    int64_t seconds;  // Frequently updated
    int64_t milliseconds;  // Very frequently updated
    int64_t utcoffset;  // Infrequently updated
};

std::ostream& operator << (std::ostream& out, const datetime_t& dt) {
    out
        << "Year " << dt.year << ", Day " << dt.day
        << " " << dt.seconds << "s " << dt.milliseconds << "ms "
        << "at UTC" << dt.utcoffset;
    return out;
}

namespace commutators {

template <>
class Commutator<datetime_t> {
public:
    Commutator() = default;

    explicit Commutator(int64_t ms_inc) : ms_inc(ms_inc) {}

    void operate(datetime_t &value) const {
        value.milliseconds += ms_inc;
        value.update();
    }

private:
    int64_t ms_inc;
};

};  // namespace commutators

template <bool MVCC>
class TestDriver {
public:
    void RunTests();

private:
    using base_value_type = datetime_t;
    using box_type = typename std::conditional<
        MVCC,
        TMvBox<base_value_type>,
        TBox<base_value_type>>::type;

    void StatsTest();
};

template <bool MVCC>
void TestDriver<MVCC>::RunTests() {
    StatsTest();
}

template <bool MVCC>
void TestDriver<MVCC>::StatsTest() {
    box_type century21;
    century21.nontrans_write((datetime_t){2000, 0, 0, 0, -4});

    {
    }

    printf("Test pass: StatsTest\n");
}

int main() {
    {
        TestDriver<false>().RunTests();
    }
    printf("ALL TESTS PASS, ignore errors after this.\n");
    auto advancer = std::thread(&Transaction::epoch_advancer, nullptr);
    Transaction::rcu_release_all(advancer, 48);
    return 0;
}
