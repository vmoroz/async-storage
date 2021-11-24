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

    struct DBTask {
        DBTask(std::vector<winrt::Microsoft::ReactNative::JSValue> &&args, Callback &&callback)
            : m_args{std::move(args)}, m_callback{std::move(callback)}
        {
        }

        DBTask(const DBTask &) = delete;
        DBTask(DBTask &&) = default;
        DBTask &operator=(const DBTask &) = delete;
        DBTask &operator=(DBTask &&) = default;

        virtual ~DBTask()
        {
        }

        virtual void Run(sqlite3 *db) = 0;

    protected:
        std::vector<winrt::Microsoft::ReactNative::JSValue> m_args;
        Callback m_callback;
    };

    struct MultiGetTask : DBTask {
        using DBTask::DBTask;
        void Run(sqlite3 *db) override;
    };

    struct MultiSetTask : DBTask {
        using DBTask::DBTask;
        void Run(sqlite3 *db) override;
    };

    struct MultiRemoveTask : DBTask {
        using DBTask::DBTask;
        void Run(sqlite3 *db) override;
    };

    struct ClearTask : DBTask {
        using DBTask::DBTask;
        void Run(sqlite3 *db) override;
    };

    struct GetAllKeysTask : DBTask {
        using DBTask::DBTask;
        void Run(sqlite3 *db) override;
    };

    DBStorage();
    ~DBStorage();

    template <typename Task>
    void AddTask(std::vector<winrt::Microsoft::ReactNative::JSValue> &&args, Callback &&jsCallback)
    {
        AddTask(std::make_unique<Task>(std::move(args), std::move(jsCallback)));
    }

    template <typename Task>
    void AddTask(Callback &&jsCallback)
    {
        AddTask<Task>({}, std::move(jsCallback));
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
