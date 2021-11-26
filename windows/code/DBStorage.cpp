// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#include "pch.h"

#include "DBStorage.h"

#include <unordered_map>

namespace winrt
{
    using namespace Microsoft::ReactNative;
    using namespace Windows::ApplicationModel::Core;
    using namespace Windows::Foundation;
    using namespace Windows::Storage;
}  // namespace winrt

namespace
{
    using ExecCallback = int(SQLITE_CALLBACK *)(void *, int, char **, char **);

    // Execute the provided SQLite statement (and optional execCallback & user data
    // in pv). On error, reports it to the callback and returns false.
    bool Exec(sqlite3 *db,
              DBStorage::DBTask &task,
              const char *statement,
              ExecCallback execCallback = nullptr,
              void *pv = nullptr)
    {
        char *errMsg = nullptr;
        int rc = sqlite3_exec(db, statement, execCallback, pv, &errMsg);
        if (errMsg) {
            task.AddError(errMsg);
            sqlite3_free(errMsg);
            return false;
        }
        if (rc != SQLITE_OK) {
            task.AddError(sqlite3_errmsg(db));
            return false;
        }
        return true;
    }

    // Convenience wrapper for using Exec with lambda expressions
    template <class Fn>
    bool Exec(sqlite3 *db, DBStorage::DBTask &task, const char *statement, Fn &fn)
    {
        return Exec(
            db,
            task,
            statement,
            [](void *pv, int i, char **x, char **y) { return (*static_cast<Fn *>(pv))(i, x, y); },
            &fn);
    }

    // Checks that the args parameter is an array, that args.size() is less than
    // SQLITE_LIMIT_VARIABLE_NUMBER, and that every member of args is a string.
    // Invokes callback to report an error and returns false.
    bool CheckArgs(sqlite3 *db, const std::vector<winrt::JSValue> &args, DBStorage::DBTask &task)
    {
        int varLimit = sqlite3_limit(db, SQLITE_LIMIT_VARIABLE_NUMBER, -1);
        auto argCount = args.size();
        if (argCount > INT_MAX || static_cast<int>(argCount) > varLimit) {
            char errorMsg[60];
            sprintf_s(errorMsg, "Too many keys. Maximum supported keys :%d", varLimit);
            task.AddError(errorMsg);
            return false;
        }
        for (int i = 0; i < static_cast<int>(argCount); i++) {
            if (!args[i].TryGetString()) {
                task.AddError("Invalid key type. Expected a string");
                return false;
            }
        }
        return true;
    }

    // Checks that the args parameter is an array, that args.size() is less than
    // SQLITE_LIMIT_VARIABLE_NUMBER, and that every member of args is a string.
    // Invokes callback to report an error and returns false.
    bool CheckArgs(sqlite3 *db, const std::vector<std::string> &args, DBStorage::DBTask &task)
    {
        int varLimit = sqlite3_limit(db, SQLITE_LIMIT_VARIABLE_NUMBER, -1);
        auto argCount = args.size();
        if (argCount > INT_MAX || static_cast<int>(argCount) > varLimit) {
            char errorMsg[60];
            sprintf_s(errorMsg, "Too many keys. Maximum supported keys :%d", varLimit);
            task.AddError(errorMsg);
            return false;
        }
        for (int i = 0; i < static_cast<int>(argCount); i++) {
            if (args[i].empty()) {
                task.AddError("Invalid key type. Expected a string");
                return false;
            }
        }
        return true;
    }

    // RAII object to manage SQLite transaction. On destruction, if
    // Commit() has not been called, rolls back the transactions
    // The provided SQLite connection handle & Callback must outlive
    // the Sqlite3Transaction object
    struct Sqlite3Transaction {
        Sqlite3Transaction(sqlite3 *db, DBStorage::DBTask &task) : m_db(db), m_task(&task)
        {
            if (!Exec(m_db, *m_task, "BEGIN TRANSACTION")) {
                m_db = nullptr;
                m_task = nullptr;
            }
        }

