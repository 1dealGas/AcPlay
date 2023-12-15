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
static unsigned char* ArfBuf = nullptr;
static float xscale, yscale, xdelta, ydelta, rotsin, rotcos;

// Internal Globals
static float SIN, COS;
static uint16_t special_hint, dt_p1, dt_p2;
static std::map<uint32_t, uint8_t> last_vec;
static std::vector<uint32_t> blnums;

// Caches
extern const float DSIN[901]; extern const float DCOS[901];
extern const float ESIN[1001]; extern const float ECOS[1001];
extern const double RCP[8192];


// Assistant Ease Functions
static inline float Quad(float ratio, const int16_t sgn) {
	if( sgn < 0 ) {
		ratio = 1.0f - ratio;
		return ( 1.0f - ratio*ratio );
	}
	else return ratio*ratio ;
}
static inline uint16_t mod_degree( uint64_t deg ) {
	do {
		if(deg > 7200) {
			if(deg > 14400) deg-=14400;
			else deg-=7200;
		}
		else deg-=3600;
	}	while(deg > 3600);
	return deg;
}
static inline void GetSINCOS(const double degree) {
	if( degree >= 0 ) {
		uint64_t deg = (uint64_t)(degree*10.0);			deg = (deg>3600) ? mod_degree(deg) : deg ;
		if(deg > 1800) {
			if(deg > 2700)	{ SIN = -DSIN[3600-deg];	COS = DCOS[3600-deg];  }
			else 			{ SIN = -DSIN[deg-1800];	COS = -DCOS[deg-1800]; }
		}
		else {
			if(deg > 900)	{ SIN = DSIN[1800-deg];		COS = -DCOS[1800-deg]; }
			else			{ SIN = DSIN[deg]; 			COS = DCOS[deg];	   }
		}
	}
	else {   // sin(-x) = -sin(x), cos(-x) = cos(x)
		uint64_t deg = (uint64_t)(-degree*10.0);		deg = (deg>3600) ? mod_degree(deg) : deg ;
		if(deg > 1800) {
			if(deg > 2700)	{ SIN = DSIN[3600-deg];		COS = DCOS[3600-deg];  }
			else 			{ SIN = DSIN[deg-1800];		COS = -DCOS[deg-1800]; }
		}
		else {
			if(deg > 900)	{ SIN = -DSIN[1800-deg];	COS = -DCOS[1800-deg]; }
			else			{ SIN = -DSIN[deg]; 		COS = DCOS[deg];	   }
		}
	}
}


// Input Functions
typedef dmVMath::Vector3* v3p;
static inline bool has_touch_near( const uint64_t hint, const v3p touches[10] ) {

	// Hint: (1)TAG+(19)judged_ms+(19)ms+(12)y+(13)x
	// x:[0,48]   visible -> [16,32]
	// y:[0,24]   visible -> [8,16]

	uint64_t u;
	u = hint & 0x1fff;			float dx = ( (int16_t)u - 3072 ) * 0.0078125f * xscale;
	u = (hint>>13) & 0xfff;		float dy = ( (int16_t)u - 1536 ) * 0.0078125f * yscale;

	// Camera transformation integrated.
	float posx = 8.0f + dx*rotcos - dy*rotsin + xdelta;
	float posy = 4.0f + dx*rotsin + dy*rotcos + ydelta;

	float hint_l = posx * 112.5f - 168.75f;		float hint_r = hint_l + 337.5f;
	float hint_d = posy * 112.5f - 78.75f;		float hint_u = hint_d + 337.5f;

	// For each finger,
	for( uint8_t i=0; i<10; i++ ){

		v3p f = touches[i];
		uint8_t fs = (uint8_t)( f->getZ() );

		// if the finger is just pressed, or onscreen,
		if ( fs==1 || fs==2 ){

			// check its x pivot
			float x = f->getX();
			if( (x>=hint_l) && x<=hint_r ) {

				// if no problem with pos_x of the touch, check its y pivot.
				// if no problem with pos_y of the touch, return true.
				float y = f->getY();
				if ( (y>=hint_d) && y<=hint_u )  return true;

			}
		}
	}

	// if the table does contain a valid touch,
	return false;

}

