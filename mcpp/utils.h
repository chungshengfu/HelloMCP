#pragma once

static constexpr size_t fnv1a_hash(const char* str, size_t value = 14695981039346656037ULL)
{
    return *str ? fnv1a_hash(str + 1, (value ^ *str) * 1099511628211ULL) : value;
}

#define S_SWITCH(x) switch (fnv1a_hash(std::data(x)))

#define S_CASE(x) case fnv1a_hash(x)