        Sqlite3Transaction(const Sqlite3Transaction &) = delete;
        Sqlite3Transaction &operator=(const Sqlite3Transaction &) = delete;

        ~Sqlite3Transaction()
        {
            Rollback();
        }

        explicit operator bool() const
        {
            return m_db != nullptr;
        }

        bool Commit()
        {
            if (!m_db) {
                return false;
            }
            auto result = Exec(m_db, *m_task, "COMMIT");
            m_db = nullptr;
            m_task = nullptr;
            return result;
        }

        void Rollback()
        {
            if (m_db) {
                Exec(m_db, *m_task, "ROLLBACK");
                m_db = nullptr;
                m_task = nullptr;
            }
        }

    private:
        sqlite3 *m_db{nullptr};
        DBStorage::DBTask *m_task{nullptr};
    };

    // Appends argCount variables to prefix in a comma-separated list.
    std::string MakeSQLiteParameterizedStatement(const char *prefix, int argCount)
    {
        assert(argCount != 0);
        std::string result(prefix);
        result.reserve(result.size() + (argCount * 2) + 1);
        result += '(';
        for (int x = 0; x < argCount - 1; x++) {
            result += "?,";
        }
        result += "?)";
        return result;
    }

    // Checks if sqliteResult is SQLITE_OK. If not, reports the error via
    // callback & returns false.
    bool CheckSQLiteResult(sqlite3 *db, DBStorage::DBTask &task, int sqliteResult)
    {
        if (sqliteResult == SQLITE_OK) {
            return true;
        } else {
            task.AddError(sqlite3_errmsg(db));
            return false;
        }
    }

    using Statement = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;

    // Creates a prepared SQLite statement. On error, returns nullptr
    Statement PrepareStatement(sqlite3 *db, DBStorage::DBTask &task, const char *stmt)
    {
        sqlite3_stmt *pStmt{nullptr};
        if (!CheckSQLiteResult(db, task, sqlite3_prepare_v2(db, stmt, -1, &pStmt, nullptr))) {
            return {nullptr, sqlite3_finalize};
        }
        return {pStmt, &sqlite3_finalize};
    }

    // Binds the index-th variable in this prepared statement to str.
    bool BindString(sqlite3 *db,
                    DBStorage::DBTask &task,
                    const Statement &stmt,
                    int index,
                    const std::string &str)
    {
        return CheckSQLiteResult(
            db, task, sqlite3_bind_text(stmt.get(), index, str.c_str(), -1, SQLITE_TRANSIENT));
    }

    // Merge source into destination.
    // It only merges objects - all other types are just clobbered (including arrays).
    void MergeJsonObjects(winrt::Windows::Data::Json::JsonObject const &destination,
                          winrt::Windows::Data::Json::JsonObject const &source) noexcept
    {
        for (auto keyValue : source) {
            auto key = keyValue.Key();
            auto sourceValue = keyValue.Value();
            if (destination.HasKey(key)) {
                auto destinationValue = destination.GetNamedValue(key);
                if (destinationValue.ValueType() ==
                        winrt::Windows::Data::Json::JsonValueType::Object &&
                    sourceValue.ValueType() == winrt::Windows::Data::Json::JsonValueType::Object) {
                    MergeJsonObjects(destinationValue.GetObject(), sourceValue.GetObject());
                    continue;
                }
            }
            destination.SetNamedValue(key, sourceValue);
        }
    }
}  // namespace

