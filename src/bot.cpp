#include <extdll.h>
#include <meta_api.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bot.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const float D2R = (float)(M_PI / 180.0);
static const float R2D = (float)(180.0 / M_PI);
static const int MAX_BOTS = 32;

static const char *Str(int offset) { return (const char *)(gpGlobals->pStringBase + (unsigned int)offset); }
static float Rnd(float lo, float hi) { return g_engfuncs.pfnRandomFloat(lo, hi); }
static float Clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static float NormAngle(float a)
{
	while (a > 180.0f) a -= 360.0f;
	while (a < -180.0f) a += 360.0f;
	return a;
}

// ---------------------------------------------------------------------------
// Weapons
// ---------------------------------------------------------------------------
struct WeaponInfo
{
	int id;                // bit index in pev->weapons
	const char *classname; // also the "use" command argument
	const char *viewmodel; // substring of pev->viewmodel while deployed
	float score[3];        // preference at near / medium / far range
	float projSpeed;       // 0 for hitscan
};

static const WeaponInfo g_weapons[] =
{
	{ 1,  "weapon_crowbar",     "v_crowbar",     { 1, 0, 0 }, 0 },
	{ 2,  "weapon_9mmhandgun",  "v_9mmhandgun",  { 2, 2, 3 }, 0 },
	{ 3,  "weapon_357",         "v_357",         { 5, 5, 6 }, 0 },
	{ 4,  "weapon_9mmAR",       "v_9mmAR",       { 6, 6, 4 }, 0 },
	{ 6,  "weapon_crossbow",    "v_crossbow",    { 3, 5, 8 }, 2000 },
	{ 7,  "weapon_shotgun",     "v_shotgun",     { 8, 5, 1 }, 0 },
	{ 8,  "weapon_rpg",         "v_rpg",         { 0, 6, 7 }, 1500 },
	{ 9,  "weapon_gauss",       "v_gauss",       { 4, 7, 7 }, 0 },
	{ 10, "weapon_egon",        "v_egon",        { 7, 8, 5 }, 0 },
	{ 11, "weapon_hornetgun",   "v_hgun",        { 2, 2, 2 }, 0 },
};
static const int NUM_WEAPONS = sizeof(g_weapons) / sizeof(g_weapons[0]);

static int CurrentWeapon(edict_t *e)
{
	const char *vm = Str(e->v.viewmodel);
	for (int i = 0; i < NUM_WEAPONS; i++)
		if (strstr(vm, g_weapons[i].viewmodel))
			return i;
	return -1;
}

static int WeaponIndexForClass(const char *cls)
{
	for (int i = 0; i < NUM_WEAPONS; i++)
		if (!strcmp(cls, g_weapons[i].classname))
			return i;
	return -1;
}

// ---------------------------------------------------------------------------
// Bot state
// ---------------------------------------------------------------------------
struct Bot
{
	bool used;
	edict_t *ent;
	int skill;

	float yaw, pitch;       // current view angles (engine convention: +pitch = down)
	float lastRun;
	bool wasDead;

	edict_t *enemy;
	float enemyFirstSeen;
	float enemyLastSeen;
	Vector enemyLastPos;
	Vector aimOffset;
	float aimOffsetTime;
	float strafeDir, strafeChange;
	float nextAlt;
	float nextJump;

	edict_t *goal;
	float goalSince;
	float nextItemScan;
	edict_t *ignoreEnt[6];
	float ignoreUntil[6];

	float moveYaw, moveCommit;
	float wanderYaw, wanderChange;
	Vector stuckPos;
	float stuckCheck;
	float unstuckUntil;
	float unstuckYaw;

	float weaponCooldown[16];
	int pendingWeapon;
	float pendingTime;
	float nextWeaponCheck;
	float nextReload;
	float travelled;
	Vector lastDbgPos;
	float nextDbg;
	int stuckCount;
	float deadToggle;
};

static Bot g_bots[MAX_BOTS];
static float g_nextQuotaCheck = 0;
static float g_activateTime = 0;
static char g_cvQuotaName[] = "crowbot_quota";
static char g_cvSkillName[] = "crowbot_skill";
static char g_cvZero[] = "0";
static char g_cvThree[] = "3";
static cvar_t g_cvQuota = { g_cvQuotaName, g_cvZero, 0, 0.0f, NULL };
static cvar_t g_cvSkill = { g_cvSkillName, g_cvThree, 0, 3.0f, NULL };
static char g_cvDebugName[] = "crowbot_debug";
static cvar_t g_cvDebug = { g_cvDebugName, g_cvZero, 0, 0.0f, NULL };

