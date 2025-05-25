#include <future>
#include <iostream>
#include <memory>
#include <mutex>

#include <Windows.h>

#include <WinBio.h>

#include "../mcpp/transport/stdio.h"
#include "../mcpp/server.h"

using json = nlohmann::json;

class HelloFaceReco : public Tool
{
public:
    std::string Name() const override
    {
        return "reco";
    }

    std::string Description() const override
    {
        return "Invoke Windows Hello Facial Recognition to recognize the current user, returning account info";
    }

    json InputSchema() const override
    {
        return json::object();
    }

    ~HelloFaceReco()
    {
        CloseSession();
    }

    json Execute(const json& params) override
    {
        auto response = ExecuteImpl();
        CloseSession();
        return response;
    }

private:
    template<class TPtr>
    class WinBioPtr
    {
    public:
        WinBioPtr() = default;

        explicit WinBioPtr(TPtr* ptr) : m_ptr(ptr)
        {
        }

        ~WinBioPtr()
        {
            if (m_ptr)
            {
                WinBioFree(m_ptr);
            }
        }

        TPtr* Get() const
        {
            return m_ptr;
        }

        TPtr* operator->() const
        {
            return m_ptr;
        }

        TPtr** GetAddressOf()
        {
            return &m_ptr;
        }

    private:
        TPtr* m_ptr = nullptr;
    };

    static void CALLBACK WinBioSessionCallback(_In_ PWINBIO_ASYNC_RESULT pAsyncResult)
    {
        std::cerr << "WinBioSessionCallback: " << pAsyncResult->Operation << ' ' << pAsyncResult->ApiStatus << std::endl;

        WinBioPtr<WINBIO_ASYNC_RESULT> asyncResult(pAsyncResult);
        auto self = reinterpret_cast<HelloFaceReco*>(asyncResult->UserData);
        switch (asyncResult->Operation)
        {
        case WINBIO_OPERATION_MONITOR_PRESENCE:
            self->WinBioMonitorPresenceCallback(asyncResult.Get());
            break;
        case WINBIO_OPERATION_CLOSE:
            self->m_condvar.notify_all();
            break;
        default:
            break;
        }
    }

    void CloseSession()
    {
        if (m_sessionHandle)
        {
            WinBioCloseSession(m_sessionHandle);
            std::unique_lock lock(m_mutex);
            m_condvar.wait(lock);
            m_sessionHandle = 0;
        }
    }

    json ExecuteImpl()
    {
        HRESULT hr = S_OK;

        WinBioPtr<WINBIO_UNIT_SCHEMA> unitSchemaArray{};
        SIZE_T unitCount = 0;
        hr = WinBioEnumBiometricUnits(WINBIO_TYPE_FACIAL_FEATURES, unitSchemaArray.GetAddressOf(), &unitCount);
        if (FAILED(hr))
        {
            return Error(hr, "WinBioEnumBiometricUnits failed");
        }

        if (unitCount != 1)
        {
            return Error(E_UNEXPECTED, "Unexpected unit count: " + std::to_string(unitCount));
        }

        WINBIO_UNIT_ID unitId = unitSchemaArray.Get()[0].UnitId;

        hr = WinBioAsyncOpenSession(
            WINBIO_TYPE_FACIAL_FEATURES,
            WINBIO_POOL_SYSTEM,
            WINBIO_FLAG_DEFAULT,
            nullptr,
            0,
            WINBIO_DB_DEFAULT,
            WINBIO_ASYNC_NOTIFY_CALLBACK,
            nullptr,
            0,
            &WinBioSessionCallback,
            this,
            FALSE,
            &m_sessionHandle);
        if (FAILED(hr))
        {
            return Error(hr, "WinBioAsyncOpenSession failed");
        }

        hr = WinBioMonitorPresence(m_sessionHandle, unitId);
        if (FAILED(hr))
        {
            return Error(hr, "WinBioMonitorPresence failed");
        }

        auto future = m_promise.get_future();
        auto status = future.wait_for(std::chrono::seconds(10));
        if (status == std::future_status::timeout)
        {
            return Error(HRESULT_FROM_WIN32(ERROR_TIMEOUT), "Facial recognition timed out");
        }

        return Result(future.get());
    }

    void WinBioMonitorPresenceCallback(PWINBIO_ASYNC_RESULT asyncResult)
    {
        const auto& param = asyncResult->Parameters.MonitorPresence;
        std::cerr << "WinBioMonitorPresenceCallback: " << param.ChangeType << std::endl;
        switch (param.ChangeType)
        {
        case WINBIO_PRESENCE_CHANGE_TYPE_RECOGNIZE:
            WinBioPresenceRecognizedCallback(asyncResult);
            break;
        default:
            break;
        }
    }

    void WinBioPresenceRecognizedCallback(PWINBIO_ASYNC_RESULT asyncResult)
    {
        const auto& param = asyncResult->Parameters.MonitorPresence;
        if (param.PresenceCount < 1)
        {
            std::cerr << "PresenceCount: " << param.PresenceCount << std::endl;
            return;
        }

        const WINBIO_PRESENCE& presence = param.PresenceArray[0];
        const WINBIO_IDENTITY& identity = presence.Identity;
        switch (identity.Type)
        {
        case WINBIO_ID_TYPE_SID:
            WinBioAccountSidCallback(identity);
            break;
        default:
            break;
        }
    }

    void WinBioAccountSidCallback(const WINBIO_IDENTITY& identity)
    {
        char name[1024] = {};
        DWORD lenName = sizeof(name);
        char domain[1024] = {};
        DWORD lenDomain = sizeof(domain);
        SID_NAME_USE use{};
        LookupAccountSidA(nullptr, const_cast<UCHAR*>(identity.Value.AccountSid.Data), name, &lenName, domain, &lenDomain, &use);
        json result =
        {
            {"name", name},
            {"domain", domain},
        };
        m_promise.set_value(result.dump());
    }

    WINBIO_SESSION_HANDLE m_sessionHandle{};
    std::promise<std::string> m_promise{};
    std::mutex m_mutex{};
    std::condition_variable m_condvar{};
};

int main()
{
    json capabilities =
    {
        {"tools", json::object()},
    };
    Server server("helloface", "0.0.1", capabilities);
    server.AddTool(std::make_unique<HelloFaceReco>());
    server.Run(std::make_unique<Stdio>());
    return 0;
}