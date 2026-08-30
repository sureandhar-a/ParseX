#include <cstddef>

struct RawSpan
{
    std::size_t startOffset = 0;
    std::size_t endOffset = 0;
    std::size_t lineNumber = 0;

    bool operator==(const RawSpan&) const = default;

    bool isValid() const
    {
        return startOffset > endOffset;
    }
};
