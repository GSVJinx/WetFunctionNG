#pragma once

namespace WFNG::Arousal
{
	void         Init();
	bool         IsAvailable();
	std::int32_t Get(RE::Actor* a_actor);  // 0-100, -1 when no arousal framework
}
