#include "host_detect.h"

static volatile uint8_t s_setting = HOSTSET_AUTO;
static volatile uint8_t s_kind = HOST_UNKNOWN;
static volatile uint8_t s_why = HOSTWHY_NONE;
// set by a strong event of this session, never by the remembered value or a weak guess
static volatile bool s_strong = false;
static volatile bool s_fresh = false;
static volatile bool s_probed = false;
static volatile uint32_t s_probeMs;

void hostDetectInit(uint8_t setting, uint8_t remembered)
{
	s_setting = setting <= HOSTSET_MAX ? setting : HOSTSET_AUTO;
	s_kind = HOST_UNKNOWN;
	s_why = HOSTWHY_NONE;
	if (remembered == HOST_PC || remembered == HOST_CONSOLE) {
		s_kind = remembered;
		s_why = HOSTWHY_REMEMBERED;
	}
	s_strong = s_fresh = s_probed = false;
}

static void strong(uint8_t kind, uint8_t why)
{
	s_kind = kind;
	s_why = why;
	s_strong = true;
	s_fresh = true;
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
		if (!s_probed) {
			s_probeMs = nowMs;
			s_probed = true;
		}
		break;
	}
}

void hostTick(uint32_t nowMs)
{
	if (s_probed && !s_strong &&
	    (uint32_t)(nowMs - s_probeMs) >= HOST_PROBE_PC_MS) {
		s_kind = HOST_PC;
		s_why = HOSTWHY_PROBE;
		s_fresh = true;
		s_probed = false;
	}
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

uint8_t hostKind()
{
	if (s_setting == HOSTSET_PC)
		return HOST_PC;
	if (s_setting == HOSTSET_CONSOLE)
		return HOST_CONSOLE;
	return s_kind;
}

uint8_t hostWhy()
{
	return s_setting == HOSTSET_AUTO ? s_why : HOSTWHY_MANUAL;
}

uint8_t hostDetectedKind()
{
	return s_kind;
}

bool hostDetectedFresh()
{
	return s_fresh;
}
