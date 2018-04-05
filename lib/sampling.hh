#pragma once

//#include <iostream>

#include <cstdlib>
#include <cassert>
#include <cstring>
#include <cmath>
#include <vector>
#include <random>
#include <algorithm>

namespace sampling {

typedef size_t index_t;
typedef std::vector<double> weight_type;
typedef std::vector<index_t> trace_type;

#if 0
// commented out since we use std::discrete_distribution now to do the sampling
// prefix-sum tree data structure used to sample a given (scaled) pmf
class PrefixSumTree {
public:
    PrefixSumTree(size_t num_intervals) {
        size_t np2 = 1;
        while (np2 < num_intervals) np2 <<= 1;
        leaves_idx = np2 - 1;
        size = np2 * 2;
        array = new size_t[size];
        bzero(array, size*sizeof(size_t));
    }

    ~PrefixSumTree() {
        delete[] array;
    }

    // populate the content of the tree given a pmf
    void load(const pmf_type& pmf) {
        for (size_t i = 0; i < pmf.size(); ++i)
            increment_interval((index_t)i, pmf[i]);
    }

    // this is the "sample" method
    index_t find_interval(size_t prefix_sum) const {
        assert(prefix_sum <= array[0]);
        index_t p = 0;
        while (p < leaves_idx) {
            index_t left = p*2+1;
            index_t right = p*2+2;
            size_t al = array[left];
            if (al >= prefix_sum) {
                p = left;
            } else {
                prefix_sum -= al;
                p = right;
                assert(prefix_sum <= array[right]);
            }
        }
        return p - leaves_idx; 
    }

    size_t adjusted_resolution() const {
        return array[0];
    }

private:
    // helper function which incrementally updates the content of the tree
    // while maintaining data structure invariants
    void increment_interval(index_t index, size_t amount) {
        index += leaves_idx;
        while (true) {
            array[index] += amount;
            if (index == 0)
                break;
            index = (index - 1)/2;
        } 
    }

    size_t *array;
    size_t size;
    size_t leaves_idx;
};
#endif

template <typename IntType>
class StoUniformIntSampler {
public:
    typedef std::mt19937 rng_type;

    explicit StoUniformIntSampler(rng_type& rng) : gen(rng), dis() {}
    StoUniformIntSampler(rng_type& rng, IntType range)
        : gen(rng), dis(0, range) {}

    IntType sample() {
        return dis(gen);
    }

    IntType sample(rng_type& rng) {
        return dis(rng);
    }

    void set_params(typename std::uniform_int_distribution<IntType>::param_type p) {
        dis.param(p);
    }

    rng_type& generator() {
        return gen;
    }

private:
    rng_type& gen;
    std::uniform_int_distribution<IntType> dis;
};

// Parameters
// n: size of the universe
// shuffle: generate a random mapping from sampled indices to actual indices used in experiments
// index_table: set a custom index translation table
template <typename IntType = index_t>
class StoRandomDistribution {
public:
    typedef typename StoUniformIntSampler<IntType>::rng_type rng_type;

    StoRandomDistribution(rng_type& rng, IntType a, IntType b, bool shuffle = false)
        : begin(a), end(b), index_transform(false), uis(rng), dist() {
        assert(a < b);
        if (shuffle) {
            index_transform = true;
            for (auto i = begin; i <= end; ++i)
                index_translation_table.push_back(i);
            std::shuffle(index_translation_table.begin(), index_translation_table.end(), generator());
        }
    }
    StoRandomDistribution(rng_type& rng, IntType a, IntType b, const std::vector<IntType>& index_table)
        : begin(a), end(b), index_transform(true), index_translation_table(index_table),
        uis(rng), dist() {assert(a < b);}

    virtual ~StoRandomDistribution() {}

    IntType sample() const {
        auto s_index = sample_idx();
        if (index_transform) {
            return index_translation_table[s_index];
        } else {
            return s_index + begin;
        }
    }

    IntType sample(rng_type& rng) const {
        auto s_index = sample_idx(rng);
        if (index_transform) {
            return index_translation_table[s_index];
        } else {
            return s_index + begin;
        }
    }

    virtual uint64_t sample_idx() const {
        return dist(uis.generator());
    }

    virtual uint64_t sample_idx(rng_type& rng) const {
        return dist(rng);
    }

    IntType idx_translate(uint64_t idx) const {
        return index_translation_table[idx];
    }

    rng_type& generator() const {
        return uis.generator();
    }