static const char *g_botNames[] =
{
	"Barney", "Gordon", "Gina", "Adrian", "Otis", "Kleiner", "Eli", "Vance",
	"Grunt", "Recon", "Tau", "Xen", "Lambda", "Headcrab", "Vortigaunt", "Gonarch",
};

static const char *g_dmModels[] = { "barney", "gina", "gordon", "helmet", "hgrunt", "recon", "robo", "scientist" };

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static bool IsPlayerValid(edict_t *e)
{
	return e && !e->free && (e->v.flags & (FL_CLIENT | FL_FAKECLIENT)) && Str(e->v.netname)[0];
}

static bool IsAlive(edict_t *e)
{
	return e->v.deadflag == DEAD_NO && e->v.health > 0 && e->v.solid != SOLID_NOT;
}

static Vector EyePos(edict_t *e) { return e->v.origin + e->v.view_ofs; }

static float Dist2D(const Vector &a, const Vector &b)
{
	float dx = a.x - b.x, dy = a.y - b.y;
	return sqrtf(dx * dx + dy * dy);
}

static float YawTo(const Vector &from, const Vector &to)
{
	return atan2f(to.y - from.y, to.x - from.x) * R2D;
}

static bool TeamPlay() { return g_engfuncs.pfnCVarGetFloat("mp_teamplay") != 0.0f; }

static const char *ModelOf(edict_t *e)
{
	return g_engfuncs.pfnInfoKeyValue(g_engfuncs.pfnGetInfoKeyBuffer(e), "model");
}

static bool SameTeam(edict_t *a, edict_t *b)
{
	return TeamPlay() && !_stricmp(ModelOf(a), ModelOf(b));
}

static Bot *FindBot(edict_t *e)
{
	for (int i = 0; i < MAX_BOTS; i++)
		if (g_bots[i].used && g_bots[i].ent == e)
			return &g_bots[i];
	return NULL;
}

static int BotCount()
{
	int n = 0;
	for (int i = 0; i < MAX_BOTS; i++)
		if (g_bots[i].used) n++;
	return n;
}

// Run a client command on behalf of a fake client. The engine's Cmd_Argv family
// is overridden (see engine_api.cpp) while this is active.
static bool g_fakeActive = false;
static int g_fakeArgc = 0;
static char g_fakeBuf[256];
static char *g_fakeArgv[8];
static const char *g_fakeArgs = "";

bool Bot_FakeCmdActive() { return g_fakeActive; }
int Bot_FakeArgc() { return g_fakeArgc; }
const char *Bot_FakeArgv(int i) { return (i >= 0 && i < g_fakeArgc) ? g_fakeArgv[i] : ""; }
const char *Bot_FakeArgs() { return g_fakeArgs; }

static void FakeClientCommand(edict_t *e, const char *cmd)
{
	strncpy(g_fakeBuf, cmd, sizeof(g_fakeBuf) - 1);
	g_fakeBuf[sizeof(g_fakeBuf) - 1] = 0;
	g_fakeArgc = 0;
	char *p = g_fakeBuf;
	while (*p && g_fakeArgc < 8)
	{
		while (*p == ' ') p++;
		if (!*p) break;
		if (g_fakeArgc == 1) g_fakeArgs = p;
		g_fakeArgv[g_fakeArgc++] = p;
		while (*p && *p != ' ') p++;
		if (*p) *p++ = 0;
	}
	g_fakeActive = true;
	gpGamedllFuncs->dllapi_table->pfnClientCommand(e);
	g_fakeActive = false;
	g_fakeArgs = "";
}

// ---------------------------------------------------------------------------
// Navigation helpers
// ---------------------------------------------------------------------------
struct Probe
{
	float dist;
	bool jump;
	bool hazard;
};

// Measure how far the bot can walk along yaw, detecting jumpable obstacles and
// dangerous drops / lava.
static Probe ProbeDir(Bot &b, float yawDeg, float maxD)
{
	Probe p = { maxD, false, false };
	edict_t *e = b.ent;
	float r = yawDeg * D2R;
	Vector dir(cosf(r), sinf(r), 0);
	Vector org = e->v.origin;
	TraceResult tr;

	Vector s = org + Vector(0, 0, 18);
	g_engfuncs.pfnTraceHull(s, s + dir * maxD, ignore_monsters, human_hull, e, &tr);
	if (tr.fAllSolid || tr.fStartSolid)
		p.dist = 0;
	else
		p.dist = tr.flFraction * maxD;

	if (p.dist < 48.0f)
	{
		Vector s2 = org + Vector(0, 0, 45);
		g_engfuncs.pfnTraceHull(s2, s2 + dir * 64.0f, ignore_monsters, human_hull, e, &tr);
		if (!tr.fAllSolid && !tr.fStartSolid && tr.flFraction >= 1.0f)
			p.jump = true;
	}

	float lookAhead = p.dist < 56.0f ? p.dist : 56.0f;
	if (lookAhead > 8.0f)
	{
		Vector pt = org + dir * lookAhead;
		g_engfuncs.pfnTraceLine(pt, pt - Vector(0, 0, 36 + 130), ignore_monsters, e, &tr);
		if (tr.flFraction >= 1.0f)
			p.hazard = true;
		else
		{
			int c = g_engfuncs.pfnPointContents(tr.vecEndPos + Vector(0, 0, 2));
			if (c == CONTENTS_LAVA || c == CONTENTS_SLIME)
				p.hazard = true;
		}
	}
	if (p.hazard)
		p.dist = 0;
	return p;
}

