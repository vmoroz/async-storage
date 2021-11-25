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
        void multiGet(
            std::vector<std::string> &&keys,
            std::function<void(const std::vector<DBStorage::Error> &errors,
                               const std::vector<DBStorage::KeyValue> &result)> &&callback) noexcept
        {
            auto promise = DBStorage::CreatePromise(
                [callback](const std::vector<DBStorage::KeyValue> &result) {
                    callback({}, result);
                },
                [callback](const std::vector<DBStorage::Error> &errors) { callback(errors, {}); });
            m_dbStorage.AddTask(
                [promise, keys = std::move(keys)](DBStorage::DBTask &task, sqlite3 *db) noexcept {
                    if (auto result = task.MultiGet(db, keys)) {
                        promise->Resolve(*result);
                    } else {
                        promise->Reject(task.GetErrors());
                    }
                },
                [promise](DBStorage::DBTask &task) noexcept { promise->Reject(task.GetErrors()); });
        }

        REACT_METHOD(multiSet)
        void multiSet(
            std::vector<DBStorage::KeyValue> &&keyValues,
            std::function<void(const std::vector<DBStorage::Error> &errors)> &&callback) noexcept
        {
            auto promise = DBStorage::CreatePromise(
                [callback](bool /*value*/) { callback({}); },
                [callback](const std::vector<DBStorage::Error> &errors) { callback(errors); });
            m_dbStorage.AddTask(
                [promise, keyValues = std::move(keyValues)](DBStorage::DBTask &task,
                                                            sqlite3 *db) noexcept {
                    if (auto result = task.MultiSet(db, keyValues)) {
                        promise->Resolve(*result);
                    } else {
                        promise->Reject(task.GetErrors());
                    }
                },
                [promise](DBStorage::DBTask &task) noexcept { promise->Reject(task.GetErrors()); });
        }

        REACT_METHOD(multiMerge)
        void multiMerge(
            std::vector<DBStorage::KeyValue> &&keyValues,
            std::function<void(const std::vector<DBStorage::Error> &errors)> &&callback) noexcept
        {
            auto promise = DBStorage::CreatePromise(
                [callback](bool /*value*/) { callback({}); },
                [callback](const std::vector<DBStorage::Error> &errors) { callback(errors); });
            m_dbStorage.AddTask(
                [promise, keyValues = std::move(keyValues)](DBStorage::DBTask &task,
                                                            sqlite3 *db) noexcept {
                    if (auto result = task.MultiMerge(db, keyValues)) {
                        promise->Resolve(*result);
                    } else {
                        promise->Reject(task.GetErrors());
                    }
                },
                [promise](DBStorage::DBTask &task) noexcept { promise->Reject(task.GetErrors()); });
        }

        REACT_METHOD(multiRemove)
        void multiRemove(
            std::vector<std::string> &&keys,
            std::function<void(const std::vector<DBStorage::Error> &errors)> &&callback) noexcept
        {
            auto promise = DBStorage::CreatePromise(
                [callback](bool /*value*/) { callback({}); },
                [callback](const std::vector<DBStorage::Error> &errors) { callback(errors); });
            m_dbStorage.AddTask(
                [promise, keys = std::move(keys)](DBStorage::DBTask &task, sqlite3 *db) noexcept {
                    if (auto result = task.MultiRemove(db, keys)) {
                        promise->Resolve(*result);
                    } else {
                        promise->Reject(task.GetErrors());
                    }
                },
                [promise](DBStorage::DBTask &task) noexcept { promise->Reject(task.GetErrors()); });
        }

        REACT_METHOD(getAllKeys)
        void
        getAllKeys(std::function<void(const std::optional<DBStorage::Error> &error,
                                      const std::vector<std::string> &keys)> &&callback) noexcept
        {
            auto promise = DBStorage::CreatePromise(
                [callback](const std::vector<std::string> &keys) { callback(std::nullopt, keys); },
                [callback](const std::vector<DBStorage::Error> &errors) {
                    callback(errors[0], {});
                });
            m_dbStorage.AddTask(
                [promise](DBStorage::DBTask &task, sqlite3 *db) noexcept {
                    if (auto result = task.GetAllKeys(db)) {
                        promise->Resolve(*result);
                    } else {
                        promise->Reject(task.GetErrors());
                    }
                },
                [promise](DBStorage::DBTask &task) noexcept { promise->Reject(task.GetErrors()); });
        }

        REACT_METHOD(clear)
        void
        clear(std::function<void(const std::optional<DBStorage::Error> &error)> &&callback) noexcept
        {
            auto promise = DBStorage::CreatePromise(
                [callback](bool /*value*/) { callback(std::nullopt); },
                [callback](const std::vector<DBStorage::Error> &errors) { callback(errors[0]); });
            m_dbStorage.AddTask(
                [promise](DBStorage::DBTask &task, sqlite3 *db) noexcept {
                    if (auto result = task.RemoveAll(db)) {
                        promise->Resolve(true);
                    } else {
                        promise->Reject(task.GetErrors());
                    }
                },
                [promise](DBStorage::DBTask &task) noexcept { promise->Reject(task.GetErrors()); });
        }

    private:
        DBStorage m_dbStorage;
    };
}  // namespace winrt::ReactNativeAsyncStorage::implementation
