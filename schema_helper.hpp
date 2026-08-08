#pragma once

#include "schemasystem/schemasystem.h"
#include "entity2/entityinstance.h"
#include <cstring>
#include <cstdint>
#include <cstdio>

#if defined(_WIN32)
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

typedef void* (*CreateInterfaceFn_Local)(const char* pName, int* pReturnCode);

inline ISchemaSystem* g_pSAWH_SchemaSystem = nullptr;

inline CSchemaSystemTypeScope* g_pSAWH_ServerTypeScope = nullptr;

inline bool SAWH_InitSchemaSystem()
{
#if defined(_WIN32)
    HMODULE hMod = GetModuleHandleA("schemasystem.dll");
    if (!hMod)
    {
        std::fprintf(stderr, "[SAWH] schemasystem.dll not found!\n");
        return false;
    }
    CreateInterfaceFn_Local fn = (CreateInterfaceFn_Local)GetProcAddress(hMod, "CreateInterface");
#else
    void* hMod = dlopen("libschemasystem.so", RTLD_NOLOAD | RTLD_NOW);
    if (!hMod)
    {
        std::fprintf(stderr, "[SAWH] libschemasystem.so not found: %s\n", dlerror());
        return false;
    }
    CreateInterfaceFn_Local fn = (CreateInterfaceFn_Local)dlsym(hMod, "CreateInterface");
#endif

    if (!fn)
    {
        std::fprintf(stderr, "[SAWH] CreateInterface symbol not found!\n");
        return false;
    }

    int ret = 0;
    g_pSAWH_SchemaSystem = (ISchemaSystem*)fn(SCHEMASYSTEM_INTERFACE_VERSION, &ret);
    if (!g_pSAWH_SchemaSystem)
    {
        std::fprintf(stderr, "[SAWH] ISchemaSystem was not provided (SchemaSystem_001)!\n");
        return false;
    }

    g_pSAWH_ServerTypeScope = g_pSAWH_SchemaSystem->FindTypeScopeForModule("libserver.so");
    if (!g_pSAWH_ServerTypeScope)
        g_pSAWH_ServerTypeScope = g_pSAWH_SchemaSystem->FindTypeScopeForModule("server.dll");

    if (!g_pSAWH_ServerTypeScope)
    {
        std::fprintf(stderr, "[SAWH] Server type scope not found!\n");
        return false;
    }

    return true;
}

inline int SAWH_GetSchemaOffset(const char* pClassName, const char* pFieldName)
{
    if (!g_pSAWH_ServerTypeScope) return -1;

    CSchemaClassInfo* pClassInfo = g_pSAWH_ServerTypeScope->FindDeclaredClass(pClassName).Get();
    if (!pClassInfo)
    {
        std::fprintf(stderr, "[SAWH] Schema class not found: %s\n", pClassName);
        return -1;
    }

    for (uint16_t i = 0; i < pClassInfo->m_nFieldCount; ++i)
    {
        const SchemaClassFieldData_t& field = pClassInfo->m_pFields[i];
        if (field.m_pszName && std::strcmp(field.m_pszName, pFieldName) == 0)
        {
            return field.m_nSingleInheritanceOffset;
        }
    }

    for (uint8_t b = 0; b < pClassInfo->m_nBaseClassCount; ++b)
    {
        SchemaBaseClassInfoData_t& base = pClassInfo->m_pBaseClasses[b];
        if (!base.m_pClass) continue;

        for (uint16_t i = 0; i < base.m_pClass->m_nFieldCount; ++i)
        {
            const SchemaClassFieldData_t& field = base.m_pClass->m_pFields[i];
            if (field.m_pszName && std::strcmp(field.m_pszName, pFieldName) == 0)
            {
                return (int)base.m_nOffset + field.m_nSingleInheritanceOffset;
            }
        }
    }

    std::fprintf(stderr, "[SAWH] Schema field not found: %s::%s\n", pClassName, pFieldName);
    return -1;
}

template <typename T>
inline T* SAWH_GetFieldPtr(void* pObject, int offset)
{
    if (!pObject || offset < 0) return nullptr;
    return reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(pObject) + offset);
}