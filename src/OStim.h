#pragma once

namespace WFNG::OStim
{
	struct Participant
	{
		RE::FormID   formID{ 0 };
		float        excitement{ 0.0f };  // 0-100
		bool         female{ false };
		bool         schlong{ false };
		std::int32_t climaxes{ 0 };
	};

	enum class Event
	{
		kStarted,
		kEnded,
		kChanged
	};

	bool                     Init();  // kDataLoaded
	bool                     IsAvailable();
	bool                     IsThreadValid(std::uint32_t a_thread);
	std::uint32_t            PlayerThread();
	std::vector<Participant> Participants(std::uint32_t a_thread);
}
