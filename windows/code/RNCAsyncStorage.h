// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once

#include <optional>

#include "DBStorage.h"
#include "NativeModules.h"

namespace winrt::ReactNativeAsyncStorage::implementation
{
    REACT_MODULE(RNCAsyncStorage)
    struct RNCAsyncStorage {
        REACT_METHOD(multiGet)
        void multiGet(std::vector<std::string> &&keys,
                      std::function<void(const std::vector<DBStorage::Error> &errors,
                                         const std::vector<DBStorage::KeyValue> &results)>
                          &&callback) noexcept;

        REACT_METHOD(multiSet)
        void multiSet(
            std::vector<DBStorage::KeyValue> &&keyValues,
            std::function<void(const std::vector<DBStorage::Error> &errors)> &&callback) noexcept;

        REACT_METHOD(multiMerge)
        void multiMerge(
            std::vector<DBStorage::KeyValue> &&keyValues,
            std::function<void(const std::vector<DBStorage::Error> &errors)> &&callback) noexcept;

        REACT_METHOD(multiRemove)
        void multiRemove(
            std::vector<std::string> &&keys,
            std::function<void(const std::vector<DBStorage::Error> &errors)> &&callback) noexcept;

        REACT_METHOD(getAllKeys)
        void
        getAllKeys(std::function<void(const std::optional<DBStorage::Error> &error,
                                      const std::vector<std::string> &keys)> &&callback) noexcept;

        REACT_METHOD(clear)
        void clear(
            std::function<void(const std::optional<DBStorage::Error> &error)> &&callback) noexcept;

    private:
        DBStorage m_dbStorage;
    };
}  // namespace winrt::ReactNativeAsyncStorage::implementation