// Choose a walking heading near wantYaw that is not blocked. Returns true if the
// chosen heading needs a jump.
static bool PickHeading(Bot &b, float wantYaw, bool goalDriven)
{
	float now = gpGlobals->time;
	Probe cur = ProbeDir(b, b.moveYaw, 120.0f);
	bool blocked = cur.dist < 50.0f && !cur.jump;

	if (now >= b.moveCommit || blocked)
	{
		static const float offs[] = { 0, 25, -25, 50, -50, 80, -80, 115, -115, 150, -150, 180 };
		float best = -1e9f;
		float bestYaw = wantYaw;
		for (int i = 0; i < (int)(sizeof(offs) / sizeof(offs[0])); i++)
		{
			float y = NormAngle(wantYaw + offs[i]);
			Probe p = ProbeDir(b, y, 250.0f);
			float score = p.dist + (p.jump ? 60.0f : 0.0f) - fabsf(offs[i]) * (goalDriven ? 1.2f : 0.35f);
			if (p.hazard) score -= 1000.0f;
			if (score > best)
			{
				best = score;
				bestYaw = y;
			}
		}
		b.moveYaw = bestYaw;
		b.moveCommit = now + (goalDriven ? Rnd(0.2f, 0.5f) : Rnd(0.5f, 1.2f));
		cur = ProbeDir(b, b.moveYaw, 120.0f);
	}
	return cur.jump && cur.dist < 50.0f;
}

// ---------------------------------------------------------------------------
// Enemy / item selection
// ---------------------------------------------------------------------------
static bool CanSee(edict_t *from, edict_t *target, Vector *aimPoint)
{
	Vector eye = EyePos(from);
	TraceResult tr;
	Vector body = target->v.origin;
	g_engfuncs.pfnTraceLine(eye, body, ignore_monsters, from, &tr);
	if (tr.flFraction >= 1.0f || tr.pHit == target)
	{
		if (aimPoint) *aimPoint = body;
		return true;
	}
	Vector head = EyePos(target);
	g_engfuncs.pfnTraceLine(eye, head, ignore_monsters, from, &tr);
	if (tr.flFraction >= 1.0f || tr.pHit == target)
	{
		if (aimPoint) *aimPoint = head;
		return true;
	}
	return false;
}

static bool IsTargetable(Bot &b, edict_t *e)
{
	return e != b.ent && IsPlayerValid(e) && IsAlive(e) && !(e->v.flags & FL_NOTARGET) && !SameTeam(b.ent, e);
}

static void FindEnemy(Bot &b)
{
	float now = gpGlobals->time;
	edict_t *me = b.ent;

	if (b.enemy)
	{
		Vector p;
		if (IsTargetable(b, b.enemy) && CanSee(me, b.enemy, &p))
		{
			b.enemyLastSeen = now;
			b.enemyLastPos = b.enemy->v.origin;
			return;
		}
		if (!IsTargetable(b, b.enemy) || now - b.enemyLastSeen > 0.4f)
			b.enemy = NULL;
		else
			return;
	}

	edict_t *best = NULL;
	float bestDist = 1e9f;
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		edict_t *e = g_engfuncs.pfnPEntityOfEntIndex(i);
		if (!e || !IsTargetable(b, e))
			continue;
		float d = (e->v.origin - me->v.origin).Length();
		if (d >= bestDist)
			continue;
		float dyaw = fabsf(NormAngle(YawTo(me->v.origin, e->v.origin) - b.yaw));
		if (dyaw > 80.0f && d > 350.0f)
			continue;
		if (!CanSee(me, e, NULL))
			continue;
		best = e;
		bestDist = d;
	}
	if (best)
	{
		b.enemy = best;
		b.enemyFirstSeen = now;
		b.enemyLastSeen = now;
		b.enemyLastPos = best->v.origin;
	}
}

static bool Ignored(Bot &b, edict_t *e)
{
	float now = gpGlobals->time;
	for (int i = 0; i < 6; i++)
		if (b.ignoreEnt[i] == e && b.ignoreUntil[i] > now)
			return true;
	return false;
}

