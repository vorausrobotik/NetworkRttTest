#pragma once

#include <algorithm>
#include <chrono>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

namespace vr::RTUtils
{
namespace detail
{
    template <typename T, typename Enable = void>
    struct SumTypeDeducer;

    template <typename T, typename Enable = void>
    struct DefaultNanosecondConverter;
}  // namespace detail

/**
 * Histogram with a fixed bucket width size but templated value.
 * The supported range of the histogram is [startValue, buckedWidth * numberOfBuckets)
 * Values can be added by using recordValue().
 * If the added value is not in the supported range, it is clipped to the first or the last bin.
 * The Histogram class also tracks metadata like min, max average etc.
 *
 * @tparam T The value Type to use.
 * @tparam SumT The type that should be used for summing ar averaging. This Type can be deduced for integral types and
 * for duration types.
 */
template <typename T, typename SumT = typename detail::SumTypeDeducer<T>::type>
class FixedHistogram
{
   public:
    static constexpr unsigned int DEFAULT_NUMBER_OF_BINS = 10000;
    using CountType = unsigned long;
    using ValueType = T;
    using SumType = SumT;

    /**
     * Creates a FixedHistogram.
     * @param startValue The minimum value that should be in range (inclusive).
     * @param binWidth The bucket width to use.
     * @param numberOfBins The number of buckets to use.
     */
    FixedHistogram(T startValue, T binWidth, unsigned int numberOfBins = DEFAULT_NUMBER_OF_BINS);

    /**
     * Records a value by binning.
     * If the value is larger than the largest representable value, it is added to the last bin.
     * If it is smaller than the smallest representable value, it is added to the first bin.
     * The metadata (max, min, etc.) are also updated accordingly.
     * @param val the value to add.
     * @return true if it fits into the representable range.
     */
    bool recordValue(const T& val);

    /**
     * Return the index of the bin in which the passed value belongs.
     * The index is clipped to the supported range.
     * @param val the value to test.
     * @return the index of the bin in which the passed value belongs.
     */
    size_t indexOf(const T& val) const;

    /**
     * Returns the (inclusive) start value of a bin index.
     * @param index The bin index.
     * @return The inclusive start value of the bin.
     */
    T getStartValueOfBin(size_t index) const { return smallestPossibleValue_ + index * bucketWidth_; }

    /**
     * Returns the smallest value that was recorded.
     * nullopt if no value was recorded.
     * @return The smallest recorded value or nullopt if no value was recorded yet.
     */
    std::optional<T> min() const { return min_; }

    /**
     * Returns the largest value that was recorded.
     * nullopt if no value was recorded.
     * @return The largest recorded value or nullopt if no value was recorded yet.
     */
    std::optional<T> max() const { return max_; }

    /**
     * Returns the last value that was recorded.
     * nullopt if no value was recorded.
     * @return The last recorded value or nullopt if no value was recorded yet.
     */
    std::optional<T> last() const { return last_; }

    /**
     * Returns the number of bins in this histogram.
     * @return number of bins in histogram.
     */
    size_t getNumberOfBins() const { return bins_.size(); }

    /**
     * Returns the number of values that were added to the histogram.
     * @return number of added values.
     */
    CountType count() const { return count_; }

    /**
     * Returns the actual histogram.
     * @see indexOf()
     * @see getStartValueOfBin()
     * @see getBucketWidth()
     * @return the actual histogram.
     */
    const std::vector<CountType>& getHistogram() const { return bins_; }

    /**
     * Returns the bucket width.
     * @return the bucket width.
     */
    T getBucketWidth() const { return bucketWidth_; }

    /**
     * Samples a value.
     * @param value The value to sample.
     * @return The number of recorded values which belongs in the same bin as the passed value.
     */
    CountType sampleValue(const T& value) const { return this->bins_.at(this->indexOf(value)); }

    /**
     * Returns the average value.
     * @return the average value.
     */
    std::optional<SumType> avg() const;

   private:
    T smallestPossibleValue_{};  // inclusive
    T largestPossibleValue_{};   // exclusive
    T bucketWidth_{};
    unsigned int numberOfBuckets_{};

    std::optional<T> min_ = std::nullopt;
    std::optional<T> max_ = std::nullopt;
    std::optional<T> last_ = std::nullopt;
    SumType sum_{};
    CountType count_{};

    std::vector<CountType> bins_;
};

/**
 * Converts a FixedHistogram to the voraus gnu gp format.
 * This function needs a way to convert and format the ValueType and the SumType of the passed histogram.
 * This can be deduced for integral types and chrono types.
 * For integral types nanoseconds are assumed, duration types are casted to nanoseconds before string formatting.
 * The Converter types must provide a static member function std::string asString(const T& val).
 *
 * @tparam HistT The type of the histogram to convert.
 * @tparam ValueTConverter The Converter for the ValueType of the histogram. This can be deduced for integral and
 * duration types.
 * @tparam SumTConverter The Converter for the SumType of the histogram. This can be deduced for integral and duration
 * types.
 * @param hist The histogram.
 * @return The histogram in voraus gnu gp format.
 */
template <class HistT,
          typename ValueTConverter = detail::DefaultNanosecondConverter<typename HistT::ValueType>,
          typename SumTConverter = detail::DefaultNanosecondConverter<typename HistT::SumType>>
std::string inVorausGpFormat(const HistT& hist);

template <class HistT,
          typename ValueTConverter = detail::DefaultNanosecondConverter<typename HistT::ValueType>,
          typename SumTConverter = detail::DefaultNanosecondConverter<typename HistT::SumType>>
std::string inJsonFormat(const HistT& hist);

}  // namespace vr::RTUtils

#include "RTUtils/detail/Histogram_impl.h"

namespace [[deprecated]] VR
{
using namespace vr;
}
