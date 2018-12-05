#pragma once

namespace db_params {

// Benchmark parameters
constexpr const char *db_params_id_names[] = {"none", "default", "opaque", "2pl", "adaptive", "swiss", "tictoc"};

enum class db_params_id : int {
    None = 0, Default, Opaque, TwoPL, Adaptive, Swiss, TicToc
};

inline std::ostream &operator<<(std::ostream &os, const db_params_id &id) {
    os << db_params_id_names[static_cast<int>(id)];
    return os;
}

inline db_params_id parse_dbid(const char *id_string) {
    if (id_string == nullptr)
        return db_params_id::None;
    for (size_t i = 0; i < sizeof(db_params_id_names); ++i) {
        if (strcmp(id_string, db_params_id_names[i]) == 0) {
            auto selected = static_cast<db_params_id>(i);
            std::cout << "Selected \"" << selected << "\" as DB concurrency control." << std::endl;
            return selected;
        }
    }
    return db_params_id::None;
}

class db_default_params {
public:
    static constexpr db_params_id Id = db_params_id::Default;
    static constexpr bool RdMyWr = true;
    static constexpr bool TwoPhaseLock = false;
    static constexpr bool Adaptive = false;
    static constexpr bool Opaque = false;
    static constexpr bool Swiss = false;
    static constexpr bool TicToc = false;
};

class db_opaque_params : public db_default_params {
public:
    static constexpr db_params_id Id = db_params_id::Opaque;
    static constexpr bool Opaque = true;
};

class db_2pl_params : public db_default_params {
public:
    static constexpr db_params_id Id = db_params_id::TwoPL;
    static constexpr bool TwoPhaseLock = true;
};

class db_adaptive_params : public db_default_params {
public:
    static constexpr db_params_id Id = db_params_id::Adaptive;
    static constexpr bool Adaptive = true;
};

class db_swiss_params : public db_default_params {
public:
    static constexpr db_params_id Id = db_params_id::Swiss;
    static constexpr bool Swiss = true;
};

class db_tictoc_params : public db_default_params {
public:
    static constexpr db_params_id Id = db_params_id::TicToc;
    static constexpr bool TicToc = true;
};

class constants {
public:
    static constexpr double million = 1000000.0;
    static constexpr double billion = 1000.0 * million;
    static double processor_tsc_frequency; // in GHz
};

}; // namespace db_params
