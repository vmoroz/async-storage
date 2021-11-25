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

    struct PromiseBase {
        PromiseBase(std::function<void(const std::vector<Error> &errors)> &&onFailure) noexcept
            : m_onFailure(std::move(onFailure))
        {
        }
        
        void AddError(std::string&& message) noexcept
        {
            m_errors.push_back(Error{std::move(message)});
        }

        ~PromiseBase()
        {
            RunOnce([&] {
                if (m_errors.empty()) {
                    AddError("Task canceled");
                }
                m_onFailure(m_errors);
            });
        }

        template <typename Fn>
        void RunOnce(Fn &&fn)
        {
            if (m_isCompleted.test_and_set() == false) {
                fn();
            }
        }

    private:
        std::atomic_flag m_isCompleted{false};
        std::function<void(const std::vector<Error> &errors)> m_onFailure;
        std::vector<Error> m_errors;
    };

    template <typename TValue>
    struct Promise : PromiseBase {
        Promise(std::function<void(const std::vector<Error> &errors, const TValue &value)>
                    &&callback) noexcept
            : PromiseBase([&](const std::vector<Error> &errors) { m_callback(errors, {}); }),
              m_callback(std::move(callback))
        {
        }
        
        void Resolve(TValue &&value) noexcept
        {
            RunOnce([&] {
                m_callback({}, value);
            });
        }

    private:
        std::function<void(const std::vector<Error> &errors, const TValue &value)> m_callback;
    };

    template <>
    struct Promise<void> : PromiseBase {
        Promise(std::function<void(const std::vector<Error> &errors)> &&callback) noexcept
            : PromiseBase([&](const std::vector<Error> &errors) { m_callback(errors); }),
              m_callback(std::move(callback))
        {
        }

        void Resolve() noexcept
        {
            RunOnce([&] { m_callback({}); });
        }

        private:
        std::function<void(const std::vector<Error> &errors)> m_callback;
    };

    using ResultCallback =
        std::function<void(const std::vector<Error> &errors, const std::vector<KeyValue> &results)>;

    using Callback = std::function<void(const std::vector<Error> &errors)>;

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
        MultiGetTask(std::vector<std::string> &&args, ResultCallback &&callback) noexcept
            : m_args{std::move(args)}, m_callback{std::move(callback)}
        {
        }

        void Run(sqlite3 *db) override;

    private:
        std::vector<std::string> m_args;
        ResultCallback m_callback;
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

    struct MultiMergeTask : DBTask {
        MultiMergeTask(std::vector<KeyValue> &&args, Callback &&callback)
            : m_args{std::move(args)}, m_callback{std::move(callback)}
        {
        }

        void Run(sqlite3 *db) override;

    private:
        std::vector<KeyValue> m_args;
        Callback m_callback;
    };

    struct MultiRemoveTask : DBTask {
        MultiRemoveTask(std::vector<std::string> &&args, Callback &&callback)
            : m_args{std::move(args)}, m_callback{std::move(callback)}
        {
        }

        void Run(sqlite3 *db) override;

    private:
        std::vector<std::string> m_args;
        Callback m_callback;
    };

    struct ClearTask : DBTask {
        ClearTask(std::function<void(const DBStorage::Error &error)> &&callback)
            : m_callback{std::move(callback)}
        {
        }

        void Run(sqlite3 *db) override;

    private:
        std::function<void(const DBStorage::Error &error)> &&m_callback;
    };

    struct GetAllKeysTask : DBTask {
        GetAllKeysTask(std::function<void(const DBStorage::Error &error,
                                          const std::vector<std::string> &keys)> &&callback)
            : m_callback{std::move(callback)}
        {
        }

        void Run(sqlite3 *db) override;

    private:
        std::function<void(const DBStorage::Error &error, const std::vector<std::string> &keys)>
            m_callback;
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

void WriteValue(const winrt::Microsoft::ReactNative::IJSValueWriter &writer,
                const DBStorage::KeyValue &value) noexcept;

void WriteValue(const winrt::Microsoft::ReactNative::IJSValueWriter &writer,
                const DBStorage::Error &value) noexcept;
