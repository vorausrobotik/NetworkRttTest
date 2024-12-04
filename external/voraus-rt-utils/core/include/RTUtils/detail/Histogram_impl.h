#pragma once
namespace vr::RTUtils
{
namespace detail
{
    template <class T>
    struct IsDuration : std::false_type
    {
    };

    template <class Rep, class Period>
    struct IsDuration<std::chrono::duration<Rep, Period>> : std::true_type
    {
    };
    static_assert(!IsDuration<int>::value);
    static_assert(IsDuration<std::chrono::nanoseconds>::value);

    template <typename T>
    struct SumTypeDeducer<T, typename std::enable_if_t<std::is_arithmetic_v<T>>>
    {
        using type = double;
    };

    template <typename T>
    struct SumTypeDeducer<T, typename std::enable_if_t<IsDuration<T>::value>>
    {
        using type = std::chrono::duration<double, typename T::period>;
    };

    static_assert(std::is_same_v<SumTypeDeducer<int>::type, double>);

}  // namespace detail

namespace detail
{
    template <typename T>
    using ConvertableToString = decltype(std::to_string(std::declval<T>()));

    template <typename T, class = void>
    struct IsConvertableToString : std::false_type
    {
    };

    template <typename T>
    struct IsConvertableToString<T, std::void_t<ConvertableToString<T>>> : std::true_type
    {
    };

    static_assert(IsConvertableToString<int>::value);

    template <typename T>
    // matches any T which can be put into std::to_string(T)
    struct DefaultNanosecondConverter<T, typename std::enable_if_t<IsConvertableToString<T>::value>>
    {
        static std::string asString(const T& val) { return std::to_string(val); }
    };

    template <typename T>
    struct DefaultNanosecondConverter<T, typename std::enable_if_t<IsDuration<T>::value>>
    {
        static std::string asString(const T& val)
        {
            return std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(val).count());
        }
    };
}  // namespace detail

template <class HistT, typename ValueTConverter, typename SumTConverter>
std::string inVorausGpFormat(const HistT& hist)
{
    constexpr auto valAsStr = ValueTConverter::asString;
    constexpr auto sumAsStr = SumTConverter::asString;
    const auto& bins = hist.getHistogram();
    std::string result = "# \n";
    // dump header
    // the inconsistent spaces must be preserved to not break any tooling relying on this...
    result += "# Results: " + valAsStr(hist.max().value_or(typename HistT::ValueType{})) + " ns (max)  " +
              valAsStr(hist.min().value_or(typename HistT::ValueType{})) + " ns (min) " +
              sumAsStr(hist.avg().value_or(typename HistT::SumType{})) + " ns(avg) " + std::to_string(hist.count()) +
              " (count)\n";

    // find start and end
    auto start = std::find_if(bins.begin(), bins.end(), [](const auto& elem) { return elem != 0; });
    auto end = std::find_if(bins.rbegin(), bins.rend(), [](const auto& elem) { return elem != 0; });

    if (start != std::end(bins) && end != std::rend(bins))
    {
        const size_t startIndex = start - bins.begin();
        auto endAsForwardIt = (end + 1).base();  // end is a reverse iterator --> convert to forward it
        const size_t endIndex = endAsForwardIt - bins.begin();
        result += valAsStr(hist.getStartValueOfBin(startIndex)) + " 1\n";
        for (size_t i = startIndex; i <= endIndex; ++i)
        {
            result += valAsStr(hist.getStartValueOfBin(i) + hist.getBucketWidth() / 2) + " " +
                      std::to_string(bins.at(i) + 1) + "\n";
        }
        result += valAsStr(hist.getStartValueOfBin(endIndex + 1)) + " 1\n";
    }
    return result;
}

template <class HistT, typename ValueTConverter, typename SumTConverter>
std::string inJsonFormat(const HistT& hist)
{
    constexpr auto valAsStr = ValueTConverter::asString;
    constexpr auto sumAsStr = SumTConverter::asString;
    const auto& bins = hist.getHistogram();
    // NOLINTBEGIN(bugprone-unchecked-optional-access)
    std::string max = hist.max().has_value() ? valAsStr(hist.max().value()) : "null";
    std::string min = hist.min().has_value() ? valAsStr(hist.min().value()) : "null";
    std::string avg = hist.avg().has_value() ? sumAsStr(hist.avg().value()) : "null";
    // NOLINTEND(bugprone-unchecked-optional-access)
    std::string fileVersion = "\"1.0.0\"";

    std::string result = "{\n";
    result += "  \"type\": \"single_fixed_histogram\",\n";
    result += "  \"file_version\": " + fileVersion + ",\n";
    result += "  \"start_value\": " + valAsStr(hist.getStartValueOfBin(0)) + ",\n";
    result += "  \"bin_width\": " + valAsStr(hist.getBucketWidth()) + ",\n";
    result += "  \"number_of_bins\": " + std::to_string(bins.size()) + ",\n";
    result += "  \"max\": " + max + ",\n";
    result += "  \"min\": " + min + ",\n";
    result += "  \"avg\": " + avg + ",\n";
    result += "  \"count\": " + std::to_string(hist.count()) + ",\n";
    result += "  \"bins\": [\n";
    for (size_t i = 0; i < bins.size(); ++i)
    {
        result += "    " + std::to_string(bins.at(i));
        if (i != bins.size() - 1)
        {
            result += ",";
        }
        result += "\n";
    }
    result += "  ]\n";
    result += "}\n";
    return result;
}

template <typename T, typename SumT>
std::optional<SumT> FixedHistogram<T, SumT>::avg() const
{
    if (count_ == 0)
    {
        return {};
    }
    return sum_ / count_;
}
template <typename T, typename SumT>
size_t FixedHistogram<T, SumT>::indexOf(const T& val) const
{
    if (val < smallestPossibleValue_)
    {
        return 0;
    }
    if (val >= largestPossibleValue_)
    {
        return numberOfBuckets_ - 1;
    }
    return (val - smallestPossibleValue_) / bucketWidth_;
}

template <typename T, typename SumT>
FixedHistogram<T, SumT>::FixedHistogram(T startValue, T binWidth, unsigned int numberOfBins)
    : smallestPossibleValue_(startValue),
      largestPossibleValue_(startValue + numberOfBins * binWidth),
      bucketWidth_(binWidth),
      numberOfBuckets_(numberOfBins),
      bins_(numberOfBins, 0)

{
}

template <typename T, typename SumT>
bool FixedHistogram<T, SumT>::recordValue(const T& val)
{
    if (!max_.has_value() || val > max_)
    {
        max_ = val;
    }
    if (!min_.has_value() || val < min_)
    {
        min_ = val;
    }
    last_ = val;
    sum_ += val;
    ++count_;
    bool outOfBounds = val >= smallestPossibleValue_ && val < largestPossibleValue_;
    bins_[indexOf(val)] += 1;
    return outOfBounds;
}

}  // namespace vr::RTUtils