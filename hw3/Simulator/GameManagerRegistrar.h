// Simulator/GameManagerRegistrar.h
#pragma once

#include <vector>
#include <string>
#include <functional>
#include <memory>
#include <cassert>
#include "AbstractGameManager.h"

/// A factory that creates a new AbstractGameManager (e.g. plugin) given a 'verbose' flag
using GameManagerFactory = std::function<std::unique_ptr<AbstractGameManager>(bool verbose)>;

class GameManagerRegistrar {
    struct Entry {
        std::string so_name;
        GameManagerFactory factory;

        explicit Entry(const std::string& name)
          : so_name(name)
        {}

        void setFactory(GameManagerFactory&& f) {
            // assert(!factory);
            factory = std::move(f);
        }

        const std::string& name() const { return so_name; }

        std::unique_ptr<AbstractGameManager> create(bool verbose) const {
            return factory(verbose);
        }

        bool hasFactory() const {
            return static_cast<bool>(factory);
        }
    };

    std::vector<Entry> entries;
    static GameManagerRegistrar registrar;

public:
    /// Get the singleton registrar
    static GameManagerRegistrar& get();

    /// Push a new entry (before you dlopen)
    void createGameManagerEntry(const std::string& name) {
        entries.emplace_back(name);
    }

    /// Called by GameManagerRegistration to attach the factory
    void addGameManagerFactoryToLastEntry(GameManagerFactory&& f) {
        entries.back().setFactory(std::move(f));
    }

    struct BadRegistrationException {
        std::string name;
        bool hasName;
        bool hasFactory;
    };

    /// Validate that the last entry has both a non-empty name and a factory
    void validateLastRegistration() {
        const auto& last = entries.back();
        bool hn = !last.name().empty();
        if (!hn || !last.hasFactory()) {
            throw BadRegistrationException{
                .name       = last.name(),
                .hasName    = hn,
                .hasFactory = last.hasFactory()
            };
        }
    }

    /// Roll back a failed registration
    void removeLast() {
        entries.pop_back();
    }

    /// Iterate over all successful registrations
    auto begin() const { return entries.begin(); }
    auto end()   const { return entries.end();   }

    /// Number of live entries
    std::size_t count() const { return entries.size(); }

    /// Clear everything (e.g. at shutdown)
    void clear() { entries.clear(); }
};