static inline bool is_safe_to_anmitsu( const uint64_t hint ){

	uint64_t u;					// No overflowing risk here.
	u = hint & 0x1fff;			int16_t hint_l = (int16_t)u - 384;		int16_t hint_r = (int16_t)u + 384;
	u = (hint>>13) & 0xfff;		int16_t hint_d = (int16_t)u - 384;		int16_t hint_u = (int16_t)u + 384;

	// Safe when blnums.size==0. The circulation is to be skipped then.
	// For registered Hints,
	uint32_t blnum;				uint16_t bs = blnums.size();
	for( uint16_t i=0; i<bs; i++ ){

		// check its x pivot,
		blnum = blnums[i];		u = blnum & 0x1fff;
		if( (u>hint_l) && u<hint_r ){

			// if problem happens with the x pivot,
			// check its y pivot, and if problem happens with the y pivot, return false.
			u = ( blnum >> 13 ) & 0xfff;
			if( (u>hint_d) && u<hint_u ) return false;

		}
	}

	// If the Hint is safe to "Anmitsu", register it, and return true.
	u = hint & 0x1ffffff;
	blnums.push_back( (uint32_t)u );
	return true;
	
}

enum { HINT_NONJUDGED_NONLIT = 0, HINT_NONJUDGED_LIT = 1,
       HINT_JUDGED = 10, HINT_JUDGED_LIT, HINT_SWEEPED    };
static inline uint8_t HStatus(uint64_t Hint){
	Hint >>= 44;
	bool TAG = (bool)(Hint >> 19);
	Hint &= 0x7ffff;
	if( Hint==1 ) 		return HINT_SWEEPED;
	else if(Hint) {
		if(TAG)			return HINT_JUDGED_LIT;
		else			return HINT_JUDGED;
	}
	else{
		if(TAG) 		return HINT_NONJUDGED_LIT;
		else			return HINT_NONJUDGED_NONLIT;
	}
}



/* Script APIs, Under Construction
   S --> Safety guaranteed, by precluding the memory leakage.  */
static Arf2* Arf = nullptr;
#define S if(!ArfSize) return 0;

// InitArf(str) -> before, total_hints, wgo_required, hgo_required
// Recommended Usage:
//     local b,t,w,h = InitArf( sys.load_resource( "Arf/1011.ar" ) )
//     collectgarbage()
static inline int InitArf(lua_State *L)
{
	// Set Global Variables
	xscale = 1.0;		yscale = 1.0;	xdelta = 0.0;	ydelta = 0.0;
	SIN = 0.0f;			COS = 0.0f;		rotsin = 0.0f;	rotcos = 1.0;
	special_hint = 0;	dt_p1 = 0;  	dt_p2 = 0;

	// Ensure a clean Initialization
	last_vec.clear();
	blnums.clear();

	// For Defold hasn't exposed something like dmResource::LoadResource(),
	// there we copy the Lua String returned by sys.load_resource() to acquire the mutable buffer.
	const char* B = luaL_checklstring(L, 1, &ArfSize);
	S ArfBuf = (unsigned char*) malloc(ArfSize);
	memcpy(ArfBuf, B, ArfSize);

	// Do API Stuff
	DM_LUA_STACK_CHECK(L, 4);
	Arf = GetMutableArf2( ArfBuf );
	special_hint = Arf->special_hint();
	lua_pushnumber( L, Arf->before() );
	lua_pushnumber( L, Arf->total_hints() );
	lua_pushnumber( L, Arf->wgo_required() );
	lua_pushnumber( L, Arf->hgo_required() );
	return 4;
}

// UpdateArf(mstime, table_w/wi/h/hi/ht/a/ai/at) -> hint_lost, wgo/hgo/ago_used
static inline int UpdateArf(lua_State *L)
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

