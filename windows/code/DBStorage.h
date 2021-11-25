// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once

#include <winsqlite/winsqlite3.h>

#include "NativeModules.h"

class DBStorage
{
public:
    typedef std::function<void(std::vector<winrt::Microsoft::ReactNative::JSValue> const &)>
        Callback;

    struct Error {
        std::string Message;
    };

    struct KeyValue {
        std::string Key;
        std::string Value;
    };

    struct DBTask {
        DBTask() = default;
        DBTask(const DBTask &) = delete;
        DBTask(DBTask &&) = default;
        DBTask &operator=(const DBTask &) = delete;
        DBTask &operator=(DBTask &&) = default;

        virtual ~DBTask()
        {
        }

        virtual void Run(sqlite3 *db) = 0;

        void AddError(std::string message) noexcept;

    private:
        std::vector<Error> m_errors;
    };

    struct MultiGetTask : DBTask {
        MultiGetTask(std::vector<winrt::Microsoft::ReactNative::JSValue> &&args,
                     Callback &&callback)
            : m_args{std::move(args)}, m_callback{std::move(callback)}
        {
        }

        void Run(sqlite3 *db) override;

    private:
        std::vector<winrt::Microsoft::ReactNative::JSValue> m_args;
        Callback m_callback;
    };

    struct MultiSetTask : DBTask {
        MultiSetTask(std::vector<KeyValue> &&args, Callback &&callback)
            : m_args{std::move(args)}, m_callback{std::move(callback)}
        {
        }

        void Run(sqlite3 *db) override;

    private:
        std::vector<KeyValue> m_args;
        Callback m_callback;
    };

    struct MultiRemoveTask : DBTask {
        MultiRemoveTask(std::vector<winrt::Microsoft::ReactNative::JSValue> &&args,
                        Callback &&callback)
            : m_args{std::move(args)}, m_callback{std::move(callback)}
        {
        }

        void Run(sqlite3 *db) override;

    private:
        std::vector<winrt::Microsoft::ReactNative::JSValue> m_args;
        Callback m_callback;
    };

    struct ClearTask : DBTask {
        ClearTask(Callback &&callback) : m_callback{std::move(callback)}
        {
        }

        void Run(sqlite3 *db) override;

    private:
        Callback m_callback;
    };

    struct GetAllKeysTask : DBTask {
        GetAllKeysTask(Callback &&callback) : m_callback{std::move(callback)}
        {
        }

        void Run(sqlite3 *db) override;

    private:
        Callback m_callback;
    };

    DBStorage();
    ~DBStorage();

    template <typename Task, typename... TArgs>
    void AddTask(TArgs &&...args)
    {
        AddTask(std::make_unique<Task>(std::forward<TArgs>(args)...));
    }

    void AddTask(std::unique_ptr<DBTask> task);

    winrt::Windows::Foundation::IAsyncAction RunTasks();

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