static void IgnoreEnt(Bot &b, edict_t *e, float seconds)
{
	int slot = 0;
	for (int i = 0; i < 6; i++)
		if (b.ignoreUntil[i] < b.ignoreUntil[slot])
			slot = i;
	b.ignoreEnt[slot] = e;
	b.ignoreUntil[slot] = gpGlobals->time + seconds;
}

static float ItemValue(Bot &b, const char *cls)
{
	entvars_t &v = b.ent->v;
	if (!strcmp(cls, "item_healthkit"))
		return v.health < 90 ? (100.0f - v.health) : 0;
	if (!strcmp(cls, "item_battery"))
		return v.armorvalue < 90 ? (100.0f - v.armorvalue) * 0.6f : 0;
	if (!strcmp(cls, "item_longjump"))
		return 10;
	if (!strncmp(cls, "weapon_", 7))
	{
		int w = WeaponIndexForClass(cls);
		if (w >= 0 && !(v.weapons & (1 << g_weapons[w].id)))
			return 80;
		return 25;
	}
	if (!strncmp(cls, "ammo_", 5))
		return 30;
	if (!strcmp(cls, "weaponbox"))
		return 50;
	return 0;
}

static void FindGoal(Bot &b)
{
	float now = gpGlobals->time;
	if (b.goal)
	{
		edict_t *g = b.goal;
		bool gone = g->free || (g->v.effects & EF_NODRAW) || g->v.solid != SOLID_TRIGGER;
		Vector c = (g->v.absmin + g->v.absmax) * 0.5f;
		if (gone || (b.ent->v.origin - c).Length() < 40.0f)
			b.goal = NULL;
		else if (now - b.goalSince > 7.0f)
		{
			IgnoreEnt(b, g, 25.0f);
			b.goal = NULL;
		}
	}
	if (b.goal || now < b.nextItemScan)
		return;
	b.nextItemScan = now + Rnd(0.6f, 1.0f);

	Vector eye = EyePos(b.ent);
	edict_t *best = NULL;
	float bestScore = 0;
	for (int i = gpGlobals->maxClients + 1; i < gpGlobals->maxEntities; i++)
	{
		edict_t *e = g_engfuncs.pfnPEntityOfEntIndex(i);
		if (!e || e->free || !e->v.classname)
			continue;
		if (e->v.effects & EF_NODRAW || e->v.solid != SOLID_TRIGGER || e->v.owner)
			continue;
		const char *cls = Str(e->v.classname);
		if (strncmp(cls, "weapon", 6) && strncmp(cls, "ammo_", 5) && strncmp(cls, "item_", 5))
			continue;
		float value = ItemValue(b, cls);
		if (value <= 0 || Ignored(b, e))
			continue;
		Vector c = (e->v.absmin + e->v.absmax) * 0.5f;
		float d = (c - eye).Length();
		if (d > 1800.0f)
			continue;
		float score = value - d * 0.04f;
		if (score <= bestScore)
			continue;
		TraceResult tr;
		g_engfuncs.pfnTraceLine(eye, c, ignore_monsters, b.ent, &tr);
		if (tr.flFraction < 0.98f && tr.pHit != e)
			continue;
		best = e;
		bestScore = score;
	}
	if (best)
	{
		b.goal = best;
		b.goalSince = now;
	}
}

// ---------------------------------------------------------------------------
// Weapon handling
// ---------------------------------------------------------------------------
static void ManageWeapon(Bot &b, float enemyDist)
{
	float now = gpGlobals->time;
	edict_t *e = b.ent;
	int cur = CurrentWeapon(e);

	if (b.pendingWeapon >= 0 && now > b.pendingTime)
	{
		if (cur != b.pendingWeapon)
			b.weaponCooldown[b.pendingWeapon] = now + 6.0f;
		b.pendingWeapon = -1;
	}
	if (now < b.nextWeaponCheck || b.pendingWeapon >= 0)
		return;
	b.nextWeaponCheck = now + 0.5f;

	int bucket = enemyDist < 200 ? 0 : (enemyDist < 700 ? 1 : 2);
	int best = -1;
	float bestScore = -1;
	for (int i = 0; i < NUM_WEAPONS; i++)
	{
		if (!(e->v.weapons & (1 << g_weapons[i].id)) || b.weaponCooldown[i] > now)
			continue;
		float s = g_weapons[i].score[bucket] + (i == cur ? 1.5f : 0.0f);
		if (s > bestScore)
		{
			bestScore = s;
			best = i;
		}
	}
	if (best >= 0 && best != cur && bestScore > 0)
	{
		char cmd[64];
		_snprintf(cmd, sizeof(cmd), "use %s", g_weapons[best].classname);
		FakeClientCommand(e, cmd);
		b.pendingWeapon = best;
		b.pendingTime = now + 0.4f;
	}
}