// JudgeArf(mstime, idelta, table_touch) -> hint_hit, hint_lost, special_hint_judged
static inline int JudgeArf(lua_State *L)
{S
	// table_touch = { any_pressed, any_released, v(x,y,s)··· }
	// s0->invalid, s1->pressed, s2->onscreen, s3->released
	// fixed to contain 12 elements
	double mstime = luaL_checknumber(L, 1);
	int8_t min_dt = -37 + (int8_t)luaL_checknumber(L, 2);
	int8_t max_dt = min_dt + 74;

	lua_pushinteger(L, 1);								// ··t  ->  1
	lua_gettable(L, 3);									// ··t  ->  1  ->  ap
	lua_pushinteger(L, 2);								// ··t  ->  1  ->  ap  ->  2
	lua_gettable(L, 3);									// ··t  ->  1  ->  ap  ->  2  ->  ar

	bool any_pressed = lua_toboolean(L, 5);
	if( lua_toboolean(L, 7) ) blnums.clear();			// Discard blocking conditions if any_released.
	lua_pop(L, 4);										// ··t

	v3p touches[10];
	for( lua_Integer i=3; i<13; i++ ) {
		//	                                            // ··t
		lua_pushinteger(L, i);                          // ··t  ->  i
		lua_gettable(L, 3);                             // ··t  ->  i  ->  t[i]
		touches[i-3] = dmScript::CheckVector3(L, 5);
		lua_pop(L, 2);                                  // ··t
	}
	lua_pop(L, 3);										//


	bool special_hint_judged;
	lua_Number hint_hit, hint_lost;

	// Acquired mstime, min/max dt, any pressed, touches[10] from Lua.
	// Now we acquire some stuff from C++ logics.
	auto hint = Arf -> mutable_hint();			auto idx_size = Arf -> index() -> size();
	uint64_t _group = (uint64_t)mstime >> 9;	uint16_t which_group = _group>1 ? _group-1 : 0 ;
			 _group = which_group + 3;			uint16_t byd1_group = _group<idx_size ? _group : idx_size;

	if(any_pressed){

		uint32_t min_time = 0;
		for(; which_group<byd1_group; which_group++) {

			auto current_hint_ids = Arf -> index() -> Get( which_group ) -> hidx();
			auto how_many_hints = current_hint_ids->size();

			for(uint16_t i=0; i<how_many_hints; i++){

				auto current_hint_id = current_hint_ids->Get(i);
				auto current_hint = hint->Get( current_hint_id );

				// Acquire the status of current Hint.
				uint64_t hint_time = (current_hint>>25) & 0x7ffff;
				double dt = mstime - (double)hint_time;
				/// Asserted to be sorted.
				if(dt <- 510.0f) break;
				if(dt >  470.0f) continue;
				///
				bool htn = has_touch_near(current_hint, touches);
				uint8_t ch_status = HStatus(current_hint);

				// For non-judged hints,
				if( ch_status<10 ) {   // HINT_NONJUDGED
					if(htn) {   // if we have touch(es) near the Hint,
						if( dt>=-100.0f && dt<=100.0f ) {   // we try to judge the Hint if dt[-100ms,100ms].
							bool checker_true = is_safe_to_anmitsu(current_hint);

							// for earliest Hint(s) valid in this touch_press,
							if( !min_time || min_time==hint_time ){

								min_time = hint_time;
								if( dt>=min_dt && dt<=max_dt )	hint_hit += 1;
								else							hint_lost += 1;

								uint64_t original_hint = current_hint & 0xfffffffffff;
								hint -> Mutate(current_hint_id, ((uint64_t)mstime)<<44 +
								original_hint + 0x8000000000000000 );

								if( current_hint_id == special_hint )
								special_hint_judged = (bool)special_hint;
							}
							else if( checker_true ){   // for other Hint(s) valid and safe_to_anmitsu,

								if( dt>=min_dt && dt<=max_dt )	hint_hit += 1;
								else							hint_lost += 1;

								uint64_t original_hint = current_hint & 0xfffffffffff;
								hint -> Mutate(current_hint_id, ((uint64_t)mstime)<<44 +
								original_hint + 0x8000000000000000 );

								if( current_hint_id == special_hint )
								special_hint_judged = (bool)special_hint;
							}
							// for Hints unsuitable to judge, just switch it into HINT_NONJUDGED_LIT.
							else hint -> Mutate( current_hint_id,
							(current_hint<<1) >> 1 + 0x8000000000000000 );
						}
						// for Hints out of judging range, just switch it into HINT_NONJUDGED_LIT.
						else hint -> Mutate( current_hint_id,
						(current_hint<<1) >> 1 + 0x8000000000000000 );
					}
					else hint -> Mutate( current_hint_id, (current_hint<<1) >> 1 );
				}

				// For judged hints,
				else if ( ch_status==HINT_JUDGED_LIT && (!htn) ){
					hint -> Mutate( current_hint_id, (current_hint<<1) >> 1 );
				}
			}
		}
	}
	else {
		for(; which_group<byd1_group; which_group++) {

			auto current_hint_ids = Arf -> index() -> Get( which_group ) -> hidx();
			auto how_many_hints = current_hint_ids->size();

			for(uint16_t i=0; i<how_many_hints; i++){

				auto current_hint_id = current_hint_ids->Get(i);
				auto current_hint = hint->Get( current_hint_id );

				// Acquire the status of current Hint.
				uint64_t hint_time = (current_hint>>25) & 0x7ffff;
				double dt = mstime - (double)hint_time;
				/// Asserted to be sorted.
				if(dt <- 510.0f) break;
				if(dt > 470.0f) continue;
				///
				bool htn = has_touch_near(current_hint, touches);
				uint8_t ch_status = HStatus(current_hint);

				if( ch_status<10 ) {   // HINT_NONJUDGED
					if(htn) hint -> Mutate( current_hint_id, (current_hint<<1) >> 1 + 0x8000000000000000 );
					else	hint -> Mutate( current_hint_id, (current_hint<<1) >> 1 );
				}
				else if ( ch_status==HINT_JUDGED_LIT && (!htn) ){
					hint -> Mutate( current_hint_id, (current_hint<<1) >> 1 );
				}
			}
		}
	}

	DM_LUA_STACK_CHECK(L, 3);
	lua_pushnumber(L, hint_hit);
	lua_pushnumber(L, hint_lost);
	lua_pushboolean(L, special_hint_judged);
	return 3;

}


