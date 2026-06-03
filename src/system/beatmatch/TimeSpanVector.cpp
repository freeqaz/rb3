#include "beatmatch/TimeSpanVector.h"
#include "os/Debug.h"
#include <utility>

void IntersectTimeSpans(
    const std::vector<std::pair<float, float> > &v1,
    const std::vector<std::pair<float, float> > &v2,
    std::vector<std::pair<float, float> > &o_rIntersection
) {
    MILO_ASSERT(o_rIntersection.empty(), 0x14);
    const std::pair<float, float> *it1 = v1.data();
    const std::pair<float, float> *end1 = v1.data() + v1.size();
    const std::pair<float, float> *it2 = v2.data();
    const std::pair<float, float> *end2 = v2.data() + v2.size();
    for (; it1 != end1 && it2 != end2;) {
        float max1 = Max(it1->first, it2->first);
        float min2 = Min(it1->second, it2->second);
        if (max1 < min2) {
            o_rIntersection.push_back(std::make_pair(max1, min2));
        }
        if (it1->second <= min2)
            ++it1;
        if (it2->second <= min2)
            ++it2;
    }
}