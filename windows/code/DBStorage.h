// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once

#include <winsqlite/winsqlite3.h>

#include "NativeModules.h"

class DBStorage
{
public:
    struct Error {
        std::string Message;
    };

    struct KeyValue {
        std::string Key;
        std::string Value;
    };

    template <typename TOnResolve, typename TOnReject>
    struct Promise {

        Promise(TOnResolve &&onResolve, TOnReject &&onReject) noexcept
            : m_onResolve(std::move(onResolve)), m_onReject(std::move(onReject))
        {
        }

        ~Promise()
        {
            VerifyElseCrash(m_isCompleted.test_and_set());
        }

        Promise(const Promise &other) = delete;
        Promise &operator=(const Promise &other) = delete;

        template <typename TValue>
        void Resolve(const TValue &value) noexcept
        {
            Complete([&] { m_onResolve(value); });
        }

        void Reject(const std::vector<Error> &errors) noexcept
        {
            Complete([&] { m_onReject(errors); });
        }

    private:
        template <typename Fn>
        void Complete(Fn &&fn)
        {
            if (m_isCompleted.test_and_set() == false) {
                fn();
            }
        }

    private:
        std::atomic_flag m_isCompleted{false};
        TOnResolve m_onResolve;
        TOnReject m_onReject;
    };

    struct DBTask {
        DBTask(std::function<void(DBTask &task, sqlite3 *db)> &&onInvoke,
               std::function<void(DBTask &task)> &&m_onCancel) noexcept;
        DBTask() = default;
        DBTask(const DBTask &) = delete;
        DBTask &operator=(const DBTask &) = delete;

        ~DBTask();

        void Invoke(sqlite3 *db) noexcept;
        void Cancel() noexcept;

        void AddError(std::string message) noexcept;
        const std::vector<Error> &GetErrors() const noexcept;

        std::optional<std::vector<KeyValue>>
        MultiGet(sqlite3 *db, const std::vector<std::string> &keys) noexcept;
        std::optional<bool> MultiSet(sqlite3 *db, const std::vector<KeyValue> &keyValues) noexcept;
        std::optional<bool> MultiMerge(sqlite3 *db,
                                       const std::vector<KeyValue> &keyValues) noexcept;
        std::optional<bool> MultiRemove(sqlite3 *db, const std::vector<std::string> &keys) noexcept;
        std::optional<std::vector<std::string>> GetAllKeys(sqlite3 *db) noexcept;
        std::optional<bool> RemoveAll(sqlite3 *db) noexcept;

    private:
        std::function<void(DBTask &task, sqlite3 *db)> m_onInvoke;
        std::function<void(DBTask &task)> m_onCancel;
        std::vector<Error> m_errors;
    };

    DBStorage();
    ~DBStorage();

    template <typename TOnResolve, typename TOnReject>
    static auto CreatePromise(TOnResolve &&onResolve, TOnReject &&onReject) noexcept
    {
        using PromiseType = Promise<std::decay_t<TOnResolve>, std::decay_t<TOnReject>>;
        return std::make_shared<PromiseType>(std::forward<TOnResolve>(onResolve),
                                             std::forward<TOnReject>(onReject));
    }

    void AddTask(std::function<void(DBStorage::DBTask &task, sqlite3 *db)> onInvoke,
                 std::function<void(DBStorage::DBTask &task)> onCancel) noexcept;

    winrt::Windows::Foundation::IAsyncAction RunTasks() noexcept;

private:
    static constexpr auto s_dbPathProperty = L"React-Native-Community-Async-Storage-Database-Path";

    sqlite3 *m_db;
    winrt::slim_mutex m_lock;
    winrt::slim_condition_variable m_cv;
    winrt::Windows::Foundation::IAsyncAction m_action{nullptr};
    std::vector<std::unique_ptr<DBTask>> m_tasks;

    std::string ConvertWstrToStr(const std::wstring &wstr);
};

void ReadValue(const winrt::Microsoft::ReactNative::IJSValueReader &reader,
               /*out*/ DBStorage::KeyValue &value) noexcept;

void WriteValue(const winrt::Microsoft::ReactNative::IJSValueWriter &writer,
                const DBStorage::KeyValue &value) noexcept;

void WriteValue(const winrt::Microsoft::ReactNative::IJSValueWriter &writer,
                const DBStorage::Error &value) noexcept;
