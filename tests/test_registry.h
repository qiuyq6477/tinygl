#pragma once

#include <string>
#include <map>
#include <functional>
#include <vector>
#include <memory>
#include "ITestCase.h"

// Factory function type
using TestCaseFactory = std::function<ITestCase*()>;

class TestCaseRegistry {
public:
    static TestCaseRegistry& get() {
        static TestCaseRegistry instance;
        return instance;
    }

    void registerTestCase(const std::string& group, const std::string& name, TestCaseFactory factory) {
        m_tests[group][name] = factory;
    }

    // Returns a map of Group -> { Name -> Factory }
    const std::map<std::string, std::map<std::string, TestCaseFactory>>& getTests() const {
        return m_tests;
    }

private:
    std::map<std::string, std::map<std::string, TestCaseFactory>> m_tests;
};

// Helper class for static registration
class TestRegistrar {
public:
    TestRegistrar(const std::string& group, const std::string& name, TestCaseFactory factory) {
        TestCaseRegistry::get().registerTestCase(group, name, factory);
    }
};