sqlite3 *DBStorage::InitializeStorage(DBStorage::DBTask &task) noexcept
{
    winrt::slim_lock_guard guard{m_lock};
    if (m_db) {
        return m_db.get();
    }

    std::string path;
    try {
        if (auto pathInspectable =
                winrt::CoreApplication::Properties().TryLookup(s_dbPathProperty)) {
            path = winrt::to_string(winrt::unbox_value<winrt::hstring>(pathInspectable));
        } else {
            auto const localAppDataPath = winrt::ApplicationData::Current().LocalFolder().Path();
            path = winrt::to_string(localAppDataPath) + "\\AsyncStorage.db";
        }
    } catch (const winrt::hresult_error &error) {
        task.AddError(winrt::to_string(error.message()));
        task.AddError(
            "Please specify 'React-Native-Community-Async-Storage-Database-Path' in "
            "CoreApplication::Properties");
        return nullptr;
    }

    sqlite3 *db{};
    if (sqlite3_open_v2(path.c_str(),
                        &db,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                        nullptr) != SQLITE_OK) {
        task.AddError(sqlite3_errmsg(db));
        sqlite3_close(db);
        return nullptr;
    }
    m_db = {db, &sqlite3_close};

    int userVersion = 0;
    auto getUserVersionCallback =
        [](void *pv, int cCol, char **rgszColText, char ** /*rgszColName*/) {
            if (cCol < 1) {
                return 1;
            }
            *static_cast<int *>(pv) = atoi(rgszColText[0]);
            return SQLITE_OK;
        };

    if (!Exec(m_db.get(), task, "PRAGMA user_version", getUserVersionCallback, &userVersion)) {
        m_db.reset();
        return nullptr;
    }

    if (userVersion == 0) {
        if (!Exec(m_db.get(),
                  task,
                  "CREATE TABLE IF NOT EXISTS AsyncLocalStorage(key TEXT PRIMARY KEY, value TEXT "
                  "NOT NULL); PRAGMA user_version=1")) {
            m_db.reset();
            return nullptr;
        }
    }

    return m_db.get();
}

DBStorage::~DBStorage()
{
    decltype(m_tasks) tasks;
    {
        // If there is an in-progress async task, cancel it and wait on the
        // condition_variable for the async task to acknowledge cancellation by
        // nulling out m_action. Once m_action is null, it is safe to proceed
        // with closing the DB connection
        winrt::slim_lock_guard guard{m_lock};
        swap(tasks, m_tasks);
        if (m_action) {
            m_action.Cancel();
            m_cv.wait(m_lock, [this]() { return m_action == nullptr; });
        }
    }
}

// Under the lock, add a task to m_tasks and, if no async task is in progress,
// schedule it
void DBStorage::AddTask(std::function<void(DBStorage::DBTask &task, sqlite3 *db)> onRun,
                        std::function<void(DBStorage::DBTask &task)> onCancel) noexcept
{
    winrt::slim_lock_guard guard(m_lock);
    m_tasks.push_back(std::make_unique<DBTask>(std::move(onRun), std::move(onCancel)));
    if (!m_action) {
        m_action = RunTasks();
    }
}

// On a background thread, while the async task  has not been canceled and
// there are more tasks to do, run the tasks. When there are either no more
// tasks or cancellation has been requested, set m_action to null to report
// that and complete the coroutine. N.B., it is important that detecting that
// m_tasks is empty and acknowledging completion is done atomically; otherwise
// there would be a race between the background task detecting m_tasks.empty()
// and AddTask checking the coroutine is running.
winrt::Windows::Foundation::IAsyncAction DBStorage::RunTasks() noexcept
{
    auto cancellationToken = co_await winrt::get_cancellation_token();
    co_await winrt::resume_background();
    while (!cancellationToken()) {
        decltype(m_tasks) tasks;
        sqlite3 *db{nullptr};
        {
            winrt::slim_lock_guard guard(m_lock);
            if (m_tasks.empty()) {
                m_action = nullptr;
                m_cv.notify_all();
                co_return;
            }
            std::swap(tasks, m_tasks);
            db = m_db.get();
        }

        for (auto &task : tasks) {
            if (!cancellationToken()) {
                task->Run(*this, db);
            } else {
                task->Cancel();
            }
        }
    }
    winrt::slim_lock_guard guard(m_lock);
    m_action = nullptr;
    m_cv.notify_all();
}

DBStorage::DBTask::DBTask(std::function<void(DBTask &task, sqlite3 *db)> &&onRun,
                          std::function<void(DBTask &task)> &&onCancel) noexcept
    : m_onRun(std::move(onRun)), m_onCancel(std::move(onCancel))
{
}

