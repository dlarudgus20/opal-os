#ifndef LIBKCDLTEST_H
#define LIBKCDLTEST_H

#include <gtest/gtest.h>

#include "LibkcApi.h"

class LibkcDlTest : public ::testing::Test {
protected:
    static LibkcApi api;

    static void SetUpTestSuite() {
        api = load_api();
    }

    static void TearDownTestSuite() {
        unload_api(api);
    }
};

#endif