    trace_type sample_trace(size_t num_samples) const {
        trace_type trace;
        for (size_t i = 0; i < num_samples; ++i)
            trace.push_back(sample());
        return trace;
    }

protected:
    void generate(weight_type&& pmf) {
        std::discrete_distribution<IntType> d_(pmf.begin(), pmf.end());
        dist.param(d_.param());
    }

    IntType begin;
    IntType end;
    bool index_transform;
    std::vector<IntType> index_translation_table;

    mutable StoUniformIntSampler<uint64_t> uis;
    // the core distribution
    mutable std::discrete_distribution<uint64_t> dist;
};

// specialization 1: uniform random distribution
template <typename IntType = index_t>
class StoUniformDistribution : public StoRandomDistribution<IntType> {
public:
    using typename StoRandomDistribution<IntType>::rng_type;
    StoUniformDistribution(rng_type& rng, IntType a, IntType b, bool shuffle = false) :
        StoRandomDistribution<IntType>(rng, a, b, shuffle) {generate(generate_weights());}
    StoUniformDistribution(rng_type& rng, IntType a, IntType b, const std::vector<index_t>& index_table) :
        StoRandomDistribution<IntType>(rng, a, b, index_table) {generate(generate_weights());}

    uint64_t sample_idx() const override {
        return this->uis.sample();
    }

    uint64_t sample_idx(rng_type& rng) const override {
        return this->uis.sample(rng);
    }

private:
    weight_type generate_weights() {
        std::uniform_int_distribution<IntType> d(this->begin, this->end);
        this->uis.set_params(d.param());
        return weight_type();
    }
};

// specialization 2: zipf distribution
template <typename IntType = index_t>
class StoZipfDistribution : public StoRandomDistribution<IntType> {
public:
    static constexpr double default_skew = 1.0;
    using typename StoRandomDistribution<IntType>::rng_type;

    StoZipfDistribution(rng_type& rng, IntType a, IntType b, double skew = default_skew, bool shuffle = false) :
        StoRandomDistribution<IntType>(rng, a, b, shuffle), skewness(skew), sum_() {
        calculate_sum();
        generate(generate_weights());
    }
    StoZipfDistribution(rng_type& rng, IntType a, IntType b, double skew, const std::vector<IntType>& index_table) :
        StoRandomDistribution<IntType>(rng, a, b, index_table), skewness(skew), sum_() {
        calculate_sum();
        generate(generate_weights());
    }

private:
    weight_type generate_weights() {
        weight_type pmf;
        for (auto i = this->begin; i <= this->end; ++i) {
            double p = 1.0/(std::pow((double)(i - this->begin + 1), skewness)*sum_);
            //std::cout << p << std::endl;
            pmf.push_back(p);
        }
        return pmf;
    }

    void calculate_sum() {
        double s = 0.0;
        for (auto i = this->begin; i <= this->end; ++i)
            s += std::pow(1.0/(double)(i- this->begin +1), skewness);
        sum_ = s;
    }

    double skewness;
    double sum_;
};

// specialization 3: random distribution defined by a histogram
template <typename IntType>
class StoCustomDistribution : public StoRandomDistribution<IntType> {
public:
    using typename StoRandomDistribution<IntType>::rng_type;

    typedef struct {
        IntType identifier;
        size_t count;
    } histogram_pt_type;

    typedef struct {
        IntType identifier;
        double weight;
    } weighted_pt_type;

    typedef std::vector<histogram_pt_type> histogram_type;
    typedef std::vector<weighted_pt_type> weightgram_type;

    StoCustomDistribution(rng_type& rng, const histogram_type& histogram) :
        StoRandomDistribution<IntType>(rng, 0, histogram.size() - 1, true) {
        reset_translation_table(histogram);
        generate(generate_weight(histogram));
    }

    StoCustomDistribution(rng_type& rng, const weightgram_type& weightgram) :
        StoRandomDistribution<IntType>(rng, 0, weightgram.size() - 1, true) {
        reset_translation_table(weightgram);
        generate(extract_weight(weightgram));
    }

private:
    template <typename PointType>
    void reset_translation_table(const std::vector<PointType>& id_list) {
        assert(this->index_translation_table.size() == id_list.size());
        size_t idx = 0;
        for (auto& point : id_list) {
            this->index_translation_table[idx] = point.identifier;
            ++idx;
        }
    }

    weight_type generate_weight(const histogram_type& histogram) {
        weight_type pmf;
        for (auto& pair : histogram)
            pmf.push_back((double)pair.count);
        return pmf;
    }

    weight_type extract_weight(const weightgram_type& weightgram) {
        weight_type pmf;
        for (auto& point : weightgram)
            pmf.push_back(point.weight);
        return pmf;
    }
};

}; // namespace sampling