DBStorage::DBTask::~DBTask() noexcept
{
    Cancel();
}

void DBStorage::DBTask::AddError(std::string message) noexcept
{
    m_errors.push_back(Error{std::move(message)});
}

const std::vector<DBStorage::Error> &DBStorage::DBTask::GetErrors() const noexcept
{
    return m_errors;
}

void DBStorage::DBTask::Run(DBStorage& storage, sqlite3 *db) noexcept
{
    if (!db) {
        db = storage.InitializeStorage(*this);
    }
    m_onRun(*this, db);
}

void DBStorage::DBTask::Cancel() noexcept
{
    if (m_errors.empty()) {
        AddError("Task is canceled");
    }
    m_onCancel(*this);
}

std::optional<std::vector<DBStorage::KeyValue>>
DBStorage::DBTask::MultiGet(sqlite3 *db, const std::vector<std::string> &keys) noexcept
{
    if (keys.empty()) {
        // Nothing to do.
        return std::vector<DBStorage::KeyValue>();
    }

    if (!CheckArgs(db, keys, *this)) {
        return std::nullopt;
    }

    auto argCount = static_cast<int>(keys.size());
    auto sql = MakeSQLiteParameterizedStatement(
        "SELECT key, value FROM AsyncLocalStorage WHERE key IN ", argCount);
    auto pStmt = PrepareStatement(db, *this, sql.data());
    if (!pStmt) {
        return std::nullopt;
    }
    for (int i = 0; i < argCount; i++) {
        if (!BindString(db, *this, pStmt, i + 1, keys[i])) {
            return std::nullopt;
        }
    }

    std::vector<DBStorage::KeyValue> result;
    for (auto stepResult = sqlite3_step(pStmt.get()); stepResult != SQLITE_DONE;
         stepResult = sqlite3_step(pStmt.get())) {
        if (stepResult != SQLITE_ROW) {
            AddError(sqlite3_errmsg(db));
            return std::nullopt;
        }

        auto key = reinterpret_cast<const char *>(sqlite3_column_text(pStmt.get(), 0));
        if (!key) {
            AddError(sqlite3_errmsg(db));
            return std::nullopt;
        }
        auto value = reinterpret_cast<const char *>(sqlite3_column_text(pStmt.get(), 1));
        if (!value) {
            AddError(sqlite3_errmsg(db));
            return std::nullopt;
        }
        result.push_back(KeyValue{key, value});
    }
    return result;
}

std::optional<bool> DBStorage::DBTask::MultiSet(sqlite3 *db,
                                                const std::vector<KeyValue> &keyValues) noexcept
{
    if (keyValues.empty()) {
        // Nothing to do.
        return true;
    }

    Sqlite3Transaction transaction(db, *this);
    if (!transaction) {
        return std::nullopt;
    }
    auto pStmt =
        PrepareStatement(db, *this, "INSERT OR REPLACE INTO AsyncLocalStorage VALUES(?, ?)");
    if (!pStmt) {
        return std::nullopt;
    }
    for (const auto &keyValue : keyValues) {
        if (!BindString(db, *this, pStmt, 1, keyValue.Key) ||
            !BindString(db, *this, pStmt, 2, keyValue.Value)) {
            return std::nullopt;
        }
        auto rc = sqlite3_step(pStmt.get());
        if (rc != SQLITE_DONE && !CheckSQLiteResult(db, *this, rc)) {
            return std::nullopt;
        }
        if (!CheckSQLiteResult(db, *this, sqlite3_reset(pStmt.get()))) {
            return std::nullopt;
        }
    }
    if (!transaction.Commit()) {
        return std::nullopt;
    }
    return true;
}