// ---------------------------------------------------------------------------
// Per-bot think
// ---------------------------------------------------------------------------
static void ResetBotState(Bot &b)
{
	b.enemy = NULL;
	b.goal = NULL;
	b.moveCommit = 0;
	b.wanderChange = 0;
	b.unstuckUntil = 0;
	b.pendingWeapon = -1;
	b.nextWeaponCheck = 0;
	memset(b.weaponCooldown, 0, sizeof(b.weaponCooldown));
	b.stuckCheck = 0;
}

static void BotThink(Bot &b)
{
	edict_t *e = b.ent;
	float now = gpGlobals->time;
	float dt = now - b.lastRun;
	if (dt <= 0) dt = 0.001f;
	if (dt > 0.1f) dt = 0.1f;
	byte msec = (byte)Clampf(dt * 1000.0f, 1.0f, 100.0f);
	b.lastRun = now;

	float forward = 0, side = 0, up = 0;
	unsigned short buttons = 0;
	float idealYaw = b.yaw, idealPitch = 0;
	float maxSpeed = g_engfuncs.pfnCVarGetFloat("sv_maxspeed");
	if (maxSpeed <= 0) maxSpeed = 320.0f;

	if (e->v.deadflag != DEAD_NO || e->v.health <= 0)
	{
		b.wasDead = true;
		if (now > b.deadToggle)
		{
			b.deadToggle = now + 0.15f;
			buttons = (rand() & 1) ? IN_ATTACK : 0;
		}
		g_engfuncs.pfnRunPlayerMove(e, Vector(b.pitch, b.yaw, 0), 0, 0, 0, buttons, 0, msec);
		return;
	}
	if (b.wasDead)
	{
		ResetBotState(b);
		b.wasDead = false;
		b.yaw = e->v.v_angle.y;
		b.pitch = 0;
	}

	float skillF = (float)b.skill;
	FindEnemy(b);

	Vector myEye = EyePos(e);
	bool wantJump = false;
	bool combat = false;
	float turnRate = 4.0f + skillF * 2.5f;

	if (b.enemy)
	{
		combat = true;
		Vector aimPt;
		if (!CanSee(e, b.enemy, &aimPt))
			aimPt = b.enemyLastPos;
		float dist = (b.enemy->v.origin - e->v.origin).Length();
		ManageWeapon(b, dist);

		int cur = CurrentWeapon(e);
		float lead = 0.0f;
		if (cur >= 0 && g_weapons[cur].projSpeed > 0)
			lead = dist / g_weapons[cur].projSpeed;
		aimPt = aimPt + b.enemy->v.velocity * lead;

		if (now > b.aimOffsetTime)
		{
			float spread = (6.0f - skillF) * 4.0f;
			b.aimOffset = Vector(Rnd(-spread, spread), Rnd(-spread, spread), Rnd(-spread, spread) * 0.6f);
			b.aimOffsetTime = now + Rnd(0.2f, 0.5f);
		}
		aimPt = aimPt + b.aimOffset;

		Vector d = aimPt - myEye;
		idealYaw = atan2f(d.y, d.x) * R2D;
		idealPitch = -atan2f(d.z, sqrtf(d.x * d.x + d.y * d.y)) * R2D;

		float yawErr = fabsf(NormAngle(idealYaw - b.yaw));
		float pitchErr = fabsf(idealPitch - b.pitch);
		float tol = Clampf(atanf(30.0f / (dist + 1.0f)) * R2D, 3.0f, 14.0f);
		float react = 0.45f - skillF * 0.07f;
		bool visible = (now - b.enemyLastSeen) < 0.15f;
		if (visible && now - b.enemyFirstSeen > react && yawErr < tol && pitchErr < tol && !(cur == 6 && dist < 250.0f))
		{
			if (cur != 0 || dist < 80.0f)
				buttons |= IN_ATTACK;
			if (cur == 3 && dist > 350 && dist < 900 && now > b.nextAlt && Rnd(0, 1) < 0.3f)
			{
				buttons = IN_ATTACK2;
				b.nextAlt = now + 4.0f;
			}
		}

		// strafe and keep a sensible range
		if (now > b.strafeChange)
		{
			b.strafeDir = (Rnd(0, 1) < 0.5f) ? -1.0f : 1.0f;
			b.strafeChange = now + Rnd(0.5f, 1.6f);
		}
		float sideYaw = b.yaw + 90.0f * b.strafeDir * -1.0f;
		Probe sp = ProbeDir(b, sideYaw, 80.0f);
		if (sp.dist < 40.0f || sp.hazard)
		{
			b.strafeDir = -b.strafeDir;
			b.strafeChange = now + Rnd(0.5f, 1.0f);
		}
		side = b.strafeDir * maxSpeed * (0.5f + 0.1f * skillF);
		bool closeRange = (cur == 5 || cur == 0 || cur == 8);
		if (closeRange && dist > 120.0f)
			forward = maxSpeed;
		else if (!closeRange && dist < 220.0f)
			forward = -maxSpeed * 0.6f;
		else if (dist > 750.0f)
			forward = maxSpeed;

		Probe fp = ProbeDir(b, b.yaw + (forward < 0 ? 180.0f : 0.0f), 70.0f);
		if (forward != 0 && (fp.dist < 40.0f || fp.hazard))
			forward = 0;

		if (now > b.nextJump && Rnd(0, 1) < 0.015f * (1.0f + skillF))
		{
			wantJump = true;
			b.nextJump = now + Rnd(1.5f, 4.0f);
		}
	}
	else
	{
		ManageWeapon(b, 400.0f);
		if (b.pendingWeapon < 0 && now > b.nextReload)
		{
			buttons |= IN_RELOAD;
			b.nextReload = now + Rnd(4.0f, 8.0f);
		}

		FindGoal(b);

		float wantYaw;
		bool goalDriven = false;
		if (now - b.enemyLastSeen < 3.0f && b.enemyLastSeen > 0)
		{
			wantYaw = YawTo(e->v.origin, b.enemyLastPos);
			goalDriven = true;
		}
		else if (b.goal)
		{
			Vector c = (b.goal->v.absmin + b.goal->v.absmax) * 0.5f;
			wantYaw = YawTo(e->v.origin, c);
			goalDriven = true;
		}
		else
		{
			if (now > b.wanderChange)
			{
				b.wanderYaw = b.moveYaw + Rnd(-70.0f, 70.0f);
				b.wanderChange = now + Rnd(1.0f, 3.0f);
			}
			wantYaw = b.wanderYaw;
		}

		if (now < b.unstuckUntil)
			wantYaw = b.unstuckYaw;

		bool jumpNeeded = PickHeading(b, wantYaw, goalDriven);
		wantJump = jumpNeeded;

		idealYaw = b.moveYaw;
		idealPitch = 0;
		float dy = fabsf(NormAngle(b.moveYaw - b.yaw));
		forward = maxSpeed * (dy > 100.0f ? 0.3f : 1.0f);
		side = 0;

		// stuck detection
		if (now > b.stuckCheck)
		{
			if (b.stuckCheck > 0 && (e->v.origin - b.stuckPos).Length() < 10.0f)
			{
				b.unstuckYaw = NormAngle(b.moveYaw + Rnd(100.0f, 260.0f));
				b.stuckCount++;
				b.unstuckUntil = now + Rnd(0.5f, 1.2f);
				b.moveCommit = 0;
				b.wanderChange = 0;
				wantJump = true;
				buttons |= IN_USE;
				if (b.goal && now - b.goalSince > 2.0f)
				{
					IgnoreEnt(b, b.goal, 20.0f);
					b.goal = NULL;
				}
			}
			b.stuckPos = e->v.origin;
			b.stuckCheck = now + 0.7f;
		}
	}

	// smooth view turning
	{
		float maxStep = (220.0f + skillF * 160.0f) * dt;
		float dy = NormAngle(idealYaw - b.yaw);
		float dp = idealPitch - b.pitch;
		b.yaw = NormAngle(b.yaw + Clampf(dy * Clampf(turnRate * dt, 0.0f, 1.0f), -maxStep, maxStep));
		b.pitch = Clampf(b.pitch + Clampf(dp * Clampf(turnRate * dt, 0.0f, 1.0f), -maxStep, maxStep), -89.0f, 89.0f);
	}

	// convert world-space walking heading to view-relative movement in non-combat mode
	if (!combat)
	{
		float rel = NormAngle(b.moveYaw - b.yaw) * D2R;
		float f = forward;
		forward = f * cosf(rel);
		side = -f * sinf(rel);
	}

	// water and ladders
	if (e->v.waterlevel >= 2)
		up = 200.0f;
	if (e->v.movetype == MOVETYPE_FLY)
	{
		forward = maxSpeed * 0.7f;
		b.pitch = -45.0f;
	}

	if (wantJump && (e->v.flags & FL_ONGROUND))
		buttons |= IN_JUMP;

	if (g_engfuncs.pfnCVarGetFloat("crowbot_debug") != 0.0f)
	{
		b.travelled += (e->v.origin - b.lastDbgPos).Length();
		b.lastDbgPos = e->v.origin;
		if (now > b.nextDbg)
		{
			char msg[200];
			_snprintf(msg, sizeof(msg), "[dbg] %s pos=(%.0f %.0f %.0f) moved=%.0f stuck=%d goal=%s enemy=%d wpn=%d\n",
				Str(e->v.netname), e->v.origin.x, e->v.origin.y, e->v.origin.z, b.travelled, b.stuckCount,
				b.goal ? Str(b.goal->v.classname) : "-", b.enemy ? 1 : 0, CurrentWeapon(e));
			g_engfuncs.pfnServerPrint(msg);
			b.travelled = 0;
			b.nextDbg = now + 5.0f;
		}
	}

	e->v.v_angle = Vector(b.pitch, b.yaw, 0);
	e->v.angles = Vector(b.pitch / -3.0f, b.yaw, 0);
	g_engfuncs.pfnRunPlayerMove(e, Vector(b.pitch, b.yaw, 0), forward, side, up, buttons, 0, msec);
}

