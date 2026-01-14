#pragma once

#include <string>
#include <map>
#include <functional>
#include <vector>
#include <memory>
#include "ITestCase.h"

// Factory function types
using TinyGLTestFactory = std::function<ITinyGLTestCase*()>;
using RHITestFactory = std::function<IRHITestCase*()>;

class TestCaseRegistry {
public:
    static TestCaseRegistry& get() {
        static TestCaseRegistry instance;
        return instance;
    }

    void registerTestCase(const std::string& group, const std::string& name, TinyGLTestFactory factory) {
        m_tinyglTests[group][name] = factory;
    }

    void registerTestCase(const std::string& group, const std::string& name, RHITestFactory factory) {
        m_rhiTests[group][name] = factory;
    }

    // Accessors
    const std::map<std::string, std::map<std::string, TinyGLTestFactory>>& getTinyGLTests() const {
        return m_tinyglTests;
    }

    const std::map<std::string, std::map<std::string, RHITestFactory>>& getRHITests() const {
        return m_rhiTests;
    }

private:
    std::map<std::string, std::map<std::string, TinyGLTestFactory>> m_tinyglTests;
    std::map<std::string, std::map<std::string, RHITestFactory>> m_rhiTests;
};

// Helper class for static registration
class TestRegistrar {
public:
    TestRegistrar(const std::string& group, const std::string& name, TinyGLTestFactory factory) {
        TestCaseRegistry::get().registerTestCase(group, name, factory);
    }
    
    TestRegistrar(const std::string& group, const std::string& name, RHITestFactory factory) {
        TestCaseRegistry::get().registerTestCase(group, name, factory);
    }
};