std::optional<bool> DBStorage::DBTask::MultiMerge(sqlite3 *db,
                                                  const std::vector<KeyValue> &keyValues) noexcept
{
    std::vector<std::string> keys;
    std::unordered_map<std::string, std::string> newValues;
    keys.reserve(keyValues.size());
    for (const auto &keyValue : keyValues) {
        keys.push_back(keyValue.Key);
        newValues.try_emplace(keyValue.Key, keyValue.Value);
    }

    auto result = MultiGet(db, keys);
    if (!result) {
        return std::nullopt;
    }

    std::vector<KeyValue> mergedResults;

    for (size_t i = 0; i < result->size(); i++) {
        auto &key = result->at(i).Key;
        auto &oldValue = result->at(i).Value;
        // TODO: what if it is at different index?
        auto &newValue = newValues[key];

        winrt::Windows::Data::Json::JsonObject oldJson;
        winrt::Windows::Data::Json::JsonObject newJson;
        if (winrt::Windows::Data::Json::JsonObject::TryParse(winrt::to_hstring(oldValue),
                                                             oldJson) &&
            winrt::Windows::Data::Json::JsonObject::TryParse(winrt::to_hstring(newValue),
                                                             newJson)) {
            MergeJsonObjects(oldJson, newJson);

            mergedResults.push_back(KeyValue{key, winrt::to_string(oldJson.ToString())});
        } else {
            AddError("Values must be valid JSON object strings");
            return std::nullopt;
        }
    }

    return MultiSet(db, std::move(mergedResults));
}

std::optional<bool> DBStorage::DBTask::MultiRemove(sqlite3 *db,
                                                   const std::vector<std::string> &keys) noexcept

{
    if (!CheckArgs(db, keys, *this)) {
        return std::nullopt;
    }

    auto argCount = static_cast<int>(keys.size());
    auto sql =
        MakeSQLiteParameterizedStatement("DELETE FROM AsyncLocalStorage WHERE key IN ", argCount);
    auto pStmt = PrepareStatement(db, *this, sql.data());
    if (!pStmt) {
        return std::nullopt;
    }
    for (int i = 0; i < argCount; i++) {
        if (!BindString(db, *this, pStmt, i + 1, keys[i])) {
            return std::nullopt;
        }
    }
    for (auto stepResult = sqlite3_step(pStmt.get()); stepResult != SQLITE_DONE;
         stepResult = sqlite3_step(pStmt.get())) {
        if (stepResult != SQLITE_ROW) {
            AddError(sqlite3_errmsg(db));
            return std::nullopt;
        }
    }
    return true;
}

std::optional<std::vector<std::string>> DBStorage::DBTask::GetAllKeys(sqlite3 *db) noexcept
{
    std::vector<std::string> result;
    auto getAllKeysCallback = [&](int cCol, char **rgszColText, char **) {
        if (cCol >= 1) {
            result.emplace_back(rgszColText[0]);
        }
        return SQLITE_OK;
    };

    if (Exec(db, *this, "SELECT key FROM AsyncLocalStorage", getAllKeysCallback)) {
        return result;
    }
    return std::nullopt;
}

std::optional<bool> DBStorage::DBTask::RemoveAll(sqlite3 *db) noexcept
{
    if (Exec(db, *this, "DELETE FROM AsyncLocalStorage")) {
        return true;
    }
    return std::nullopt;
}

void ReadValue(const winrt::IJSValueReader &reader,
               /*out*/ DBStorage::KeyValue &value) noexcept
{
    if (reader.ValueType() == winrt::JSValueType::Array) {
        int index = 0;
        while (reader.GetNextArrayItem()) {
            if (index == 0) {
                ReadValue(reader, value.Key);
            } else if (index == 1) {
                ReadValue(reader, value.Value);
            } else {
                winrt::SkipValue<winrt::JSValue>(reader);
            }
            ++index;
        }
    }
}

void WriteValue(const winrt::Microsoft::ReactNative::IJSValueWriter &writer,
                const DBStorage::KeyValue &value) noexcept
{
    writer.WriteArrayBegin();
    WriteValue(writer, value.Key);
    WriteValue(writer, value.Value);
    writer.WriteArrayEnd();
}

void WriteValue(const winrt::IJSValueWriter &writer, const DBStorage::Error &value) noexcept
{
    writer.WriteObjectBegin();
    winrt::WriteProperty(writer, L"message", value.Message);
    writer.WriteObjectEnd();
}
