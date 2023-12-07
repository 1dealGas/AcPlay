#define LIB_NAME "libArf2"
#define MODULE_NAME "Arf2"

#include <dmsdk/sdk.h>
#include <dmsdk/dlib/vmath.h>
#include <dmsdk/script/script.h>
#include <arf2_generated.h>
#include <vector>
#include <map>

// API Globals
Arf2* Arf;
static double xscale, yscale, xdelta, ydelta, rotdeg;

// Internal Globals
uint16_t special_hint, dt_progress;
uint32_t dt_layer1, dt_layer2;
std::map<uint32_t, uint8_t> last_vec;
std::vector<uint32_t> blnums;

// Pointers
// Caches
extern float SIN[901];
extern float COS[901];
extern float ESIN[1001];
extern float ECOS[1001];
extern float SQRT[1001];
extern double RCP[8192];



// Script APIs, Under Construction
static inline int InitArf(lua_State *L)   // InitArf(str) -> before, total_hint, wgo_required, hgo_required
{
    xscale = 1.0;  yscale = 1.0;  xdelta = 0.0;  ydelta = 0.0;  rotdeg = 0.0;
    special_hint = 0;  dt_progress = 0;  dt_layer1 = 0;  dt_layer2 = 0;
    return 0;
}


typedef dmVMath::Vector3* v3p;
static inline int UpdateArf(lua_State *L)   // UpdateArf(mstime, table_w/wi/h/hi/ht/a/ai/at) -> hint_lost, wgo/hgo/ago_used
{
    return 0;
}


static inline int JudgeArf(lua_State *L)   // JudgeArf(mstime, table_touch) -> hint_hit, hint_lost
{
    return 0;
}


static inline int FinalArf(lua_State *L)
{
    return 0;
}



static inline int SetXScale(lua_State *L) { xscale = luaL_checknumber(L,1); return 0; }
static inline int SetYScale(lua_State *L) { yscale = luaL_checknumber(L,1); return 0; }
static inline int SetXDelta(lua_State *L) { xdelta = luaL_checknumber(L,1); return 0; }
static inline int SetYDelta(lua_State *L) { ydelta = luaL_checknumber(L,1); return 0; }
static inline int SetRotDeg(lua_State *L) { rotdeg = luaL_checknumber(L,1); return 0; }

// Defold Lifecycle Related Stuff
static const luaL_reg M[] =
{
    {"InitArf", InitArf},
    {"UpdateArf", UpdateArf},
    {"JudgeArf", JudgeArf},
    {"FinalArf", FinalArf},
    {"SetXScale", SetXScale},
    {"SetYScale", SetYScale},
    {"SetXDelta", SetXDelta},
    {"SetYDelta", SetYDelta},
    {"SetRotDeg", SetRotDeg},
    {0, 0}
};

static inline dmExtension::Result LuaInit(dmExtension::Params* params)
{
    lua_State* L = params->m_L;

    int top = lua_gettop(L);
    luaL_register(L, MODULE_NAME, M);
    lua_pop(L, 1);

    assert(top == lua_gettop(L));
    return dmExtension::RESULT_OK;
}

static inline dmExtension::Result OK(dmExtension::Params* params) { return dmExtension::RESULT_OK; }
static inline dmExtension::Result APPOK(dmExtension::AppParams* params) { return dmExtension::RESULT_OK; }
DM_DECLARE_EXTENSION(libArf2, LIB_NAME, APPOK, APPOK, LuaInit, 0, 0, OK)
