////////////////////////////////////////////////////////////////////////////
//	Module 		: object_factory_script.cpp
//	Created 	: 27.05.2004
//  Modified 	: 28.06.2004
//	Author		: Dmitriy Iassenev
//	Description : Object factory script export
////////////////////////////////////////////////////////////////////////////

#include "pch_script.h"

#include "object_factory.h"
#include "object_item_script.h"
#include "ai_space.h"

#include "xrScriptEngine/script_engine.hpp"

void CObjectFactory::register_script_class(LPCSTR client_class, LPCSTR server_class, LPCSTR clsid, LPCSTR script_clsid)
{
    ZoneScoped;

    Msg("* CObjectFactory: registering script class '%s' (client: '%s', server: '%s')", clsid, client_class, server_class);
    luabind::object client;
    if (!GEnv.ScriptEngine->function_object(client_class, client, LUA_TUSERDATA))
    {
        Msg("! CObjectFactory: Cannot register client class '%s' (clsid '%s')", client_class, clsid);
        GEnv.ScriptEngine->script_log(LuaMessageType::Error, "Cannot register class %s", client_class);
        return;
    }

    luabind::object server;
    if (!GEnv.ScriptEngine->function_object(server_class, server, LUA_TUSERDATA))
    {
        Msg("! CObjectFactory: Cannot register server class '%s' (clsid '%s')", server_class, clsid);
        GEnv.ScriptEngine->script_log(LuaMessageType::Error, "Cannot register class %s", server_class);
        return;
    }

    add(xr_new<CObjectItemScript>(client, server, TEXT2CLSID(clsid), script_clsid));
}

void CObjectFactory::register_script_class(LPCSTR unknown_class, LPCSTR clsid, LPCSTR script_clsid)
{
    ZoneScoped;

    Msg("* CObjectFactory: registering script class '%s' ('%s')", clsid, unknown_class);
    luabind::object creator;
    if (!GEnv.ScriptEngine->function_object(unknown_class, creator, LUA_TUSERDATA))
    {
        Msg("! CObjectFactory: Cannot register script class '%s' (clsid '%s')", unknown_class, clsid);
        GEnv.ScriptEngine->script_log(LuaMessageType::Error, "Cannot register class %s", unknown_class);
        return;
    }
    add(xr_new<CObjectItemScript>(creator, creator, TEXT2CLSID(clsid), script_clsid));
}

void CObjectFactory::register_script_classes()
{
    if (!GEnv.isDedicatedServer)
        ai();
}

struct CInternal
{
};

void CObjectFactory::register_script() const
{
    ZoneScoped;

    actualize();

    luabind::class_<CInternal> instance("clsid");

    const_iterator I = clsids().begin(), B = I;
    const_iterator E = clsids().end();
    for (; I != E; ++I)
        instance.enum_("_clsid")[luabind::value((*I)->script_clsid().c_str(), int(I - B))];

    luabind::module(GEnv.ScriptEngine->lua())[instance];
}

void CObjectFactory::script_register(lua_State* luaState)
{
    using namespace luabind;

    module(luaState)
    [
        class_<CObjectFactory>("object_factory")
        .def("register", (void (CObjectFactory::*)(LPCSTR, LPCSTR, LPCSTR, LPCSTR))(
                             &CObjectFactory::register_script_class))
        .def("register", (void (CObjectFactory::*)(LPCSTR, LPCSTR, LPCSTR))(
                             &CObjectFactory::register_script_class))
    ];
}
