#include "host_detect.h"

// One packed word, updated with compare-and-swap: events arrive on the usbd
// task while hostTick() runs in loop(), and neither may overwrite the other.
#define ST_KIND(s) ((s) & 3u)
#define ST_WHY(s) (((s) >> 2) & 15u)
#define ST_STRONG (1u << 6) // set by a strong event of this session
#define ST_FRESH (1u << 7)
#define ST_PROBED (1u << 8)
#define ST_PACK(kind, why) ((uint32_t)(kind) | ((uint32_t)(why) << 2))

static volatile uint8_t s_setting = HOSTSET_AUTO;
static volatile uint32_t s_state;
static volatile uint32_t s_probeMs;

// Atomically replace the state with f(old); f must be pure.
template <typename F> static void update(F f)
{
	uint32_t old = __atomic_load_n(&s_state, __ATOMIC_RELAXED);
	while (!__atomic_compare_exchange_n(&s_state, &old, f(old), true,
					    __ATOMIC_ACQ_REL, __ATOMIC_RELAXED))
		;
}

void hostDetectInit(uint8_t setting, uint8_t remembered)
{
	s_setting = setting <= HOSTSET_MAX ? setting : HOSTSET_AUTO;
	uint32_t st = 0;
	if (remembered == HOST_PC || remembered == HOST_CONSOLE)
		st = ST_PACK(remembered, HOSTWHY_REMEMBERED);
	__atomic_store_n(&s_state, st, __ATOMIC_RELEASE);
}

static void strong(uint8_t kind, uint8_t why)
{
	update([&](uint32_t) {
		return ST_PACK(kind, why) | ST_STRONG | ST_FRESH;
	});
}

void hostEvent(enum HostEvent ev, uint32_t nowMs)
{
	switch (ev) {
	case HE_MSOS2:
		strong(HOST_PC, HOSTWHY_MSOS2);
		break;
	case HE_WEBUSB:
		strong(HOST_PC, HOSTWHY_WEBUSB);
		break;
	case HE_AUTH:
		strong(HOST_CONSOLE, HOSTWHY_AUTH);
		break;
	case HE_PROBE:
		if (!(__atomic_load_n(&s_state, __ATOMIC_ACQUIRE) &
		      ST_PROBED)) {
			// time first: tick reads it only once ST_PROBED is set
			s_probeMs = nowMs;
			update([](uint32_t s) { return s | ST_PROBED; });
		}
		break;
	}
}

void hostTick(uint32_t nowMs)
{
	// the probe verdict is applied only if no strong event got in between
	update([&](uint32_t s) {
		if ((s & ST_PROBED) && !(s & ST_STRONG) &&
		    (uint32_t)(nowMs - s_probeMs) >= HOST_PROBE_PC_MS)
			return ST_PACK(HOST_PC, HOSTWHY_PROBE) | ST_FRESH;
		return s;
	});
}

void hostSetSetting(uint8_t setting)
{
	if (setting <= HOSTSET_MAX)
		s_setting = setting;
}

uint8_t hostSetting()
{
	return s_setting;
}

static uint32_t state()
{
	return __atomic_load_n(&s_state, __ATOMIC_ACQUIRE);
}

uint8_t hostKind()
{
	if (s_setting == HOSTSET_PC)
		return HOST_PC;
	if (s_setting == HOSTSET_CONSOLE)
		return HOST_CONSOLE;
	return (uint8_t)ST_KIND(state());
}

uint8_t hostWhy()
{
	return s_setting == HOSTSET_AUTO ? (uint8_t)ST_WHY(state()) :
					   HOSTWHY_MANUAL;
}

uint8_t hostDetectedKind()
{
	return (uint8_t)ST_KIND(state());
}

bool hostDetectedFresh()
{
	return state() & ST_FRESH;
}
