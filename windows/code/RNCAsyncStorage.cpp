// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#include "pch.h"

#include "RNCAsyncStorage.h"

namespace winrt::ReactNativeAsyncStorage::implementation
{
    void RNCAsyncStorage::multiGet(
        std::vector<std::string> &&keys,
        std::function<void(const std::vector<DBStorage::Error> &errors,
                           const std::vector<DBStorage::KeyValue> &results)> &&callback) noexcept
    {
        //auto promise = std::make_shared<
        //    DBStorage::Promise<std::vector<DBStorage::KeyValue>, std::vector<DBStorage::Error>>>(
        //    std::move(callback));
        //m_dbStorage.AddTask<DBStorage::MultiGetTask>(std::move(keys), std::move(callback));
    }

    void RNCAsyncStorage::multiSet(
        std::vector<DBStorage::KeyValue> &&keyValues,
        std::function<void(const std::vector<DBStorage::Error> &errors)> &&callback) noexcept
    {
        //auto promise = std::make_shared<DBStorage::Promise<void, std::vector<DBStorage::Error>>>(
        //    std::move(callback));
        //m_dbStorage.AddTask<DBStorage::MultiSetTask>(std::move(keyValues), std::move(callback));
    }

    void RNCAsyncStorage::multiMerge(
        std::vector<DBStorage::KeyValue> &&keyValues,
        std::function<void(const std::vector<DBStorage::Error> &errors)> &&callback) noexcept
    {
        //auto promise = std::make_shared<DBStorage::Promise<void, std::vector<DBStorage::Error>>>(
        //    std::move(callback));
        //m_dbStorage.AddTask<DBStorage::MultiMergeTask>(std::move(keyValues), std::move(callback));
    }

    void RNCAsyncStorage::multiRemove(
        std::vector<std::string> &&keys,
        std::function<void(const std::vector<DBStorage::Error> &errors)> &&callback) noexcept
    {
        //auto promise = std::make_shared<DBStorage::Promise<void, std::vector<DBStorage::Error>>>(
        //    std::move(callback));
        //m_dbStorage.AddTask<DBStorage::MultiRemoveTask>(std::move(keys), std::move(callback));
    }

    void RNCAsyncStorage::getAllKeys(
        std::function<void(const std::optional<DBStorage::Error> &error,
                           const std::vector<std::string> &keys)> &&callback) noexcept
    {
        auto promise = DBStorage::CreatePromise(
            [callback](const std::vector<std::string> &keys) { callback(std::nullopt, keys); },
            [callback](const std::vector<DBStorage::Error> &errors) { callback(errors[0], {}); });
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

    void RNCAsyncStorage::clear(
        std::function<void(const std::optional<DBStorage::Error> &error)> &&callback) noexcept
    {
        //auto promise = std::make_shared<DBStorage::Promise<void, std::optional<DBStorage::Error>>>(
        //    std::move(callback));
        //m_dbStorage.AddTask<DBStorage::ClearTask>(std::move(callback));
    }

}  // namespace winrt::ReactNativeAsyncStorage::implementation
