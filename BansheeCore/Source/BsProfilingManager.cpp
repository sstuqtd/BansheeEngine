//__________________________ Banshee Project - A modern game development toolkit _________________________________//
//_____________________________________ www.banshee-project.com __________________________________________________//
//________________________ Copyright (c) 2014 Marko Pintera. All rights reserved. ________________________________//
#include "BsProfilingManager.h"
#include "BsMath.h"

namespace BansheeEngine
{
	const UINT32 ProfilingManager::NUM_SAVED_FRAMES = 200;

	ProfilingManager::ProfilingManager()
		:mSavedSimReports(nullptr), mNextSimReportIdx(0),
		mSavedCoreReports(nullptr), mNextCoreReportIdx(0)
	{
		mSavedSimReports = bs_newN<ProfilerReport, ProfilerAlloc>(NUM_SAVED_FRAMES);
		mSavedCoreReports = bs_newN<ProfilerReport, ProfilerAlloc>(NUM_SAVED_FRAMES);
	}

	ProfilingManager::~ProfilingManager()
	{
		if(mSavedSimReports != nullptr)
			bs_deleteN<ProfilerAlloc>(mSavedSimReports, NUM_SAVED_FRAMES);

		if(mSavedCoreReports != nullptr)
			bs_deleteN<ProfilerAlloc>(mSavedCoreReports, NUM_SAVED_FRAMES);
	}

	void ProfilingManager::_update()
	{
#if BS_PROFILING_ENABLED
		mSavedSimReports[mNextSimReportIdx].cpuReport = gProfilerCPU().generateReport();

		gProfilerCPU().reset();

		mNextSimReportIdx = (mNextSimReportIdx + 1) % NUM_SAVED_FRAMES;
#endif
	}

	void ProfilingManager::_updateCore()
	{
#if BS_PROFILING_ENABLED
		BS_LOCK_MUTEX(mSync);
		mSavedCoreReports[mNextCoreReportIdx].cpuReport = gProfilerCPU().generateReport();

		gProfilerCPU().reset();

		mNextCoreReportIdx = (mNextCoreReportIdx + 1) % NUM_SAVED_FRAMES;
#endif
	}

	const ProfilerReport& ProfilingManager::getReport(ProfiledThread thread, UINT32 idx) const
	{
		idx = Math::clamp(idx, 0U, (UINT32)(NUM_SAVED_FRAMES - 1));

		if(thread == ProfiledThread::Core)
		{
			BS_LOCK_MUTEX(mSync);

			UINT32 reportIdx = mNextCoreReportIdx + (UINT32)((INT32)NUM_SAVED_FRAMES - ((INT32)idx + 1));
			reportIdx = (reportIdx) % NUM_SAVED_FRAMES;

			return mSavedCoreReports[reportIdx];
		}
		else
		{
			UINT32 reportIdx = mNextSimReportIdx + (UINT32)((INT32)NUM_SAVED_FRAMES - ((INT32)idx + 1));
			reportIdx = (reportIdx) % NUM_SAVED_FRAMES;

			return mSavedSimReports[reportIdx];
		}
	}

	ProfilingManager& gProfiler()
	{
		return ProfilingManager::instance();
	}
}