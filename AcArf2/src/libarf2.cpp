#define LIB_NAME "libArf2"
#define MODULE_NAME "Arf2"

#include <dmsdk/sdk.h>
#include <dmsdk/dlib/vmath.h>
#include <dmsdk/script/script.h>
#include <arf2_generated.h>
#include <vector>
#include <map>

// Data & Globals
// For Safety Concern, Nothing will happen if !ArfSize.
static size_t ArfSize = 0;
static char* ArfBuf = nullptr;
static double xscale, yscale, xdelta, ydelta, rotdeg;

// Internal Globals
static uint16_t special_hint, dt_p1, dt_p2;
static std::map<uint32_t, uint8_t> last_vec;
static std::vector<uint32_t> blnums;

// Caches
extern float SIN[901]; extern float COS[901];
extern float ESIN[1001]; extern float ECOS[1001]; extern float SQRT[1001];
extern double RCP[8192];

// Input Functions
typedef dmVMath::Vector3* v3p;
static inline bool has_touch_near( uint64_t hint, v3p touches[10] ) {

	// Hint: (1)terminated+(19)judged_ms+(19)ms+(12)y+(13)x
	// x:[0,48]   visible -> [16,32]
	// y:[0,24]   visible -> [8,16]

	uint64_t u;					int16_t s;			// *0.87890625f  ->  /128.0f*112.5f
	u = hint & 0x1fff;			s = u - 2048;		float hint_l = (float)s * 0.87890625f - 168.75f;		float hint_r = hint_l + 337.5f;
	u = (hint>>13) & 0xfff;		s = u - 1024;		float hint_d = (float)s * 0.87890625f - 78.75f;			float hint_u = hint_d + 337.5f;

	// For each finger,
	for( uint8_t i=0; i<10; i++ ){

		v3p f = touches[i];
		uint8_t fs = (uint8_t)( f->getZ() );

		// if the finger is just pressed, or onscreen,
		if ( (fs==1) || (fs==2) ){

			// check its x pivot
			float x = f->getX();
			if( (x>=hint_l) && (x<=hint_r) ) {

				// if no problem with pos_x of the touch, check its y pivot.
				// if no problem with pos_y of the touch, return true.
				float y = f->getY();
				if ( (y>=hint_d) && (y<=hint_u) )  return true;

			}
		}
	}

	// if the table does contain a valid touch,
	return false;

}

static inline bool is_safe_to_anmitsu( uint64_t hint ){

	uint64_t u;					// No overflowing risk here.
	u = hint & 0x1fff;			int16_t hint_l = u - 384;		int16_t hint_r = u + 384;
	u = (hint>>13) & 0xfff;		int16_t hint_d = u - 384;		int16_t hint_u = u + 384;

	// Safe when blnums.size==0. The circulation is to be skipped then.
	// For registered Hints,
	for( uint16_t i=0; i<blnums.size(); i++ ){

		// check its x pivot,
		u = blnums[i] & 0x1fff;
		if( (u>hint_l) && (u<hint_r) ){

			// if problem happens with the x pivot,
			// check its y pivot, and if problem happens with the y pivot, return false.
			u = ( (blnums[i]) >> 13 ) & 0xfff;
			if( (u>hint_d) && (u<hint_u) ) return false;

		}
	}

	// If the Hint is safe to "Anmitsu", register it, and return true.
	u = hint & 0x1ffffff;
	blnums.push_back( (uint32_t)u );
	return true;
	
}



/* Script APIs, Under Construction
   S --> Safety guaranteed, by precluding the memory leakage.  */
static Arf2* Arf = nullptr;
#define S if(!ArfSize) return 0;
static inline int InitArf(lua_State *L)   // InitArf(str) -> before, total_hints, wgo_required, hgo_required
{
	xscale = 1.0;  yscale = 1.0;  xdelta = 0.0;  ydelta = 0.0;  rotdeg = 0.0;
	special_hint = 0;  dt_p1 = 0;  dt_p2 = 0;

	const char* B = luaL_checklstring(L, 1, &ArfSize);
	S ArfBuf = new char[ArfSize];
	for(uint64_t i=0; i<ArfSize; i++) ArfBuf[i] = B[i];
	lua_gc(L, LUA_GCCOLLECT, 0);

	DM_LUA_STACK_CHECK(L, 4);
	Arf = GetMutableArf2( ArfBuf );
	special_hint = Arf->special_hint();
	lua_pushnumber( L, Arf->before() );
	lua_pushnumber( L, Arf->total_hints() );
	lua_pushnumber( L, Arf->wgo_required() );
	lua_pushnumber( L, Arf->hgo_required() );
	return 4;
}


static inline int UpdateArf(lua_State *L)   // UpdateArf(mstime, table_w/wi/h/hi/ht/a/ai/at) -> hint_lost, wgo/hgo/ago_used
{S
	// Process msTime
	uint32_t mstime = luaL_checknumber(L, 1);
	uint32_t idx_group = mstime >> 9;

	// Check DTime

	// Search & Interpolate Wishes

	// Sweep Hints

	// Render Hints & Effects
	return 4;
}


static inline int JudgeArf(lua_State *L)   // JudgeArf(mstime, idelta, table_touch) -> hint_hit, hint_lost, special_hint_judged
{S
	// table_touch = { any_pressed, any_released, v(x,y,s)··· }, s0->invalid, s1->pressed, s2->onscreen, s3->released.
	// fixed to contain 12 elements
	double mstime = luaL_checknumber(L, 1);
	double idelta = luaL_checknumber(L, 2);
	lua_pop(L, 2);

	v3p touches[10];
	for( lua_Integer i=3; i<13; i++ ) {
		//                                              // table_touch
		lua_pushinteger(L, i);                          // table_touch  ->  i
		lua_gettable(L, 1);                             // table_touch  ->  i  -> table_touch[i]
		touches[i-3] = dmScript::CheckVector3(L, 3);
		lua_pop(L, 2);                                  // table_touch
	}


	return 3;
}


static inline int FinalArf(lua_State *L)
{S
	Arf = nullptr;
	delete [] ArfBuf;
	ArfBuf = nullptr;
	ArfSize = 0;
	return 0;
}


static inline int SetXScale(lua_State *L) { xscale = luaL_checknumber(L, 1); return 0; }
static inline int SetYScale(lua_State *L) { yscale = luaL_checknumber(L, 1); return 0; }
static inline int SetXDelta(lua_State *L) { xdelta = luaL_checknumber(L, 1); return 0; }
static inline int SetYDelta(lua_State *L) { ydelta = luaL_checknumber(L, 1); return 0; }
static inline int SetRotDeg(lua_State *L) { rotdeg = luaL_checknumber(L, 1); return 0; }
static inline int NewTable(lua_State *L) {
	DM_LUA_STACK_CHECK(L, 1);
	lua_createtable( L, (int)luaL_checknumber(L,1), (int)luaL_checknumber(L, 2) );
	return 1;
}


// Defold Lifecycle Related Stuff
static const luaL_reg M[] =
{
	{"InitArf", InitArf}, {"UpdateArf", UpdateArf}, {"JudgeArf", JudgeArf}, {"FinalArf", FinalArf},
	{"SetXScale", SetXScale}, {"SetYScale", SetYScale}, {"SetXDelta", SetXDelta}, {"SetYDelta", SetYDelta}, {"SetRotDeg", SetRotDeg},
	{"NewTable", NewTable},
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