// Host test for OpenPuck/host_detect.cpp (pure logic). Build + run: sh tests/binds/run.sh
#include "../../OpenPuck/host_detect.h"
#include <stdio.h>

static int g_fail = 0, g_n = 0;
#define CHECK(c)                                                               \
	do {                                                                   \
		g_n++;                                                         \
		if (!(c)) {                                                    \
			g_fail++;                                              \
			printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c);    \
		}                                                              \
	} while (0)

static void testStartsUnknown()
{
	hostDetectInit(HOSTSET_AUTO, HOST_UNKNOWN);
	CHECK(hostKind() == HOST_UNKNOWN && hostWhy() == HOSTWHY_NONE);
	CHECK(!hostDetectedFresh());
	hostTick(100000);
	CHECK(hostKind() == HOST_UNKNOWN);
}

static void testWindowsAndBrowser()
{
	hostDetectInit(HOSTSET_AUTO, HOST_UNKNOWN);
	hostEvent(HE_MSOS2, 10);
	CHECK(hostKind() == HOST_PC && hostWhy() == HOSTWHY_MSOS2);
	CHECK(hostDetectedFresh());
	hostDetectInit(HOSTSET_AUTO, HOST_UNKNOWN);
	hostEvent(HE_WEBUSB, 10);
	CHECK(hostKind() == HOST_PC && hostWhy() == HOSTWHY_WEBUSB);
}

static void testConsoleAuth()
{
	hostDetectInit(HOSTSET_AUTO, HOST_UNKNOWN);
	hostEvent(HE_PROBE, 0);
	hostEvent(HE_AUTH, 300);
	CHECK(hostKind() == HOST_CONSOLE && hostWhy() == HOSTWHY_AUTH);
	// the probe timer must not turn a console into a PC afterwards
	hostTick(60000);
	CHECK(hostKind() == HOST_CONSOLE);
}

static void testProbeTimer()
{
	hostDetectInit(HOSTSET_AUTO, HOST_UNKNOWN);
	hostEvent(HE_PROBE, 1000);
	hostEvent(HE_PROBE, 2000); // only the first one starts the clock
	hostTick(1000 + HOST_PROBE_PC_MS - 1);
	CHECK(hostKind() == HOST_UNKNOWN);
	hostTick(1000 + HOST_PROBE_PC_MS);
	CHECK(hostKind() == HOST_PC && hostWhy() == HOSTWHY_PROBE);
	CHECK(hostDetectedFresh());
	// the timer is wrap-safe
	hostDetectInit(HOSTSET_AUTO, HOST_UNKNOWN);
	hostEvent(HE_PROBE, 0xFFFFF000u);
	hostTick(0xFFFFF000u + HOST_PROBE_PC_MS);
	CHECK(hostKind() == HOST_PC);
}

static void testStrongBeatsWeakLatestStrongWins()
{
	hostDetectInit(HOSTSET_AUTO, HOST_UNKNOWN);
	hostEvent(HE_PROBE, 0);
	hostTick(HOST_PROBE_PC_MS);
	CHECK(hostKind() == HOST_PC && hostWhy() == HOSTWHY_PROBE);
	hostEvent(HE_AUTH, HOST_PROBE_PC_MS + 10);
	CHECK(hostKind() == HOST_CONSOLE);
	hostEvent(HE_MSOS2, HOST_PROBE_PC_MS + 20); // cable moved to a PC
	CHECK(hostKind() == HOST_PC && hostWhy() == HOSTWHY_MSOS2);
}

static void testRemembered()
{
	hostDetectInit(HOSTSET_AUTO, HOST_PC);
	CHECK(hostKind() == HOST_PC && hostWhy() == HOSTWHY_REMEMBERED);
	CHECK(!hostDetectedFresh()); // nothing to store: it is not news
	hostEvent(HE_AUTH, 50);
	CHECK(hostKind() == HOST_CONSOLE && hostDetectedFresh());
	// a remembered console is turned into a PC by a weak probe too
	hostDetectInit(HOSTSET_AUTO, HOST_CONSOLE);
	CHECK(hostKind() == HOST_CONSOLE);
	hostEvent(HE_PROBE, 0);
	hostTick(HOST_PROBE_PC_MS);
	CHECK(hostKind() == HOST_PC);
	// junk values are ignored
	hostDetectInit(9, 7);
	CHECK(hostSetting() == HOSTSET_AUTO && hostKind() == HOST_UNKNOWN);
}

static void testManual()
{
	hostDetectInit(HOSTSET_CONSOLE, HOST_PC);
	CHECK(hostKind() == HOST_CONSOLE && hostWhy() == HOSTWHY_MANUAL);
	hostEvent(HE_MSOS2, 0); // detection keeps running underneath ...
	CHECK(hostKind() == HOST_CONSOLE);
	CHECK(hostDetectedKind() == HOST_PC);
	hostSetSetting(HOSTSET_PC);
	CHECK(hostKind() == HOST_PC && hostWhy() == HOSTWHY_MANUAL);
	hostSetSetting(HOSTSET_AUTO); // ... so auto picks it up again
	CHECK(hostKind() == HOST_PC && hostWhy() == HOSTWHY_MSOS2);
	hostSetSetting(77);
	CHECK(hostSetting() == HOSTSET_AUTO);
}

int main()
{
	testStartsUnknown();
	testWindowsAndBrowser();
	testConsoleAuth();
	testProbeTimer();
	testStrongBeatsWeakLatestStrongWins();
	testRemembered();
	testManual();
	printf("%d checks, %d failed\n", g_n, g_fail);
	return g_fail ? 1 : 0;
}