// ---------------------------------------------------------------------------
// Creation / removal
// ---------------------------------------------------------------------------
static void PickTeamModel(char *out, size_t outSize)
{
	if (TeamPlay())
	{
		char list[256];
		strncpy(list, g_engfuncs.pfnCVarGetString("mp_teamlist"), sizeof(list) - 1);
		list[sizeof(list) - 1] = 0;
		char *teams[16];
		int counts[16];
		int n = 0;
		for (char *t = strtok(list, ";"); t && n < 16; t = strtok(NULL, ";"))
		{
			teams[n] = t;
			counts[n++] = 0;
		}
		if (n > 0)
		{
			for (int i = 1; i <= gpGlobals->maxClients; i++)
			{
				edict_t *p = g_engfuncs.pfnPEntityOfEntIndex(i);
				if (!IsPlayerValid(p)) continue;
				for (int t = 0; t < n; t++)
					if (!_stricmp(ModelOf(p), teams[t])) counts[t]++;
			}
			int best = 0;
			for (int t = 1; t < n; t++)
				if (counts[t] < counts[best]) best = t;
			strncpy(out, teams[best], outSize - 1);
			out[outSize - 1] = 0;
			return;
		}
	}
	const char *m = g_dmModels[g_engfuncs.pfnRandomLong(0, (int)(sizeof(g_dmModels) / sizeof(g_dmModels[0])) - 1)];
	strncpy(out, m, outSize - 1);
	out[outSize - 1] = 0;
}

