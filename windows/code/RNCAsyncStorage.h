// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once

// TODO: remove
#include "pch.h"

#include "DBStorage.h"
#include "NativeModules.h"

namespace winrt::ReactNativeAsyncStorage::implementation
{
    REACT_MODULE(RNCAsyncStorage);
    struct RNCAsyncStorage {
        REACT_METHOD(multiGet)
        void multiGet(std::vector<std::string> &&keys,
                      DBStorage::ResultCallback &&callback) noexcept
        {
            m_dbStorage.AddTask<DBStorage::MultiGetTask>(std::move(keys), std::move(callback));
        }

        REACT_METHOD(multiSet)
        void multiSet(std::vector<DBStorage::KeyValue> &&keyValues,
                      DBStorage::Callback &&callback) noexcept
        {
            m_dbStorage.AddTask<DBStorage::MultiSetTask>(std::move(keyValues), std::move(callback));
        }

        REACT_METHOD(multiMerge)
        void multiMerge(std::vector<DBStorage::KeyValue> &&keyValues,
                        DBStorage::Callback &&callback) noexcept
        {
            m_dbStorage.AddTask<DBStorage::MultiMergeTask>(std::move(keyValues),
                                                           std::move(callback));
        }

        REACT_METHOD(multiRemove)
        void multiRemove(std::vector<std::string> &&keys, DBStorage::Callback &&callback) noexcept
        {
            m_dbStorage.AddTask<DBStorage::MultiRemoveTask>(std::move(keys), std::move(callback));
        }

        REACT_METHOD(getAllKeys)
        void getAllKeys(
            std::function<void(const DBStorage::Error &error, const std::vector<std::string> &keys)>
                &&callback) noexcept
        {
            m_dbStorage.AddTask<DBStorage::GetAllKeysTask>(std::move(callback));
        }

        REACT_METHOD(clear)
        void clear(std::function<void(const DBStorage::Error &error)> &&callback) noexcept
        {
            m_dbStorage.AddTask<DBStorage::ClearTask>(std::move(callback));
        }

    private:
        DBStorage m_dbStorage;
    };
}  // namespace winrt::ReactNativeAsyncStorage::implementation
