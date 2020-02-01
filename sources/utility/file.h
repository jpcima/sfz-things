#pragma once
#include <memory>
#include <cstdio>

struct FILE_deleter {
    void operator()(FILE *x) const
    {
        fclose(x);
    }
};

typedef std::unique_ptr<FILE, FILE_deleter> FILE_u;