static bool NameInUse(const char *name)
{
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		edict_t *p = g_engfuncs.pfnPEntityOfEntIndex(i);
		if (IsPlayerValid(p) && !_stricmp(Str(p->v.netname), name))
			return true;
	}
	return false;
}

static bool AddBot(int skill)
{
	int slot = -1;
	for (int i = 0; i < MAX_BOTS; i++)
		if (!g_bots[i].used) { slot = i; break; }
	if (slot < 0)
		return false;

	char name[32] = "Bot";
	int count = (int)(sizeof(g_botNames) / sizeof(g_botNames[0]));
	int start = g_engfuncs.pfnRandomLong(0, count - 1);
	for (int i = 0; i < count; i++)
	{
		const char *cand = g_botNames[(start + i) % count];
		if (!NameInUse(cand))
		{
			strncpy(name, cand, sizeof(name) - 1);
			break;
		}
	}

	edict_t *e = g_engfuncs.pfnCreateFakeClient(name);
	if (!e)
		return false;

	char *ib = g_engfuncs.pfnGetInfoKeyBuffer(e);
	int idx = g_engfuncs.pfnIndexOfEdict(e);
	char model[32];
	PickTeamModel(model, sizeof(model));
	g_engfuncs.pfnSetClientKeyValue(idx, ib, "model", model);
	g_engfuncs.pfnSetClientKeyValue(idx, ib, "rate", "3500");
	g_engfuncs.pfnSetClientKeyValue(idx, ib, "cl_updaterate", "30");
	g_engfuncs.pfnSetClientKeyValue(idx, ib, "cl_lw", "0");
	g_engfuncs.pfnSetClientKeyValue(idx, ib, "cl_lc", "0");
	g_engfuncs.pfnSetClientKeyValue(idx, ib, "topcolor", "40");
	g_engfuncs.pfnSetClientKeyValue(idx, ib, "bottomcolor", "160");

	char reject[128] = "";
	gpGamedllFuncs->dllapi_table->pfnClientConnect(e, name, "127.0.0.1", reject);
	gpGamedllFuncs->dllapi_table->pfnClientPutInServer(e);
	e->v.flags |= FL_FAKECLIENT;

	Bot &b = g_bots[slot];
	memset(&b, 0, sizeof(b));
	b.used = true;
	b.ent = e;
	b.skill = (int)Clampf((float)skill, 1.0f, 5.0f);
	b.lastRun = gpGlobals->time;
	b.moveYaw = Rnd(-180.0f, 180.0f);
	ResetBotState(b);
	return true;
}