static inline int FinalArf(lua_State *L)
{S
	Arf = nullptr;
	free(ArfBuf);
	ArfBuf = nullptr;
	ArfSize = 0;
	return 0;
}


static inline int SetXScale(lua_State *L) { xscale = luaL_checknumber(L, 1); return 0; }
static inline int SetYScale(lua_State *L) { yscale = luaL_checknumber(L, 1); return 0; }
static inline int SetXDelta(lua_State *L) { xdelta = luaL_checknumber(L, 1); return 0; }
static inline int SetYDelta(lua_State *L) { ydelta = luaL_checknumber(L, 1); return 0; }
static inline int SetRotDeg(lua_State *L) {
	GetSINCOS( luaL_checknumber(L, 1) );
	rotsin = SIN;	rotcos = COS;
	return 0;
}
static inline int NewTable(lua_State *L) {
	DM_LUA_STACK_CHECK(L, 1);
	lua_createtable( L, (uint32_t)luaL_checknumber(L,1), (uint32_t)luaL_checknumber(L, 2) );
	return 1;
}


// Defold Lifecycle Related Stuff
static const luaL_reg M[] =
{
	{"InitArf", InitArf}, {"UpdateArf", UpdateArf}, {"JudgeArf", JudgeArf}, {"FinalArf", FinalArf},
	{"SetXScale", SetXScale}, {"SetYScale", SetYScale},
	{"SetXDelta", SetXDelta}, {"SetYDelta", SetYDelta},
	{"SetRotDeg", SetRotDeg},
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