static void KickBot(Bot &b)
{
	char cmd[32];
	_snprintf(cmd, sizeof(cmd), "kick #%d\n", g_engfuncs.pfnGetPlayerUserId(b.ent));
	g_engfuncs.pfnServerCommand(cmd);
}

// ---------------------------------------------------------------------------
// Commands / lifecycle
// ---------------------------------------------------------------------------
static void Say(const char *msg) { g_engfuncs.pfnServerPrint(msg); }

static void HandleCommand()
{
	const char *sub = g_engfuncs.pfnCmd_Argc() > 1 ? g_engfuncs.pfnCmd_Argv(1) : "";
	char buf[128];

	if (!_stricmp(sub, "add"))
	{
		int skill = g_engfuncs.pfnCmd_Argc() > 2 ? atoi(g_engfuncs.pfnCmd_Argv(2)) : 		(int)g_engfuncs.pfnCVarGetFloat("crowbot_skill");
				if (!AddBot(skill)) Say("crowbot: could not add a bot (server full?)\n");
	}
	else if (!_stricmp(sub, "kick"))
	{
		for (int i = MAX_BOTS - 1; i >= 0; i--)
			if (g_bots[i].used) { KickBot(g_bots[i]); break; }
	}
	else if (!_stricmp(sub, "kickall"))
	{
		g_engfuncs.pfnCVarSetFloat("crowbot_quota", 0);
		for (int i = 0; i < MAX_BOTS; i++)
			if (g_bots[i].used) KickBot(g_bots[i]);
	}
	else if (!_stricmp(sub, "quota") && g_engfuncs.pfnCmd_Argc() > 2)
		g_engfuncs.pfnCVarSetFloat("crowbot_quota", (float)atoi(g_engfuncs.pfnCmd_Argv(2)));
	else if (!_stricmp(sub, "skill") && g_engfuncs.pfnCmd_Argc() > 2)
		g_engfuncs.pfnCVarSetFloat("crowbot_skill", Clampf((float)atoi(g_engfuncs.pfnCmd_Argv(2)), 1.0f, 5.0f));
	else
	{
		_snprintf(buf, sizeof(buf), "crowbot: %d bot(s). Usage: crowbot add [skill] | kick | kickall | quota <n> | skill <1-5>\n", BotCount());
		Say(buf);
	}
}

static void ServerCmd() { HandleCommand(); }

bool Bot_ClientCommand(edict_t *ent)
{
	if (g_fakeActive || _stricmp(g_engfuncs.pfnCmd_Argv(0), "crowbot"))
		return false;
	// only the listen-server host may control bots from the in-game console
	if (!g_engfuncs.pfnIsDedicatedServer() && g_engfuncs.pfnIndexOfEdict(ent) == 1)
		HandleCommand();
	return true;
}

void Bot_Init()
{
	g_engfuncs.pfnCVarRegister(&g_cvQuota);
	g_engfuncs.pfnCVarRegister(&g_cvSkill);
	g_engfuncs.pfnCVarRegister(&g_cvDebug);
	g_engfuncs.pfnAddServerCommand("crowbot", ServerCmd);
}

void Bot_ServerActivate()
{
	memset(g_bots, 0, sizeof(g_bots));
	g_activateTime = gpGlobals->time;
	g_nextQuotaCheck = gpGlobals->time + 3.0f;
}

void Bot_ServerDeactivate()
{
	memset(g_bots, 0, sizeof(g_bots));
}

void Bot_ClientDisconnect(edict_t *ent)
{
	Bot *b = FindBot(ent);
	if (b) b->used = false;
}

void Bot_StartFrame()
{
	float now = gpGlobals->time;

	if (now >= g_nextQuotaCheck)
	{
		g_nextQuotaCheck = now + 1.0f;
		int quota = (int)g_engfuncs.pfnCVarGetFloat("crowbot_quota");
		int count = BotCount();
		if (count < quota)
			AddBot((int)g_engfuncs.pfnCVarGetFloat("crowbot_skill"));
		else if (count > quota)
			for (int i = MAX_BOTS - 1; i >= 0; i--)
				if (g_bots[i].used) { KickBot(g_bots[i]); break; }
	}

	for (int i = 0; i < MAX_BOTS; i++)
	{
		Bot &b = g_bots[i];
		if (!b.used)
			continue;
		if (!b.ent || b.ent->free)
		{
			b.used = false;
			continue;
		}
		// the game DLL clears this on every respawn
		b.ent->v.flags |= FL_FAKECLIENT;
		BotThink(b);
	}
}
